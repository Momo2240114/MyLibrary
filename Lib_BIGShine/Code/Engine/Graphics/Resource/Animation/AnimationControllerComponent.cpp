// 実行時処理 / Blackboard / State / Transition
// AnimationControllerComponent の実装を役割ごとに分けたファイルです。
// クラス自体は分離せず、同じクラスのメンバ関数定義だけを別 cpp に置いています。
// そのため既存の参照関係や public/private の扱いはそのままです。

#include "AnimationControllerComponent.h"
#include "AnimSequenceAdapter.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "AnimationDataAsset.h"
#include "PoseLibraryResourceManager.h"
#include "IKMapComponent.h"

#include <imgui.h>
#include <imgui_stdlib.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <cmath>
#include <unordered_map>

namespace ed = ax::NodeEditor;

namespace
{
    // グラフ上でだけ使う内部ノード名。保存データにも入るので文字列は変更しない。
    constexpr const char* START_NODE_NAME = "__Start__";
    constexpr const char* ANY_NODE_NAME = "__Any__";
    constexpr const char* ENTRY_NODE_PREFIX = "__Entry__:";

    // サブグラフに入った時、最初に実行する入口ノード名を作る。
    std::string MakeEntryNodeName(const std::string& scopeId_)
    {
        return std::string(ENTRY_NODE_PREFIX) + scopeId_;
    }
}

void AnimationControllerComponent::SetTrigger(const std::string& name_)
{
    GetEditableBlackboard()[name_] = true;
}

void AnimationControllerComponent::ResetTrigger(const std::string& name_)
{
    if (GetEditableBlackboard().count(name_))
    {
        GetEditableBlackboard()[name_] = false;
    }
}

const BlackboardCategory& AnimationControllerComponent::GetBlackboard() const
{
    return GetCurrentBlackboard();
}

void AnimationControllerComponent::SetParam(const std::string& name_, BlackBoardValue value_)
{
    GetEditableBlackboard()[name_] = value_;
}

BlackboardCategory& AnimationControllerComponent::GetEditableBlackboard()
{
    static BlackboardCategory fallbackBlackboard_;

    if (!blackboardComponent)
    {
        auto owner_ = GetGameObject();
        if (!owner_) return fallbackBlackboard_;

        auto component_ = owner_->GetComponent<BlackboardComponent>();
        if (!component_)
        {
            component_ = owner_->AddComponent<BlackboardComponent>();
        }

        blackboardComponent = component_.get();
    }

    return blackboardComponent->GetOrCreateCategory(animationBlackboardCategory);
}

const BlackboardCategory& AnimationControllerComponent::GetCurrentBlackboard() const
{
    static const BlackboardCategory emptyBlackboard_;
    if (!blackboardComponent) return emptyBlackboard_;

    return blackboardComponent->GetCategory(animationBlackboardCategory);
}

void AnimationControllerComponent::EnsureNodeEditorContext()
{
    if (nodeEditorContext) return;

    nodeEditorConfig.SettingsFile = "Data/Editor/AnimationEditor.json";
    nodeEditorConfig.DragButtonIndex = 0;
    nodeEditorConfig.SelectButtonIndex = 0;
    nodeEditorConfig.NavigateButtonIndex = 2;
    nodeEditorConfig.ContextMenuButtonIndex = 1;
    nodeEditorContext = ed::CreateEditor(&nodeEditorConfig);
}

void AnimationControllerComponent::EnsureStatesFromModel()
{
    if (!modelComp || !GetGameObject()) return;

    const auto& animations_ = modelComp->GetAnimations();
    if (animations_.empty()) return;

    bool addedState_ = false;
    for (int animIndex_ = 0; animIndex_ < static_cast<int>(animations_.size()); ++animIndex_)
    {
        const std::string& animName_ = animations_[animIndex_].name;
        if (animName_.empty()) continue;

        auto stateIt_ = states.find(animName_);
        if (stateIt_ == states.end())
        {
            auto state_ = std::make_unique<AnimState>();
            state_->name = animName_;
            state_->playbackSpeed = 1.0f;
            state_->isLoop = true;
            state_->animationIndex = animIndex_;
            states[animName_] = std::move(state_);
            RegisterStateAction(animName_);
            EnsureGraphNodeForState(animName_);
            addedState_ = true;
        }
        else if (stateIt_->second)
        {
            stateIt_->second->animationIndex = animIndex_;
        }
    }

    if (!m_poseLibrary && !modelComp->GetModelName().empty())
    {
        m_poseLibrary = PoseLibraryResourceManager::Instance().GetOrCreateResource(modelComp->GetModelName());
    }

    if (m_poseLibrary)
    {
        for (int customAnimationIndex_ = 0; customAnimationIndex_ < static_cast<int>(m_poseLibrary->customAnims.size()); ++customAnimationIndex_)
        {
            const CustomAnimation& customAnimation_ = m_poseLibrary->customAnims[customAnimationIndex_];
            auto stateIt_ = states.find(customAnimation_.name);
            if (stateIt_ == states.end() || !stateIt_->second ||
                stateIt_->second->type != AnimStateType::Custom ||
                stateIt_->second->customAnimIndex != customAnimationIndex_)
            {
                AddOrUpdateCustomState(customAnimationIndex_);
            }
        }
    }

    if (!currentState && !states.empty())
    {
        ChangeState(animations_[0].name, 0.0f);
    }
    if (addedState_)
    {
        EnsureSpecialGraphNodes();
    }
}
AnimationControllerComponent::~AnimationControllerComponent()
{
    if (nodeEditorContext)
    {
        ed::DestroyEditor(nodeEditorContext);
        nodeEditorContext = nullptr;
    }
}

void AnimationControllerComponent::Start()
{
    auto owner_ = GetGameObject();
    auto blackboard_ = owner_->GetComponent<BlackboardComponent>();
    if (!blackboard_)
    {
        blackboard_ = owner_->AddComponent<BlackboardComponent>();
    }
    blackboardComponent = blackboard_.get();

    auto MyObject = owner_.get();
    modelComp = MyObject->GetComponent<GltfModelCommponennt>().get();

    EnsureNodeEditorContext();
    if (!modelComp) return;

    std::string modelName = modelComp->GetModelName();
    if (!modelName.empty()) {
        m_poseLibrary = PoseLibraryResourceManager::Instance().GetOrCreateResource(modelName);
    }

    const auto& animations = modelComp->GetAnimations();
    std::string objName = MyObject->GetName();

    for (int i = 0; i < (int)animations.size(); ++i)
    {
        const std::string& animName = animations[i].name;

        auto it = states.find(animName);

        if (it == states.end()) {
            auto newState = std::make_unique<AnimState>();
            newState->name = animName;
            newState->playbackSpeed = 1.0f;
            newState->isLoop = true;
            states[animName] = std::move(newState);
            it = states.find(animName);
        }

        auto& state = it->second;
        state->animationIndex = i;

        std::string fullActionName = objName + "_" + animName;
        ActionRegistry::Instance().RegisterAction("Animation", fullActionName, [this, animName]() {
            this->ChangeState(animName);
            });

        registeredFullNames.push_back(fullActionName);
    }

    EnsureSpecialGraphNodes();

    EnsureNodeEditorContext();

    if (currentState) {
        animator.CrossFadeTo(currentState, 0.0f);
    }
    else {
        TryEnterStartState();
        if (!currentState && !states.empty() && !animations.empty()) {
            ChangeState(animations[0].name, 0.0f);
        }
    }


}
void AnimationControllerComponent::Update(float deltaTime)
{
    EnsureNodeEditorContext();
    if (!modelComp)
    {
        auto owner_ = GetGameObject();
        modelComp = owner_ ? owner_->GetComponent<GltfModelCommponennt>().get() : nullptr;
        if (!modelComp)
        {
            return;
        }
    }

    EnsureStatesFromModel();

    if (!curvecomp)
    {
        auto owner_ = GetGameObject();
        curvecomp = owner_ ? owner_->GetComponent<CurveComponent>().get() : nullptr;
    }

    if (m_pendingGraphAssetLoad)
    {
        m_pendingGraphAssetLoad = false;

        if (!m_pendingGraphAssetPath.empty())
        {
            LoadGraphAsset(m_pendingGraphAssetPath);
            TryEnterStartState();
        }
    }

    if (m_editorPoseOverride)
    {
        modelComp->UpdateTransform();
        return;
    }

    if (usedTransitionGraph)
    {
        // 状態遷移の評価
        EvaluateGraphTransitions();
    }

    //ModelAnimator::Updateを呼ぶ際、ポーズライブラリのリソースポインタを第三引数に授ける
    animator.Update(deltaTime, modelComp, curvecomp, m_poseLibrary, GetGameObject());

    const AnimState* currentState = animator.GetCurrentState();
    if (currentState && currentState->rootMotion.enable)
    {
        auto delta = animator.GetRootMotionDelta();
        auto transform = GetGameObject()->transform;

        if (transform)
        {
            using namespace DirectX;
            using namespace DirectX::SimpleMath;

            // localRotation は degree 前提
            float xRad = XMConvertToRadians(transform->localRotation.x);
            float yRad = XMConvertToRadians(transform->localRotation.y);
            float zRad = XMConvertToRadians(transform->localRotation.z);

            // YXZ 運用なら yaw(Y), pitch(X), roll(Z) の順で作る
            Quaternion currentRot = Quaternion::CreateFromYawPitchRoll(yRad, xRad, zRad);
            currentRot.Normalize();

            // 位置 RootMotion
            Vector3 worldDeltaPos = Vector3::Transform(delta.deltaPosition, currentRot);
            transform->localPosition.x += worldDeltaPos.x;
            transform->localPosition.y += worldDeltaPos.y;
            transform->localPosition.z += worldDeltaPos.z;
            transform->AddMoveOffset(worldDeltaPos);

            transform->MarkDirty();
        }
    }

    // 計算された姿勢をモデルに書き戻し
    modelComp->SetNodePoses(animator.GetCurrentPose());
    // 行列の最終更新
    modelComp->UpdateTransform();
}


void AnimationControllerComponent::ChangeState(const std::string& stateName, float duration)
{
    auto it = states.find(stateName);
    if (it == states.end()) return;

    const AnimState* nextState = it->second.get();
    if (currentState == nextState) return;

    if (auto owner_ = GetGameObject())
    {
        if (auto ik_ = owner_->GetComponent<IKMapComponent>())
        {
            ik_->RequestSimulationWarmup();
        }
    }

    // Animatorクラスに遷移を命令
    animator.CrossFadeTo(nextState, duration);
    currentState = nextState;
}

void AnimationControllerComponent::AddState(const std::string& stateName, int animIndex)
{
    if (states.count(stateName)) return; // 重複チェック

    auto newState = std::make_unique<AnimState>();
    newState->name = stateName;
    newState->animationIndex = animIndex;
    newState->playbackSpeed = 1.0f;
    newState->isLoop = true;
    states[stateName] = std::move(newState);

    // アクションも登録
    std::string objName = GetGameObject()->GetName();
    std::string fullActionName = objName + "_" + stateName;
    ActionRegistry::Instance().RegisterAction("Animation", fullActionName, [this, stateName]() {
        this->ChangeState(stateName);
        });

    // グラフ用ノードデータの作成
    AnimGraphNode node;
    node.id = ed::NodeId(GetNextID());
    node.stateName = stateName;
    node.inputPin = ed::PinId(GetNextID());
    node.outputPin = ed::PinId(GetNextID());
    node.pos = ImVec2(0, 0); // 初期座標
    node.type = AnimGraphNodeType::State;

    graphNodes.push_back(node);
}

bool AnimationControllerComponent::AddOrUpdateCustomState(int customAnimIndex_)
{
    if (!m_poseLibrary || customAnimIndex_ < 0 || customAnimIndex_ >= static_cast<int>(m_poseLibrary->customAnims.size())) return false;

    const CustomAnimation& customAnimation_ = m_poseLibrary->customAnims[customAnimIndex_];
    if (customAnimation_.name.empty()) return false;

    auto stateIt_ = states.find(customAnimation_.name);
    if (stateIt_ == states.end())
    {
        auto state_ = std::make_unique<AnimState>();
        state_->name = customAnimation_.name;
        state_->playbackSpeed = 1.0f;
        state_->isLoop = true;
        stateIt_ = states.emplace(customAnimation_.name, std::move(state_)).first;
    }

    AnimState* state_ = stateIt_->second.get();
    if (!state_) return false;

    state_->type = AnimStateType::Custom;
    state_->customAnimIndex = customAnimIndex_;
    state_->animationIndex = customAnimation_.baseAnimationIndex >= 0 ? customAnimation_.baseAnimationIndex : 0;
    editingState = state_;

    RegisterStateAction(customAnimation_.name);
    EnsureGraphNodeForState(customAnimation_.name);
    return true;
}

bool AnimationControllerComponent::PreviewCustomAnimation(int customAnimIndex_, float time_)
{
    if (!AddOrUpdateCustomState(customAnimIndex_)) return false;

    if (!modelComp)
    {
        auto owner_ = GetGameObject();
        modelComp = owner_ ? owner_->GetComponent<GltfModelCommponennt>().get() : nullptr;
    }
    if (!modelComp) return false;

    if (!curvecomp)
    {
        auto owner_ = GetGameObject();
        curvecomp = owner_ ? owner_->GetComponent<CurveComponent>().get() : nullptr;
    }

    const CustomAnimation& customAnimation_ = m_poseLibrary->customAnims[customAnimIndex_];
    ChangeState(customAnimation_.name, 0.0f);
    animator.SetCurrentTime(time_);
    animator.Update(0.0f, modelComp, curvecomp, m_poseLibrary, GetGameObject());
    modelComp->SetNodePoses(animator.GetCurrentPose());
    modelComp->UpdateTransform();
    m_editorPoseOverride = true;
    return true;
}

bool AnimationControllerComponent::SelectStateForEditing(const std::string& stateName_)
{
    auto stateIt_ = states.find(stateName_);
    if (stateIt_ == states.end() || !stateIt_->second) return false;

    editingState = stateIt_->second.get();
    return true;
}
void AnimationControllerComponent::RemoveState(const std::string& stateName)
{
    auto it = states.find(stateName);
    if (it == states.end()) return;

    // --- 安全策: 消そうとしているステートが現在再生中なら、他のステートへ逃がす ---
    if (currentState == it->second.get())
    {
        currentState = nullptr;
        // 他にステートがあれば最初のやつに、なければ停止
        if (states.size() > 1) {
            for (auto& [name, ptr] : states) {
                if (name != stateName) {
                    ChangeState(name, 0.0f);
                    break;
                }
            }
        }
    }

    // AddState で登録した時と同じ形式の名前を構築
    std::string objName = GetGameObject()->GetName();
    std::string fullActionName = objName + "_" + stateName;
    ActionRegistry::Instance().RemoveAction("Animation", fullActionName);

    // 削除実行
    states.erase(it);
}


void AnimationControllerComponent::EnsureSpecialGraphNodes()
{
    auto ensureNode = [&](AnimGraphNodeType type, const char* internalName, const ImVec2& defaultPos)
    {
        auto it = std::find_if(graphNodes.begin(), graphNodes.end(),
            [&](const AnimGraphNode& node) { return node.type == type || node.stateName == internalName; });

        if (it == graphNodes.end())
        {
            AnimGraphNode node;
            node.id = ed::NodeId(GetNextID());
            node.stateName = internalName;
            node.inputPin = ed::PinId(0);
            node.outputPin = ed::PinId(GetNextID());
            node.pos = defaultPos;
            node.type = type;
            node.scopeId = "Root";
            graphNodes.push_back(node);
            return;
        }

        it->stateName = internalName;
        it->type = type;
        it->scopeId = "Root";
        if ((uintptr_t)it->id.Get() == 0)
            it->id = ed::NodeId(GetNextID());
        it->inputPin = ed::PinId(0);
        if ((uintptr_t)it->outputPin.Get() == 0)
            it->outputPin = ed::PinId(GetNextID());
    };

    ensureNode(AnimGraphNodeType::Start, START_NODE_NAME, ImVec2(-360.0f, -80.0f));
    ensureNode(AnimGraphNodeType::Any, ANY_NODE_NAME, ImVec2(-360.0f, 140.0f));
}

void AnimationControllerComponent::TryEnterStartState()
{
    for (auto& transition : graphTransitions)
    {
        if (transition.fromStateName != START_NODE_NAME)
            continue;

        auto* toNode = FindNodeByName(transition.toStateName);
        if (toNode && toNode->type == AnimGraphNodeType::SubGraph)
        {
            if (TryEnterSubGraph(*toNode, 0.0f))
                return;
        }

        if (states.count(transition.toStateName) == 0)
            continue;

        ChangeState(transition.toStateName, 0.0f);
        return;
    }
}

bool AnimationControllerComponent::TryEnterSubGraph(const AnimGraphNode& subGraphNode, float duration)
{
    if (subGraphNode.type != AnimGraphNodeType::SubGraph || subGraphNode.childScopeId.empty())
        return false;

    const std::string entryName = MakeEntryNodeName(subGraphNode.childScopeId);
    for (auto& transition : graphTransitions)
    {
        if (transition.scopeId != subGraphNode.childScopeId)
            continue;
        if (transition.fromStateName != entryName)
            continue;
        if (states.count(transition.toStateName) == 0)
            continue;

        ChangeState(transition.toStateName, duration);
        return true;
    }

    return false;
}
AnimGraphNode* AnimationControllerComponent::AFindNodeByPin(ed::PinId pinId)
{
    for (auto& node : graphNodes) 
    {
        // ノードが持つ入力ピンまたは出力ピンのIDが、探しているIDと一致するかチェック
        if (node.inputPin == pinId || node.outputPin == pinId) {
            return &node;
        }
    }
    return nullptr; // 見つからなかった場合
}

AnimGraphNode* AnimationControllerComponent::FindNodeByName(const std::string& name)
{
    for (auto& node : graphNodes)
    {
        if (node.stateName == name)
        {
            return &node;
        }
    }
    return nullptr;
}


void AnimationControllerComponent::SetIDCounterFloor(uintptr_t minVal)
{
    if (m_NextID <= minVal)
        m_NextID = minVal + 1;
}
void AnimationControllerComponent::EvaluateGraphTransitions()
{
    const AnimState* activeState = animator.GetCurrentState();
    if (!activeState)
    {
        TryEnterStartState();
        activeState = animator.GetCurrentState();
    }
    if (!activeState) return;

    _runtimeActiveStateName = activeState->name;

    auto areConditionsMet = [&](AnimTransition& transition) -> bool
        {
            bool isMet = transition.conditions.empty();
            if (!transition.conditions.empty())
            {
                isMet = (transition.logic == TransitionLogic::And);
                for (auto& cond : transition.conditions)
                {
                    const bool conditionMet = EvaluateCondition(cond);
                    if (transition.logic == TransitionLogic::And && !conditionMet)
                    {
                        isMet = false;
                        break;
                    }
                    if (transition.logic == TransitionLogic::Or && conditionMet)
                    {
                        isMet = true;
                        break;
                    }
                }
            }
            return isMet;
        };

    auto resetTriggerConditions = [&](AnimTransition& transition)
        {
            for (auto& cond : transition.conditions)
            {
                if (cond.type == ConditionType::Trigger)
                {
                    auto it = GetEditableBlackboard().find(cond.parameterName);
                    if (it != GetEditableBlackboard().end())
                    {
                        it->second = false;
                    }
                }
            }
        };

    auto enterTransitionTarget = [&](AnimTransition& transition) -> bool
        {
            auto* toNode = FindNodeByName(transition.toStateName);
            if (toNode && toNode->type == AnimGraphNodeType::SubGraph)
            {
                if (TryEnterSubGraph(*toNode, transition.duration))
                {
                    return true;
                }
                return false;
            }

            if (states.count(transition.toStateName) == 0)
                return false;

            ChangeState(transition.toStateName, transition.duration);
            return true;
        };

    auto exitSubGraph = [&](AnimTransition& exitTransition) -> bool
        {
            AnimGraphNode* ownerSubGraph = nullptr;
            for (auto& node : graphNodes)
            {
                if (node.type == AnimGraphNodeType::SubGraph && node.childScopeId == exitTransition.scopeId)
                {
                    ownerSubGraph = &node;
                    break;
                }
            }
            if (!ownerSubGraph)
                return false;

            for (auto& parentTransition : graphTransitions)
            {
                if (parentTransition.fromStateName != ownerSubGraph->stateName)
                    continue;
                if (!areConditionsMet(parentTransition))
                    continue;

                resetTriggerConditions(parentTransition);
                return enterTransitionTarget(parentTransition);
            }

            return false;
        };

    for (auto& transition : graphTransitions)
    {
        const bool fromCurrent = (transition.fromStateName == activeState->name);
        const bool fromAny = (transition.fromStateName == ANY_NODE_NAME);

        if (!fromCurrent && !fromAny)
            continue;
        if (transition.toStateName == activeState->name)
            continue;

        auto* toNode = FindNodeByName(transition.toStateName);
        const bool toExit = toNode && toNode->type == AnimGraphNodeType::Exit;
        const bool toSubGraph = toNode && toNode->type == AnimGraphNodeType::SubGraph;
        if (!toExit && !toSubGraph && states.count(transition.toStateName) == 0)
            continue;

        if (!areConditionsMet(transition))
            continue;

        resetTriggerConditions(transition);

        if (toExit)
        {
            if (exitSubGraph(transition))
                break;
            continue;
        }

        if (enterTransitionTarget(transition))
            break;
    }
}
bool AnimationControllerComponent::EvaluateCondition(const AnimCondition& cond)
{
    // 1. アニメーション終了判定
    if (cond.type == ConditionType::AnimationEnd) {
        const auto* state = animator.GetCurrentState();
        if (!state) return false;

        float duration = modelComp->GetAnimationDuration(
            state->type == AnimStateType::Single ? state->animationIndex : state->animIndexA
        );
        return animator.GetCurrentFadeTime() >= duration;
    }

    // 2. 変数・トリガー判定
    if (GetEditableBlackboard().find(cond.parameterName) == GetEditableBlackboard().end()) return false;
    const auto& bbValue = GetEditableBlackboard().at(cond.parameterName);

    auto ToFloat = [](const BlackBoardValue& v) -> float {
        return std::visit([](auto&& arg) -> float {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, float>) return arg;
            if constexpr (std::is_same_v<T, int>)   return (float)arg;
            if constexpr (std::is_same_v<T, bool>)  return arg ? 1.0f : 0.0f;
            if constexpr (std::is_same_v<T, DirectX::SimpleMath::Vector2>) return arg.Length();
            return 0.0f;
            }, v);
        };

    float currentVal = ToFloat(bbValue);

    if (cond.type == ConditionType::Trigger) return currentVal >= 0.5f;

    switch (cond.op) {
    case ConditionOp::Greater:  return currentVal > cond.threshold;
    case ConditionOp::Less:     return currentVal < cond.threshold;
    case ConditionOp::Equal:    return std::abs(currentVal - cond.threshold) < 0.001f;
    case ConditionOp::NotEqual: return std::abs(currentVal - cond.threshold) >= 0.001f;
    }
    return false;
}

const std::string GRAPH_ASSET_DIR = "Data/Animation/GraphData/";
const std::string STATE_ASSET_DIR = "Data/Animation/StateData/";


void AnimationControllerComponent::RegisterStateAction(const std::string& stateName_)
{
    if (stateName_.empty() || !GetGameObject()) return;

    std::string objectName_ = GetGameObject()->GetName();
    std::string fullActionName_ = objectName_ + "_" + stateName_;
    ActionRegistry::Instance().RegisterAction("Animation", fullActionName_, [this, stateName_]() {
        this->ChangeState(stateName_);
        });
}

void AnimationControllerComponent::EnsureGraphNodeForState(const std::string& stateName_)
{
    if (stateName_.empty()) return;

    auto exists_ = std::any_of(graphNodes.begin(), graphNodes.end(), [&](const AnimGraphNode& node_) {
        return node_.stateName == stateName_;
        });
    if (exists_) return;

    AnimGraphNode node_;
    node_.id = ed::NodeId(GetNextID());
    node_.stateName = stateName_;
    node_.inputPin = ed::PinId(GetNextID());
    node_.outputPin = ed::PinId(GetNextID());
    node_.pos = ImVec2(0.0f, 0.0f);
    node_.type = AnimGraphNodeType::State;
    node_.scopeId = m_currentGraphScopeId.empty() ? "Root" : m_currentGraphScopeId;
    graphNodes.push_back(node_);
}


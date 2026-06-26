// 実行時処理 / Blackboard / State / Transition
// AnimationControllerComponent の実装を役割ごとに分けたファイルです。
// クラス自体は分離せず、同じクラスのメンバ関数定義だけを別 cpp に置いています。
// そのため既存の参照関係や public/private の扱いはそのままです。

#include "AnimationControllerComponent.h"
#include "AnimSequenceAdapter.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Component/CharacterMovementComponent.h"
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

#include "Engine/Debug/TracyProfiler.h"


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

    // Node positions belong to each graph asset. A shared settings file mixes
    // layouts from different graphs/scopes and can restore invalid positions.
    nodeEditorConfig.SettingsFile = nullptr;
    nodeEditorConfig.DragButtonIndex = 0;
    nodeEditorConfig.SelectButtonIndex = 0;
    nodeEditorConfig.NavigateButtonIndex = 2;
    nodeEditorConfig.ContextMenuButtonIndex = 1;
    nodeEditorContext = ed::CreateEditor(&nodeEditorConfig);
}

void AnimationControllerComponent::EnsureStatesFromModel()
{
    if (!modelComponent || !GetGameObject()) return;

    const auto& animations_ = modelComponent->GetAnimations();
    if (animations_.empty()) return;

    bool addedState_ = false;
    for (int animIndex_ = 0; animIndex_ < static_cast<int>(animations_.size()); ++animIndex_)
    {
        const std::string& animName_ = animations_[animIndex_].name;
        if (animName_.empty()) continue;

        auto stateIt_ = states.find(animName_);
        if (stateIt_ == states.end())
        {
            const bool animationAlreadyUsed_ = std::any_of(
                states.begin(),
                states.end(),
                [animIndex_](const auto& stateEntry_)
                {
                    return stateEntry_.second &&
                        stateEntry_.second->type != AnimStateType::Custom &&
                        stateEntry_.second->animationIndex == animIndex_;
                });
            if (animationAlreadyUsed_) continue;

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

    if (!poseLibrary && !modelComponent->GetModelName().empty())
    {
        poseLibrary = PoseLibraryResourceManager::Instance().GetOrCreateResource(modelComponent->GetModelName());
    }

    if (poseLibrary)
    {
        for (int customAnimationIndex_ = 0; customAnimationIndex_ < static_cast<int>(poseLibrary->customAnims.size()); ++customAnimationIndex_)
        {
            const CustomAnimation& customAnimation_ = poseLibrary->customAnims[customAnimationIndex_];
            auto stateIt_ = states.find(customAnimation_.name);
            const bool customAnimationAlreadyUsed_ = std::any_of(
                states.begin(),
                states.end(),
                [customAnimationIndex_](const auto& stateEntry_)
                {
                    return stateEntry_.second &&
                        stateEntry_.second->type == AnimStateType::Custom &&
                        stateEntry_.second->customAnimIndex == customAnimationIndex_;
                });
            if (stateIt_ == states.end() && customAnimationAlreadyUsed_) continue;
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
    if (!owner_) return;

    owner_->GetOrAddComponent<CharacterMovementComponent>();
    auto blackboard_ = owner_->GetComponent<BlackboardComponent>();
    if (!blackboard_)
    {
        blackboard_ = owner_->AddComponent<BlackboardComponent>();
    }
    blackboardComponent = blackboard_.get();

    auto object_ = owner_.get();
    modelComponent = object_->GetComponent<GltfModelCommponennt>().get();

    EnsureNodeEditorContext();
    if (!modelComponent) return;

    std::string modelName_ = modelComponent->GetModelName();
    if (!modelName_.empty()) {
        poseLibrary = PoseLibraryResourceManager::Instance().GetOrCreateResource(modelName_);
    }

    const auto& animations_ = modelComponent->GetAnimations();
    std::string objectName_ = object_->GetName();

    for (int index_ = 0; index_ < (int)animations_.size(); ++index_)
    {
        const std::string& animationName_ = animations_[index_].name;

        auto iterator_ = states.find(animationName_);

        if (iterator_ == states.end()) {
            auto newState_ = std::make_unique<AnimState>();
            newState_->name = animationName_;
            newState_->playbackSpeed = 1.0f;
            newState_->isLoop = true;
            states[animationName_] = std::move(newState_);
            iterator_ = states.find(animationName_);
        }

        auto& state_ = iterator_->second;
        state_->animationIndex = index_;

        std::string fullActionName_ = objectName_ + "_" + animationName_;
        ActionRegistry::Instance().RegisterAction("Animation", fullActionName_, [this, animationName_]() {
            this->ChangeState(animationName_);
            });

        registeredActionNames.push_back(fullActionName_);
    }

    EnsureSpecialGraphNodes();

    EnsureNodeEditorContext();

    if (currentState) {
        animator.CrossFadeTo(currentState, 0.0f, GetGameObject());
    }
    else {
        TryEnterStartState();
        if (!currentState && !states.empty() && !animations_.empty()) {
            ChangeState(animations_[0].name, 0.0f);
        }
    }


}
void AnimationControllerComponent::Update(float deltaTime_)
{
    BS_PROFILE_SCOPE("AnimationControllerComponent::Update");

    {
        BS_PROFILE_SCOPE("Animation::EnsureNodeEditorContext");
        EnsureNodeEditorContext();
    }

    if (!modelComponent)
    {
        BS_PROFILE_SCOPE("Animation::ResolveModelComponent");

        auto owner_ = GetGameObject();
        modelComponent = owner_ ? owner_->GetComponent<GltfModelCommponennt>().get() : nullptr;
        if (!modelComponent)
        {
            return;
        }
    }

    {
        BS_PROFILE_SCOPE("Animation::EnsureStatesFromModel");
        EnsureStatesFromModel();
    }

    if (!curveComponent)
    {
        BS_PROFILE_SCOPE("Animation::ResolveCurveComponent");

        auto owner_ = GetGameObject();
        curveComponent = owner_ ? owner_->GetComponent<CurveComponent>().get() : nullptr;
    }

    if (pendingGraphAssetLoad)
    {
        BS_PROFILE_SCOPE("Animation::PendingGraphAssetLoad");

        pendingGraphAssetLoad = false;

        if (!pendingGraphAssetPath.empty())
        {
            {
                BS_PROFILE_SCOPE("Animation::LoadGraphAsset");
                LoadGraphAsset(pendingGraphAssetPath);
            }

            {
                BS_PROFILE_SCOPE("Animation::ApplyPendingStateAssets");
                ApplyPendingStateAssets();
            }

            {
                BS_PROFILE_SCOPE("Animation::TryEnterStartState");
                TryEnterStartState();
            }
        }
    }

    if (editorPoseOverride)
    {
        BS_PROFILE_SCOPE("Animation::EditorPoseOverrideUpdateTransform");
        modelComponent->UpdateTransform();
        return;
    }

    if (useTransitionGraph && !animationPaused)
    {
        BS_PROFILE_SCOPE("Animation::EvaluateGraphTransitions");
        EvaluateGraphTransitions();
    }

    const BlackboardCategory& animationBlackboard_ = GetCurrentBlackboard();
    const float animationDeltaTime_ = animationPaused ? 0.0f : deltaTime_;

    {
        BS_PROFILE_SCOPE("ModelAnimator::Update");
        animator.Update(
            animationDeltaTime_,
            modelComponent,
            curveComponent,
            poseLibrary,
            GetGameObject(),
            &animationBlackboard_);
    }

    {
        BS_PROFILE_SCOPE("Animation::RootMotion");

        const AnimState* activeState_ = animator.GetCurrentState();
        if (activeState_ && activeState_->rootMotion.enable)
        {
            auto rootMotionDelta_ = animator.GetRootMotionDelta();
            auto transform_ = GetGameObject()->transform;

            if (transform_)
            {
                using namespace DirectX;
                using namespace DirectX::SimpleMath;

                float xRadians_ = XMConvertToRadians(transform_->localRotation.x);
                float yRadians_ = XMConvertToRadians(transform_->localRotation.y);
                float zRadians_ = XMConvertToRadians(transform_->localRotation.z);

                Quaternion currentRotation_ = Quaternion::CreateFromYawPitchRoll(
                    yRadians_,
                    xRadians_,
                    zRadians_);

                currentRotation_.Normalize();

                Vector3 worldDeltaPosition_ =
                    Vector3::Transform(rootMotionDelta_.deltaPosition, currentRotation_);

                if (!activeState_->rootMotion.applyPositionY)
                {
                    worldDeltaPosition_.y = 0.0f;
                }

                const std::shared_ptr<GameObject> owner_ = GetGameObject();
                const std::shared_ptr<CharacterMovementComponent> movement_ = owner_
                    ? owner_->GetComponent<CharacterMovementComponent>()
                    : nullptr;

                if (movement_)
                {
                    rootMotionDelta = worldDeltaPosition_;
                    movement_->AddRootMotionDelta(worldDeltaPosition_);
                }
                else
                {
                    transform_->AddMoveOffset(worldDeltaPosition_);
                    transform_->MarkDirty();
                }
            }
        }
    }

    {
        BS_PROFILE_SCOPE("GltfModel::SetNodePoses");
        modelComponent->SetNodePoses(animator.GetCurrentPose());
        modelComponent->MarkPoseDirty();
    }

    {
        BS_PROFILE_SCOPE("GltfModel::UpdateTransform");
        modelComponent->UpdateTransformIfDirty();
    }
}

void AnimationControllerComponent::ChangeState(const std::string& stateName_, float duration_)
{
    auto iterator_ = states.find(stateName_);
    if (iterator_ == states.end()) return;

    const AnimState* nextState_ = iterator_->second.get();
    if (currentState == nextState_) return;

    if (auto owner_ = GetGameObject())
    {
        if (auto ik_ = owner_->GetComponent<IKMapComponent>())
        {
            ik_->RequestSimulationWarmup();
        }
    }

    // Animatorクラスに遷移を命令
    animator.CrossFadeTo(nextState_, duration_, GetGameObject());
    currentState = nextState_;
}

void AnimationControllerComponent::AddState(const std::string& stateName_, int animationIndex_)
{
    if (states.count(stateName_)) return; // 重複チェック

    auto newState_ = std::make_unique<AnimState>();
    newState_->name = stateName_;
    newState_->animationIndex = animationIndex_;
    newState_->playbackSpeed = 1.0f;
    newState_->isLoop = true;
    states[stateName_] = std::move(newState_);

    // アクションも登録
    std::string objectName_ = GetGameObject()->GetName();
    std::string fullActionName_ = objectName_ + "_" + stateName_;
    ActionRegistry::Instance().RegisterAction("Animation", fullActionName_, [this, stateName_]() {
        this->ChangeState(stateName_);
        });

    // グラフ用ノードデータの作成
    AnimGraphNode node_;
    node_.id = ed::NodeId(GetNextId());
    node_.stateName = stateName_;
    node_.inputPin = ed::PinId(GetNextId());
    node_.outputPin = ed::PinId(GetNextId());
    node_.pos = ImVec2(0, 0); // 初期座標
    node_.type = AnimGraphNodeType::State;

    graphNodes.push_back(node_);
}

bool AnimationControllerComponent::AddOrUpdateCustomState(int customAnimIndex_)
{
    if (!poseLibrary || customAnimIndex_ < 0 || customAnimIndex_ >= static_cast<int>(poseLibrary->customAnims.size())) return false;

    const CustomAnimation& customAnimation_ = poseLibrary->customAnims[customAnimIndex_];
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

    if (!modelComponent)
    {
        auto owner_ = GetGameObject();
        modelComponent = owner_ ? owner_->GetComponent<GltfModelCommponennt>().get() : nullptr;
    }
    if (!modelComponent) return false;

    if (!curveComponent)
    {
        auto owner_ = GetGameObject();
        curveComponent = owner_ ? owner_->GetComponent<CurveComponent>().get() : nullptr;
    }

    const CustomAnimation& customAnimation_ = poseLibrary->customAnims[customAnimIndex_];
    ChangeState(customAnimation_.name, 0.0f);
    animator.SetCurrentTime(time_);
    const BlackboardCategory& animationBlackboard_ = GetCurrentBlackboard();
    animator.Update(0.0f, modelComponent, curveComponent, poseLibrary, GetGameObject(), &animationBlackboard_);
    modelComponent->SetNodePoses(animator.GetCurrentPose());
    modelComponent->UpdateTransform();
    editorPoseOverride = true;
    return true;
}

bool AnimationControllerComponent::SelectStateForEditing(const std::string& stateName_)
{
    auto stateIt_ = states.find(stateName_);
    if (stateIt_ == states.end() || !stateIt_->second) return false;

    editingState = stateIt_->second.get();
    return true;
}

bool AnimationControllerComponent::RenameState(
    const std::string& oldName_,
    const std::string& newName_)
{
    if (oldName_.empty() || newName_.empty()) return false;
    if (oldName_ == newName_) return true;

    auto stateIterator_ = states.find(oldName_);
    if (stateIterator_ == states.end() || !stateIterator_->second) return false;
    if (states.find(newName_) != states.end()) return false;

    const bool graphNameUsed_ = std::any_of(
        graphNodes.begin(),
        graphNodes.end(),
        [&](const AnimGraphNode& node_)
        {
            return node_.stateName == newName_;
        });
    if (graphNameUsed_) return false;

    std::unique_ptr<AnimState> renamedState_ = std::move(stateIterator_->second);
    AnimState* renamedStatePointer_ = renamedState_.get();
    states.erase(stateIterator_);
    renamedState_->name = newName_;
    states.emplace(newName_, std::move(renamedState_));

    for (AnimGraphNode& node_ : graphNodes)
    {
        if (node_.type == AnimGraphNodeType::State && node_.stateName == oldName_)
        {
            node_.stateName = newName_;
        }
    }
    for (AnimTransition& transition_ : graphTransitions)
    {
        if (transition_.fromStateName == oldName_) transition_.fromStateName = newName_;
        if (transition_.toStateName == oldName_) transition_.toStateName = newName_;
    }

    const auto assetPathIterator_ = stateAssetPaths.find(oldName_);
    if (assetPathIterator_ != stateAssetPaths.end())
    {
        const std::string assetPath_ = assetPathIterator_->second;
        stateAssetPaths.erase(assetPathIterator_);
        stateAssetPaths[newName_] = assetPath_;
    }
    const auto pendingPathIterator_ = pendingStateAssetPaths.find(oldName_);
    if (pendingPathIterator_ != pendingStateAssetPaths.end())
    {
        const std::string pendingPath_ = pendingPathIterator_->second;
        pendingStateAssetPaths.erase(pendingPathIterator_);
        pendingStateAssetPaths[newName_] = pendingPath_;
    }

    if (runtimeActiveStateName == oldName_) runtimeActiveStateName = newName_;
    if (editingState == renamedStatePointer_) editingState = renamedStatePointer_;

    if (const std::shared_ptr<GameObject> owner_ = GetGameObject())
    {
        const std::string oldActionName_ = owner_->GetName() + "_" + oldName_;
        ActionRegistry::Instance().RemoveAction("Animation", oldActionName_);
    }
    RegisterStateAction(newName_);
    return true;
}

void AnimationControllerComponent::RemoveState(const std::string& stateName_)
{
    auto iterator_ = states.find(stateName_);
    if (iterator_ == states.end()) return;

    // --- 安全策: 消そうとしているステートが現在再生中なら、他のステートへ逃がす ---
    if (currentState == iterator_->second.get())
    {
        currentState = nullptr;
        // 他にステートがあれば最初のやつに、なければ停止
        if (states.size() > 1) {
            for (auto& [name, statePointer_] : states) {
                if (name != stateName_) {
                    ChangeState(name, 0.0f);
                    break;
                }
            }
        }
    }

    // AddState で登録した時と同じ形式の名前を構築
    std::string objectName_ = GetGameObject()->GetName();
    std::string fullActionName_ = objectName_ + "_" + stateName_;
    ActionRegistry::Instance().RemoveAction("Animation", fullActionName_);

    // 削除実行
    states.erase(iterator_);
    stateAssetPaths.erase(stateName_);
}


void AnimationControllerComponent::EnsureSpecialGraphNodes()
{
    auto ensureNode_ = [&](AnimGraphNodeType nodeType_, const char* internalName_, const ImVec2& defaultPosition_)
    {
        auto iterator_ = std::find_if(graphNodes.begin(), graphNodes.end(),
            [&](const AnimGraphNode& node_) { return node_.type == nodeType_ || node_.stateName == internalName_; });

        if (iterator_ == graphNodes.end())
        {
            AnimGraphNode node_;
            node_.id = ed::NodeId(GetNextId());
            node_.stateName = internalName_;
            node_.inputPin = ed::PinId(0);
            node_.outputPin = ed::PinId(GetNextId());
            node_.pos = defaultPosition_;
            node_.type = nodeType_;
            node_.scopeId = "Root";
            graphNodes.push_back(node_);
            return;
        }

        iterator_->stateName = internalName_;
        iterator_->type = nodeType_;
        iterator_->scopeId = "Root";
        if ((uintptr_t)iterator_->id.Get() == 0)
            iterator_->id = ed::NodeId(GetNextId());
        iterator_->inputPin = ed::PinId(0);
        if ((uintptr_t)iterator_->outputPin.Get() == 0)
            iterator_->outputPin = ed::PinId(GetNextId());
    };

    ensureNode_(AnimGraphNodeType::Start, START_NODE_NAME, ImVec2(-360.0f, -80.0f));
    ensureNode_(AnimGraphNodeType::Any, ANY_NODE_NAME, ImVec2(-360.0f, 140.0f));
}

void AnimationControllerComponent::TryEnterStartState()
{
    for (auto& transition_ : graphTransitions)
    {
        if (transition_.fromStateName != START_NODE_NAME)
            continue;

        auto* toNode_ = FindNodeByName(transition_.toStateName);
        if (toNode_ && toNode_->type == AnimGraphNodeType::SubGraph)
        {
            if (TryEnterSubGraph(*toNode_, 0.0f))
                return;
        }

        if (states.count(transition_.toStateName) == 0)
            continue;

        ChangeState(transition_.toStateName, 0.0f);
        return;
    }
}

bool AnimationControllerComponent::TryEnterSubGraph(const AnimGraphNode& subGraphNode_, float duration_)
{
    if (subGraphNode_.type != AnimGraphNodeType::SubGraph || subGraphNode_.childScopeId.empty())
        return false;

    const std::string entryName_ = MakeEntryNodeName(subGraphNode_.childScopeId);
    for (auto& transition_ : graphTransitions)
    {
        if (transition_.scopeId != subGraphNode_.childScopeId)
            continue;
        if (transition_.fromStateName != entryName_)
            continue;
        if (states.count(transition_.toStateName) == 0)
            continue;

        bool conditionsMet_ = transition_.conditions.empty();
        if (!transition_.conditions.empty())
        {
            conditionsMet_ = transition_.logic == TransitionLogic::And;
            for (const AnimCondition& condition_ : transition_.conditions)
            {
                const bool conditionMet_ = EvaluateCondition(condition_);
                if (transition_.logic == TransitionLogic::And && !conditionMet_)
                {
                    conditionsMet_ = false;
                    break;
                }
                if (transition_.logic == TransitionLogic::Or && conditionMet_)
                {
                    conditionsMet_ = true;
                    break;
                }
            }
        }
        if (!conditionsMet_)
            continue;

        for (const AnimCondition& condition_ : transition_.conditions)
        {
            if (condition_.type != ConditionType::Trigger)
                continue;

            auto iterator_ = GetEditableBlackboard().find(condition_.parameterName);
            if (iterator_ != GetEditableBlackboard().end())
            {
                iterator_->second = false;
            }
        }

        ChangeState(transition_.toStateName, duration_);
        return true;
    }

    return false;
}
AnimGraphNode* AnimationControllerComponent::FindNodeByPin(ed::PinId pinId_)
{
    for (auto& node_ : graphNodes)
    {
        // ノードが持つ入力ピンまたは出力ピンのIDが、探しているIDと一致するかチェック
        if (node_.inputPin == pinId_ || node_.outputPin == pinId_) {
            return &node_;
        }
    }
    return nullptr; // 見つからなかった場合
}

AnimGraphNode* AnimationControllerComponent::FindNodeByName(const std::string& name_)
{
    for (auto& node_ : graphNodes)
    {
        if (node_.stateName == name_)
        {
            return &node_;
        }
    }
    return nullptr;
}


void AnimationControllerComponent::SetIdCounterFloor(uintptr_t minValue_)
{
    if (nextGraphElementId <= minValue_)
        nextGraphElementId = minValue_ + 1;
}
void AnimationControllerComponent::EvaluateGraphTransitions()
{
    const AnimState* activeState_ = animator.GetCurrentState();
    if (!activeState_)
    {
        TryEnterStartState();
        activeState_ = animator.GetCurrentState();
    }
    if (!activeState_) return;

    runtimeActiveStateName = activeState_->name;

    auto areConditionsMet_ = [&](AnimTransition& transition_) -> bool
        {
            bool conditionsMet_ = transition_.conditions.empty();
            if (!transition_.conditions.empty())
            {
                conditionsMet_ = (transition_.logic == TransitionLogic::And);
                for (auto& condition_ : transition_.conditions)
                {
                    const bool conditionMet_ = EvaluateCondition(condition_);
                    if (transition_.logic == TransitionLogic::And && !conditionMet_)
                    {
                        conditionsMet_ = false;
                        break;
                    }
                    if (transition_.logic == TransitionLogic::Or && conditionMet_)
                    {
                        conditionsMet_ = true;
                        break;
                    }
                }
            }
            return conditionsMet_;
        };

    auto resetTriggerConditions_ = [&](AnimTransition& transition_)
        {
            for (auto& condition_ : transition_.conditions)
            {
                if (condition_.type == ConditionType::Trigger)
                {
                    auto iterator_ = GetEditableBlackboard().find(condition_.parameterName);
                    if (iterator_ != GetEditableBlackboard().end())
                    {
                        iterator_->second = false;
                    }
                }
            }
        };

    auto enterTransitionTarget_ = [&](AnimTransition& transition_) -> bool
        {
            auto* toNode_ = FindNodeByName(transition_.toStateName);
            if (toNode_ && toNode_->type == AnimGraphNodeType::SubGraph)
            {
                if (TryEnterSubGraph(*toNode_, transition_.duration))
                {
                    return true;
                }
                return false;
            }

            if (states.count(transition_.toStateName) == 0)
                return false;

            ChangeState(transition_.toStateName, transition_.duration);
            return true;
        };

    auto exitSubGraph_ = [&](AnimTransition& exitTransition_) -> bool
        {
            AnimGraphNode* ownerSubGraph_ = nullptr;
            for (auto& node_ : graphNodes)
            {
                if (node_.type == AnimGraphNodeType::SubGraph && node_.childScopeId == exitTransition_.scopeId)
                {
                    ownerSubGraph_ = &node_;
                    break;
                }
            }
            if (!ownerSubGraph_)
                return false;

            for (auto& parentTransition_ : graphTransitions)
            {
                if (parentTransition_.fromStateName != ownerSubGraph_->stateName)
                    continue;
                if (!areConditionsMet_(parentTransition_))
                    continue;

                resetTriggerConditions_(parentTransition_);
                return enterTransitionTarget_(parentTransition_);
            }

            return false;
        };

    for (auto& transition_ : graphTransitions)
    {
        const bool fromCurrent_ = (transition_.fromStateName == activeState_->name);
        const bool fromAny_ = (transition_.fromStateName == ANY_NODE_NAME);

        if (!fromCurrent_ && !fromAny_)
            continue;
        if (transition_.toStateName == activeState_->name)
            continue;

        auto* toNode_ = FindNodeByName(transition_.toStateName);
        const bool toExit_ = toNode_ && toNode_->type == AnimGraphNodeType::Exit;
        const bool toSubGraph_ = toNode_ && toNode_->type == AnimGraphNodeType::SubGraph;
        if (!toExit_ && !toSubGraph_ && states.count(transition_.toStateName) == 0)
            continue;

        if (!areConditionsMet_(transition_))
            continue;

        resetTriggerConditions_(transition_);

        if (toExit_)
        {
            if (exitSubGraph_(transition_))
                break;
            continue;
        }

        if (enterTransitionTarget_(transition_))
            break;
    }
}
bool AnimationControllerComponent::EvaluateCondition(const AnimCondition& condition_)
{
    // 1. アニメーション終了判定
    if (condition_.type == ConditionType::AnimationEnd) {
        const auto* state_ = animator.GetCurrentState();
        if (!state_) return false;

        float animationDuration_ = animator.GetCurrentSampleDuration();
        if (animationDuration_ <= 0.0001f && state_->type == AnimStateType::Single)
        {
            animationDuration_ = modelComponent->GetAnimationDuration(state_->animationIndex);
        }
        return animator.GetCurrentFadeTime() >= animationDuration_;
    }

    // 2. 変数・トリガー判定
    if (GetEditableBlackboard().find(condition_.parameterName) == GetEditableBlackboard().end()) return false;
    const auto& blackboardValue_ = GetEditableBlackboard().at(condition_.parameterName);

    auto toFloat_ = [](const BlackBoardValue& value_) -> float {
        return std::visit([](auto&& value_) -> float {
            using ValueType = std::decay_t<decltype(value_)>;
            if constexpr (std::is_same_v<ValueType, float>) return value_;
            if constexpr (std::is_same_v<ValueType, int>)   return (float)value_;
            if constexpr (std::is_same_v<ValueType, bool>)  return value_ ? 1.0f : 0.0f;
            if constexpr (std::is_same_v<ValueType, DirectX::SimpleMath::Vector2>) return value_.Length();
            return 0.0f;
            }, value_);
        };

    float currentValue_ = toFloat_(blackboardValue_);

    if (condition_.type == ConditionType::Trigger) return currentValue_ >= 0.5f;

    switch (condition_.op) {
    case ConditionOp::Greater:  return currentValue_ > condition_.threshold;
    case ConditionOp::Less:     return currentValue_ < condition_.threshold;
    case ConditionOp::Equal:    return std::abs(currentValue_ - condition_.threshold) < 0.001f;
    case ConditionOp::NotEqual: return std::abs(currentValue_ - condition_.threshold) >= 0.001f;
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
    node_.id = ed::NodeId(GetNextId());
    node_.stateName = stateName_;
    node_.inputPin = ed::PinId(GetNextId());
    node_.outputPin = ed::PinId(GetNextId());
    node_.pos = ImVec2(0.0f, 0.0f);
    node_.type = AnimGraphNodeType::State;
    node_.scopeId = currentGraphScopeId.empty() ? "Root" : currentGraphScopeId;
    graphNodes.push_back(node_);
}


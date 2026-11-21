#include "PrecompiledHeader.h"
#include "AnimationGraphBridgeComponent.h"

// 実行時処理 / Blackboard / State / Transition
// AnimationControllerComponent の実装を役割ごとに分けたファイルです。
// クラス自体は分離せず、同じクラスのメンバ関数定義だけを別 cpp に置いています。
// そのため既存の参照関係や public/private の扱いはそのままです。

#include "AnimationControllerComponent.h"
#include "AnimSequenceAdapter.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Component/CharacterMovementComponent.h"
#include "Engine/GamePlay/Core/Component/MotionWarpComponent.h"
#include "Engine/Common/DebugLog.h"
#include "AnimationDataAsset.h"
#include "PoseLibraryResourceManager.h"
#include "IKMapComponent.h"

#include <imgui.h>
#include <imgui_stdlib.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <cmath>
#include <cstdio>
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
    MarkBlackboardGuiCacheDirty();
}

void AnimationControllerComponent::ResetTrigger(const std::string& name_)
{
    if (GetEditableBlackboard().count(name_))
    {
        GetEditableBlackboard()[name_] = false;
        MarkBlackboardGuiCacheDirty();
    }
}

const BlackboardCategory& AnimationControllerComponent::GetBlackboard() const
{
    return GetCurrentBlackboard();
}

void AnimationControllerComponent::SetParam(const std::string& name_, BlackBoardValue value_)
{
    GetEditableBlackboard()[name_] = value_;
    MarkBlackboardGuiCacheDirty();
}

void AnimationControllerComponent::SetRootMotionYawLimit(float yawDegrees_)
{
    rootMotionYawLimitRemaining = yawDegrees_;
    rootMotionYawLimitActive = true;
}

void AnimationControllerComponent::ClearRootMotionYawLimit()
{
    rootMotionYawLimitRemaining = 0.0f;
    rootMotionYawLimitActive = false;
}

float AnimationControllerComponent::ConsumeRootMotionYawLimit(float yawDegrees_)
{
    if (!rootMotionYawLimitActive)
    {
        return yawDegrees_;
    }

    constexpr float MINIMUM_YAW_DEGREES = 0.0001f;
    if (std::abs(yawDegrees_) <= MINIMUM_YAW_DEGREES)
    {
        return 0.0f;
    }

    if (std::abs(rootMotionYawLimitRemaining) <= MINIMUM_YAW_DEGREES ||
        yawDegrees_ * rootMotionYawLimitRemaining <= 0.0f)
    {
        rootMotionYawLimitRemaining = 0.0f;
        return 0.0f;
    }

    // 90度ターンなどのRootMotionを、AIが要求した角度までで止める。
    const float appliedYawDegrees_ =
        std::copysign(
            (std::min)(std::abs(yawDegrees_), std::abs(rootMotionYawLimitRemaining)),
            rootMotionYawLimitRemaining);
    rootMotionYawLimitRemaining -= appliedYawDegrees_;

    if (std::abs(rootMotionYawLimitRemaining) <= MINIMUM_YAW_DEGREES)
    {
        rootMotionYawLimitRemaining = 0.0f;
    }

    return appliedYawDegrees_;
}

BlackboardCategory& AnimationControllerComponent::GetEditableBlackboard()
{
    static BlackboardCategory fallbackBlackboard_;

    if (!blackboardComponent)
    {
        auto owner_ = GetGameObject();
        if (!owner_)
        {
            return fallbackBlackboard_;
        }

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
    if (!blackboardComponent)
    {
        return emptyBlackboard_;
    }

    return blackboardComponent->GetCategory(animationBlackboardCategory);
}

void AnimationControllerComponent::EnsureNodeEditorContext()
{
    if (nodeEditorContext)
    {
        return;
    }

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
    if (!modelComponent)
    {
        return;
    }
    modelComponent->UpdateLoadState();
    if (!modelComponent->IsModelReady())
    {
        return;
    }

    const auto& animations_ = modelComponent->GetAnimations();
    for (int animIndex_ = 0; animIndex_ < static_cast<int>(animations_.size()); ++animIndex_)
    {
        const std::string& animName_ = animations_[animIndex_].name;
        if (animName_.empty())
        {
            continue;
        }

        auto stateIt_ = states.find(animName_);
        if (stateIt_ != states.end() && stateIt_->second)
        {
            stateIt_->second->animationIndex = animIndex_;
        }
    }

    if (!poseLibrary && !modelComponent->GetModelName().empty())
    {
        poseLibrary = PoseLibraryResourceManager::Instance().GetOrCreateResource(modelComponent->GetModelName());
    }

    for (const auto& [stateName_, state_] : states)
    {
        if (state_)
        {
            RegisterStateAction(stateName_);
        }
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
    if (!owner_)
    {
        return;
    }

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
    if (!modelComponent)
    {
        return;
    }
    modelComponent->UpdateLoadState();
    if (!modelComponent->IsModelReady())
    {
        return;
    }

    modelComponent->SetTransformUpdateDrivenExternally(true);

    std::string modelName_ = modelComponent->GetModelName();
    if (!modelName_.empty()) {
        poseLibrary = PoseLibraryResourceManager::Instance().GetOrCreateResource(modelName_);
    }

    EnsureStatesFromModel();

    EnsureSpecialGraphNodes();

    EnsureNodeEditorContext();

    if (currentState) {
        animator.CrossFadeTo(currentState, 0.0f, GetGameObject());
    }
    else {
        TryEnterStartState();
        if (!currentState && !states.empty()) {
            ChangeState(states.begin()->first, 0.0f);
        }
    }


}

void AnimationControllerComponent::PrepareGraphEditor()
{
    // 停止中も、ゲームのStartやUpdateを通さずに編集対象を読み込む。
    LoadPendingGraphAsset();
    EnsureNodeEditorContext();
}

void AnimationControllerComponent::LoadPendingGraphAsset()
{
    if (!pendingGraphAssetLoad)
    {
        return;
    }
    BS_PROFILE_SCOPE("Animation::PendingGraphAssetLoad");

    pendingGraphAssetLoad = false;

    if (!pendingGraphAssetPath.empty())
    {
        // 開幕Actionで選んだStateは、遅延Graph読み込みによるState再構築後にも引き継ぐ。
        const auto presentationState_ = (presentationControlled || skillControlled) && currentState
            ? std::make_unique<AnimState>(*currentState) : nullptr;
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
            if (presentationState_)
            {
                AddState(presentationState_->name, presentationState_->animationIndex);
                *states.at(presentationState_->name) = *presentationState_;
                ChangeState(presentationState_->name, 0.0f);
            }
            else
            {
                TryEnterStartState();
            }
        }
    }
}

void AnimationControllerComponent::Update(float deltaTime_)
{
    BS_PROFILE_SCOPE("AnimationControllerComponent::Update");
    BS_PROFILE_PLOT("Animation.ActiveControllerCount", 1);

#if BS_PROFILE_DETAILED_ENABLED
    {
        const std::shared_ptr<GameObject> owner_ = GetGameObject();
        const AnimState* activeState_ = animator.GetCurrentState();
        char profileText_[256]{};
        const int textLength_ = std::snprintf(
            profileText_,
            sizeof(profileText_),
            "Object: %s\nState: %s\nBlend: %s\nGraphNodeCount: %zu\nStateCount: %zu",
            owner_ ? owner_->GetName().c_str() : "None",
            activeState_ ? activeState_->name.c_str() : "None",
            activeState_ && activeState_->type != AnimStateType::Single ? "true" : "false",
            graphNodes.size(),
            states.size());
        if (textLength_ > 0)
        {
            BS_PROFILE_TEXT(
                profileText_,
                static_cast<size_t>((std::min)(textLength_, static_cast<int>(sizeof(profileText_) - 1))));
        }
    }
#endif

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
        modelComponent->UpdateLoadState();
        if (!modelComponent->IsModelReady())
        {
            return;
        }

        modelComponent->SetTransformUpdateDrivenExternally(true);
    }
    else
    {
        modelComponent->UpdateLoadState();
        if (!modelComponent->IsModelReady())
        {
            return;
        }
    }

    EnsureStatesFromModel();

    if (!curveComponent)
    {
        BS_PROFILE_SCOPE("Animation::ResolveCurveComponent");

        auto owner_ = GetGameObject();
        curveComponent = owner_ ? owner_->GetComponent<CurveComponent>().get() : nullptr;
    }

    LoadPendingGraphAsset();

    ProcessBlackboardGraphRequests();

    if (editorPoseOverride)
    {
        BS_PROFILE_SCOPE("Animation::EditorPoseOverrideUpdateTransform");
        modelComponent->UpdateTransform();
        return;
    }

    if (useTransitionGraph && !animationPaused && !presentationControlled && !skillControlled)
    {
        BS_PROFILE_SCOPE("Animation::EvaluateGraphTransitions");
        EvaluateGraphTransitions();
    }

    const BlackboardCategory& animationBlackboard_ = GetCurrentBlackboard();
    const float animationDeltaTime_ = animationPaused ? 0.0f : deltaTime_;

    {
        BS_PROFILE_SCOPE("Animation::UpdateModelAnimator");
        animator.Update(
            animationDeltaTime_,
            modelComponent,
            curveComponent,
            poseLibrary,
            GetGameObject(),
            &animationBlackboard_,
            &states);
    }

    {
        // ModelAnimatorが抽出したRootMotion差分を、実際のGameObject移動へ反映する。
        // MotionWarpComponentが存在する場合は、移動へ渡す直前に差分を補正する。
        BS_PROFILE_SCOPE("Animation::RootMotion");

        const AnimState* activeState_ = animator.GetCurrentState();
        const auto warpOwner_ = GetGameObject();
        const auto activeWarp_ = warpOwner_ ? warpOwner_->GetComponent<MotionWarpComponent>() : nullptr;
        // その場攻撃でも明示対象への追加補正は更新し、保存されたRootMotion設定は維持する。
        if (activeState_ && (activeState_->rootMotion.enable || (activeWarp_ && activeWarp_->RequiresRootMotionUpdate())))
        {
            // アニメーションのRootノードから算出された、このフレーム分の移動・回転差分を取得する。
            auto rootMotionDelta_ = animator.GetRootMotionDelta();
            if (!activeState_->rootMotion.enable)
            {
                rootMotionDelta_ = ModelAnimator::RootMotionDelta{};
            }
            auto transform_ = GetGameObject()->transform;

            if (transform_)
            {
                using namespace DirectX;
                using namespace DirectX::SimpleMath;

                float xRadians_ = XMConvertToRadians(transform_->localRotation.x);
                float yRadians_ = XMConvertToRadians(transform_->localRotation.y);
                float zRadians_ = XMConvertToRadians(transform_->localRotation.z);

                // RootMotionのローカル移動をワールド方向へ変換するため、現在のObject回転をQuaternionへ変換する。
                Quaternion currentRotation_ = Quaternion::CreateFromYawPitchRoll(
                    yRadians_,
                    xRadians_,
                    zRadians_);

                currentRotation_.Normalize();
                // アニメーション基準の移動差分を、現在のキャラクターの向きに合わせたワールド移動へ変換する。
                Vector3 worldDeltaPosition_ =
                    Vector3::Transform(rootMotionDelta_.deltaPosition, currentRotation_);
                Quaternion deltaRotation_ = rootMotionDelta_.deltaRotation;
                deltaRotation_.Normalize();
                float rootMotionYawDegrees_ = 0.0f;
                // Y回転を使用するステートだけ、RootMotionの回転差分からYaw角を取り出す。
                if (activeState_->rootMotion.applyRotationY)
                {
                    rootMotionYawDegrees_ = XMConvertToDegrees(std::atan2(
                        2.0f * (deltaRotation_.w * deltaRotation_.y + deltaRotation_.x * deltaRotation_.z),
                        1.0f - 2.0f * (deltaRotation_.y * deltaRotation_.y + deltaRotation_.x * deltaRotation_.x)));
                }

                if (!activeState_->rootMotion.applyPositionY)
                {
                    worldDeltaPosition_.y = 0.0f;
                }

                const std::shared_ptr<GameObject> owner_ = GetGameObject();
                rootMotionDelta_.deltaPosition = worldDeltaPosition_;
                // MotionWarpが有効なら、現在の再生位置を正規化してRootMotion差分を補正させる。
                if (const std::shared_ptr<MotionWarpComponent> motionWarp_ = owner_
                    ? owner_->GetComponent<MotionWarpComponent>()
                    : nullptr)
                {
                    const float duration_ = animator.GetCurrentSampleDuration();
                    const float normalizedTime_ = duration_ > 0.000001f
                        ? std::clamp(animator.GetCurrentFadeTime() / duration_, 0.0f, 1.0f)
                        : 0.0f;
                    const MotionWarpResult warpResult_ =
                        motionWarp_->ApplyRootMotionWarp(
                            rootMotionDelta_,
                            normalizedTime_,
                            deltaTime_);
                    rootMotionDelta_ = warpResult_.rootMotionDelta;
                    worldDeltaPosition_ = rootMotionDelta_.deltaPosition;
                }

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
                }
                // 微小な誤差を除外し、抽出したYaw回転だけをObjectへ加算する。
                if (std::abs(rootMotionYawDegrees_) > 0.0001f)
                {
                    rootMotionYawDegrees_ = ConsumeRootMotionYawLimit(rootMotionYawDegrees_);
                }
                if (std::abs(rootMotionYawDegrees_) > 0.0001f)
                {
                    transform_->localRotation.y += rootMotionYawDegrees_;
                    transform_->MarkDirty();
                }
            }
        }
    }

    {
        BS_PROFILE_SCOPE("GltfModel::SetNodePoses");
        modelComponent->SetNodePoses(animator.GetCurrentPose());
    }

    {
        BS_PROFILE_SCOPE("Animation::UpdateModelTransform");
        modelComponent->UpdateTransformIfDirty();
    }
    // 最新のノード姿勢とRootMotion移動を反映してから、頭部のワールド位置を参照する。
    animator.SampleWeaponHitBoxes(GetGameObject(), modelComponent);
    animator.FlushAttackTelegraphs(GetGameObject());
    PublishGraphFeedback();
}

bool AnimationControllerComponent::EstimateRootMotion(const std::string& stateName_,
    float startNormalizedTime_, float endNormalizedTime_, DirectX::XMFLOAT3& localDelta_,
    float& playbackDuration_) const
{
    BS_PROFILE_SCOPE("AnimationControllerComponent::EstimateRootMotion");
    localDelta_ = { 0.0f, 0.0f, 0.0f };
    playbackDuration_ = 0.0f;
    const auto stateIterator_ = states.find(stateName_);
    if (!modelComponent || !modelComponent->IsModelReady() || stateIterator_ == states.end() || !stateIterator_->second)
    {
        return false;
    }
    const auto& state_ = *stateIterator_->second;
    const auto& animations_ = modelComponent->GetAnimations();
    if (state_.type != AnimStateType::Single || state_.animationIndex < 0 ||
        state_.animationIndex >= static_cast<int>(animations_.size()))
    {
        return false;
    }
    const auto& clip_ = animations_[state_.animationIndex];
    const auto& nodes_ = modelComponent->GetNodes();
    const std::string rootName_ = state_.rootMotion.sourceNodeName.empty()
        ? (modelComponent->GetRootMotionName().empty() ? "root" : modelComponent->GetRootMotionName())
        : state_.rootMotion.sourceNodeName;
    const int rootIndex_ = modelComponent->GetNodeIndex(rootName_.c_str());
    if (rootIndex_ < 0 || rootIndex_ >= static_cast<int>(nodes_.size()) || clip_.secondsLength <= 0.0f)
    {
        return false;
    }
    const float start_ = std::clamp(startNormalizedTime_, 0.0f, 1.0f);
    const float end_ = std::clamp(endNormalizedTime_, start_, 1.0f);
    if (end_ <= start_)
    {
        return false;
    }
    using namespace DirectX::SimpleMath;
    // 再生中のポーズは変更せず、抽出元と親階層だけを読み取る。
    auto sampleNode_ = [&](int index_, float time_, GltfModelCommponennt::NodePose& pose_)
    {
        if (index_ < 0 || index_ >= static_cast<int>(clip_.nodeAnims.size()))
        {
            return false;
        }
        const auto& keys_ = clip_.nodeAnims[index_];
        if (keys_.positionKeyframes.empty() || keys_.rotationKeyframes.empty() || keys_.scaleKeyframes.empty())
        {
            return false;
        }
        pose_.position = keys_.positionKeyframes.front().value;
        pose_.rotation = keys_.rotationKeyframes.front().value;
        pose_.scale = keys_.scaleKeyframes.front().value;
        modelComponent->ComputeAnimation(state_.animationIndex, index_, time_, pose_);
        return true;
    };
    constexpr int SAMPLE_COUNT = 16;
    GltfModelCommponennt::NodePose previous_;
    if (!sampleNode_(rootIndex_, start_ * clip_.secondsLength, previous_))
    {
        return false;
    }
    Vector3 accumulated_ = Vector3::Zero;
    for (int index_ = 1; index_ <= SAMPLE_COUNT; ++index_)
    {
        const float normalized_ = start_ + (end_ - start_) * static_cast<float>(index_) / SAMPLE_COUNT;
        const float time_ = normalized_ * clip_.secondsLength;
        GltfModelCommponennt::NodePose pose_;
        if (!sampleNode_(rootIndex_, time_, pose_))
        {
            return false;
        }
        Vector3 delta_ = Vector3(pose_.position) - Vector3(previous_.position);
        if (!state_.rootMotion.applyPositionX)
        {
            delta_.x = 0.0f;
        }
        if (!state_.rootMotion.applyPositionY)
        {
            delta_.y = 0.0f;
        }
        if (!state_.rootMotion.applyPositionZ)
        {
            delta_.z = 0.0f;
        }
        Matrix parentMatrix_ = Matrix::Identity;
        int parentIndex_ = nodes_[rootIndex_].parentIndex;
        std::size_t depth_ = 0;
        while (parentIndex_ >= 0)
        {
            if (parentIndex_ >= static_cast<int>(nodes_.size()) || ++depth_ > nodes_.size())
            {
                return false;
            }
            GltfModelCommponennt::NodePose parentPose_;
            if (!sampleNode_(parentIndex_, time_, parentPose_))
            {
                return false;
            }
            parentMatrix_ *= Matrix::CreateScale(Vector3(parentPose_.scale)) *
                Matrix::CreateFromQuaternion(Quaternion(parentPose_.rotation)) * Matrix::CreateTranslation(Vector3(parentPose_.position));
            parentIndex_ = nodes_[parentIndex_].parentIndex;
        }
        float scale_ = state_.rootMotion.enable ? state_.rootMotionScale : 0.0f;
        if (curveComponent && !state_.rootMotionScaleCurveName.empty())
        {
            scale_ *= curveComponent->GetValue(state_.rootMotionScaleCurveName, normalized_);
        }
        accumulated_ += Vector3::TransformNormal(delta_, parentMatrix_) * scale_;
        const float midpoint_ = normalized_ - (end_ - start_) / (2.0f * SAMPLE_COUNT);
        float speed_ = state_.playbackSpeed;
        if (curveComponent && !state_.speedCurveName.empty() && curveComponent->HasTrack(state_.speedCurveName))
        {
            speed_ = curveComponent->GetValue(state_.speedCurveName, midpoint_);
        }
        for (const auto& event_ : state_.events)
        {
            if (event_.overridePlaybackSpeed && midpoint_ * clip_.secondsLength >= event_.startTime && midpoint_ * clip_.secondsLength <= event_.endTime)
            {
                speed_ = (std::max)(event_.playbackSpeed, 0.01f);
            }
        }
        if (!std::isfinite(speed_) || speed_ <= 0.0001f)
        {
            return false;
        }
        playbackDuration_ += (end_ - start_) * clip_.secondsLength / (SAMPLE_COUNT * speed_);
        previous_ = pose_;
    }
    localDelta_ = { accumulated_.x, 0.0f, accumulated_.z };
    return std::isfinite(localDelta_.x) && std::isfinite(localDelta_.z) && std::isfinite(playbackDuration_);
}

void AnimationControllerComponent::ChangeState(const std::string& stateName_, float duration_, bool restart_, float startTime_)
{
    auto iterator_ = states.find(stateName_);
    if (iterator_ == states.end())
    {
        return;
    }

    const AnimState* nextState_ = iterator_->second.get();
    if (currentState == nextState_ && !restart_)
    {
        return;
    }

    const std::string previousStateName_ = currentState ? currentState->name : "None";
    const std::string ownerName_ = GetGameObject() ? GetGameObject()->GetName() : "None";
    BS_LOG_INFO(
        LOG_CATEGORY::GAME,
        "Animation transition: " + ownerName_ + " " + previousStateName_ + " -> " + nextState_->name);

    if (auto owner_ = GetGameObject())
    {
        if (auto ik_ = owner_->GetComponent<IKMapComponent>())
        {
            ik_->RequestSimulationWarmup();
        }
    }

    // Animatorクラスに遷移を命令
    animator.CrossFadeTo(nextState_, duration_, GetGameObject(), restart_, startTime_);
    currentState = nextState_;
    PublishGraphFeedback();
}

void AnimationControllerComponent::AddState(const std::string& stateName_, int animationIndex_)
{
    if (states.count(stateName_))
    {
        return; // 重複チェック
    }

    auto newState_ = std::make_unique<AnimState>();
    newState_->name = stateName_;
    newState_->animationIndex = animationIndex_;
    newState_->playbackSpeed = 1.0f;
    newState_->isLoop = true;
    states[stateName_] = std::move(newState_);

    // weak参照とBlackboard経由の再生を、既存のState登録処理へ統一する。
    RegisterStateAction(stateName_);

    // グラフ用ノードデータの作成
    AnimGraphNode node_;
    node_.id = ed::NodeId(GetNextId());
    node_.stateName = stateName_;
    node_.inputPin = ed::PinId(GetNextId());
    node_.outputPin = ed::PinId(GetNextId());
    node_.pos = ImVec2(0, 0); // 初期座標
    node_.type = AnimGraphNodeType::State;

    graphNodes.push_back(node_);
    MarkStateGuiCacheDirty();
    MarkActionCategoryCacheDirty();
    MarkSequenceAdapterDirty();
}

bool AnimationControllerComponent::AddOrUpdateCustomState(int customAnimIndex_)
{
    if (!poseLibrary || customAnimIndex_ < 0 || customAnimIndex_ >= static_cast<int>(poseLibrary->customAnims.size()))
    {
        return false;
    }

    const CustomAnimation& customAnimation_ = poseLibrary->customAnims[customAnimIndex_];
    if (customAnimation_.name.empty())
    {
        return false;
    }

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
    if (!state_)
    {
        return false;
    }

    state_->type = AnimStateType::Custom;
    state_->customAnimIndex = customAnimIndex_;
    state_->animationIndex = customAnimation_.baseAnimationIndex >= 0 ? customAnimation_.baseAnimationIndex : 0;
    editingState = state_;

    RegisterStateAction(customAnimation_.name);
    EnsureGraphNodeForState(customAnimation_.name);
    MarkStateGuiCacheDirty();
    MarkActionCategoryCacheDirty();
    MarkSequenceAdapterDirty();
    MarkAnimationMaskCacheDirty();
    return true;
}

bool AnimationControllerComponent::PreviewCustomAnimation(int customAnimIndex_, float time_)
{
    if (!AddOrUpdateCustomState(customAnimIndex_))
    {
        return false;
    }

    if (!modelComponent)
    {
        auto owner_ = GetGameObject();
        modelComponent = owner_ ? owner_->GetComponent<GltfModelCommponennt>().get() : nullptr;
    }
    if (!modelComponent)
    {
        return false;
    }
    modelComponent->UpdateLoadState();
    if (!modelComponent->IsModelReady())
    {
        return false;
    }

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
    if (stateIt_ == states.end() || !stateIt_->second)
    {
        return false;
    }

    editingState = stateIt_->second.get();
    MarkSequenceAdapterDirty();
    MarkAnimationMaskCacheDirty();
    return true;
}

bool AnimationControllerComponent::RenameState(
    const std::string& oldName_,
    const std::string& newName_)
{
    if (oldName_.empty() || newName_.empty())
    {
        return false;
    }
    if (oldName_ == newName_)
    {
        return true;
    }

    auto stateIterator_ = states.find(oldName_);
    if (stateIterator_ == states.end() || !stateIterator_->second)
    {
        return false;
    }
    if (states.find(newName_) != states.end())
    {
        return false;
    }

    const bool graphNameUsed_ = std::any_of(
        graphNodes.begin(),
        graphNodes.end(),
        [&](const AnimGraphNode& node_)
        {
            return node_.stateName == newName_;
        });
    if (graphNameUsed_)
    {
        return false;
    }

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
        if (transition_.fromStateName == oldName_)
        {
            transition_.fromStateName = newName_;
        }
        if (transition_.toStateName == oldName_)
        {
            transition_.toStateName = newName_;
        }
    }
    for (auto& [stateName_, statePointer_] : states)
    {
        if (!statePointer_)
        {
            continue;
        }

        // Sequenceステートが参照しているState名も、リネーム後の名前へ追従させる。
        for (AnimSequenceClip& clip_ : statePointer_->sequenceClips)
        {
            if (clip_.stateName == oldName_)
            {
                clip_.stateName = newName_;
            }
        }
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

    if (runtimeActiveStateName == oldName_)
    {
        runtimeActiveStateName = newName_;
    }
    if (editingState == renamedStatePointer_)
    {
        editingState = renamedStatePointer_;
    }

    if (const std::shared_ptr<GameObject> owner_ = GetGameObject())
    {
        const std::string oldActionName_ = owner_->GetName() + "_" + oldName_;
        ActionRegistry::Instance().RemoveAction("Animation", oldActionName_);
    }
    RegisterStateAction(newName_);
    MarkStateGuiCacheDirty();
    MarkActionCategoryCacheDirty();
    MarkSequenceAdapterDirty();
    MarkAnimationMaskCacheDirty();
    return true;
}

void AnimationControllerComponent::RemoveState(const std::string& stateName_)
{
    auto iterator_ = states.find(stateName_);
    if (iterator_ == states.end())
    {
        return;
    }

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
    for (auto& [name_, statePointer_] : states)
    {
        if (!statePointer_)
        {
            continue;
        }

        // 削除されたStateをSequenceが参照し続けないよう、直接クリップ参照へ戻す。
        for (AnimSequenceClip& clip_ : statePointer_->sequenceClips)
        {
            if (clip_.stateName == stateName_)
            {
                clip_.stateName.clear();
            }
        }
    }
    MarkStateGuiCacheDirty();
    MarkActionCategoryCacheDirty();
    MarkSequenceAdapterDirty();
    MarkAnimationMaskCacheDirty();
}


void AnimationControllerComponent::EnsureSpecialGraphNodes()
{
    auto ensureNode_ = [&](AnimGraphNodeType nodeType_, const char* internalName_, const ImVec2& defaultPosition_)
    {
        auto iterator_ = std::find_if(graphNodes.begin(), graphNodes.end(),
            [&](const AnimGraphNode& node_)
        {
            return node_.type == nodeType_ || node_.stateName == internalName_;
        });

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
        {
            iterator_->id = ed::NodeId(GetNextId());
        }
        iterator_->inputPin = ed::PinId(0);
        if ((uintptr_t)iterator_->outputPin.Get() == 0)
        {
            iterator_->outputPin = ed::PinId(GetNextId());
        }
    };

    ensureNode_(AnimGraphNodeType::Start, START_NODE_NAME, ImVec2(-360.0f, -80.0f));
    ensureNode_(AnimGraphNodeType::Any, ANY_NODE_NAME, ImVec2(-360.0f, 140.0f));
}

void AnimationControllerComponent::TryEnterStartState()
{
    for (auto& transition_ : graphTransitions)
    {
        if (transition_.fromStateName != START_NODE_NAME)
        {
            continue;
        }

        auto* toNode_ = FindNodeByName(transition_.toStateName);
        if (toNode_ && toNode_->type == AnimGraphNodeType::SubGraph)
        {
            if (TryEnterSubGraph(*toNode_, 0.0f))
            {
                return;
            }
        }

        if (states.count(transition_.toStateName) == 0)
        {
            continue;
        }

        ChangeState(transition_.toStateName, 0.0f);
        return;
    }
}

bool AnimationControllerComponent::TryEnterSubGraph(const AnimGraphNode& subGraphNode_, float duration_)
{
    if (subGraphNode_.type != AnimGraphNodeType::SubGraph || subGraphNode_.childScopeId.empty())
    {
        return false;
    }

    const std::string entryName_ = MakeEntryNodeName(subGraphNode_.childScopeId);
    for (auto& transition_ : graphTransitions)
    {
        if (transition_.scopeId != subGraphNode_.childScopeId)
        {
            continue;
        }
        if (transition_.fromStateName != entryName_)
        {
            continue;
        }
        if (states.count(transition_.toStateName) == 0)
        {
            continue;
        }

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
        {
            continue;
        }

        for (const AnimCondition& condition_ : transition_.conditions)
        {
            if (condition_.type != ConditionType::Trigger)
            {
                continue;
            }

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
    {
        nextGraphElementId = minValue_ + 1;
    }
}

void AnimationControllerComponent::EvaluateGraphTransitions()
{
    const AnimState* activeState_ = animator.GetCurrentState();
    if (!activeState_)
    {
        TryEnterStartState();
        activeState_ = animator.GetCurrentState();
    }
    if (!activeState_)
    {
        return;
    }

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
        {
            return false;
        }

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
        {
            return false;
        }

        for (auto& parentTransition_ : graphTransitions)
            {
                if (parentTransition_.fromStateName != ownerSubGraph_->stateName)
            {
                continue;
            }
            if (!areConditionsMet_(parentTransition_))
            {
                continue;
            }

            resetTriggerConditions_(parentTransition_);
                return enterTransitionTarget_(parentTransition_);
            }

            return false;
        };

    auto tryTransition_ = [&](AnimTransition& transition_) -> bool
    {
        if (transition_.toStateName == activeState_->name)
        {
            return false;
        }

        auto* toNode_ = FindNodeByName(transition_.toStateName);
        const bool toExit_ = toNode_ && toNode_->type == AnimGraphNodeType::Exit;
        const bool toSubGraph_ = toNode_ && toNode_->type == AnimGraphNodeType::SubGraph;
        if (!toExit_ && !toSubGraph_ && states.count(transition_.toStateName) == 0)
        {
            return false;
        }

        if (!areConditionsMet_(transition_))
        {
            return false;
        }

        resetTriggerConditions_(transition_);

        if (toExit_)
        {
            if (exitSubGraph_(transition_))
            {
                return true;
            }
            return false;
        }

        return enterTransitionTarget_(transition_);
    };

    for (auto& transition_ : graphTransitions)
    {
        if (transition_.blackboardRequestRoute)
        {
            continue;
        }
        if (transition_.fromStateName != activeState_->name)
        {
            continue;
        }

        if (tryTransition_(transition_))
        {
            return;
        }
    }

    for (auto& transition_ : graphTransitions)
    {
        if (transition_.blackboardRequestRoute)
        {
            continue;
        }
        if (transition_.fromStateName != ANY_NODE_NAME)
        {
            continue;
        }

        if (tryTransition_(transition_))
        {
            return;
        }
    }
}

bool AnimationControllerComponent::EvaluateCondition(const AnimCondition& condition_)
{
    // 1. アニメーション終了判定
    if (condition_.type == ConditionType::AnimationEnd) {
        const auto* state_ = animator.GetCurrentState();
        if (!state_)
        {
            return false;
        }

        float animationDuration_ = animator.GetCurrentSampleDuration();
        if (animationDuration_ <= 0.0001f && state_->type == AnimStateType::Single)
        {
            if (!modelComponent || !modelComponent->IsModelReady())
            {
                return false;
            }
            animationDuration_ = modelComponent->GetAnimationDuration(state_->animationIndex);
        }
        return animator.GetCurrentFadeTime() >= animationDuration_;
    }

    // 2. 変数・トリガー判定
    if (GetEditableBlackboard().find(condition_.parameterName) == GetEditableBlackboard().end())
    {
        return false;
    }
    const auto& blackboardValue_ = GetEditableBlackboard().at(condition_.parameterName);

    auto toFloat_ = [](const BlackBoardValue& value_) -> float {
        return std::visit([](auto&& value_) -> float {
            using ValueType = std::decay_t<decltype(value_)>;
            if constexpr (std::is_same_v<ValueType, float>)
            {
                return value_;
            }
            if constexpr (std::is_same_v<ValueType, int>)
            {
                return (float)value_;
            }
            if constexpr (std::is_same_v<ValueType, bool>)
            {
                return value_ ? 1.0f : 0.0f;
            }
            if constexpr (std::is_same_v<ValueType, DirectX::SimpleMath::Vector2>)
            {
                return value_.Length();
            }
            return 0.0f;
            }, value_);
        };

    float currentValue_ = toFloat_(blackboardValue_);

    if (condition_.type == ConditionType::Trigger)
    {
        return currentValue_ >= 0.5f;
    }

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
    if (stateName_.empty() || !GetGameObject())
    {
        return;
    }
    const auto owner_ = GetGameObject();
    const std::weak_ptr<AnimationControllerComponent> weak_ = owner_->GetComponent<AnimationControllerComponent>();
    // OwnerのSequenceはLocalRegistryから再生し、別Objectの同名Stateへ依存しない。
    owner_->RegisterAction("Animation", stateName_, [weak_, stateName_]()
    {
        if (const auto animation_ = weak_.lock())
        {
            const auto owner_ = animation_->GetGameObject();
            const auto graph_ = owner_ ? owner_->GetComponent<AnimationGraphBridgeComponent>() : nullptr;
            if (graph_)
            {
                graph_->RequestStateTransition(stateName_, 0.2f, false, 0.0f, animation_->IsSkillControlled());
            }
            else
            {
                animation_->ChangeState(stateName_);
            }
        }
    });

    std::string objectName_ = GetGameObject()->GetName();
    std::string fullActionName_ = objectName_ + "_" + stateName_;
    ActionRegistry::Instance().RegisterAction("Animation", fullActionName_, [weak_, stateName_]()
    {
        if (const auto animation_ = weak_.lock())
        {
            const auto owner_ = animation_->GetGameObject();
            const auto graph_ = owner_ ? owner_->GetComponent<AnimationGraphBridgeComponent>() : nullptr;
            if (graph_)
            {
                graph_->RequestStateTransition(stateName_, 0.2f, false, 0.0f, animation_->IsSkillControlled());
            }
            else
            {
                animation_->ChangeState(stateName_);
            }
        }
    });
}

void AnimationControllerComponent::EnsureGraphNodeForState(const std::string& stateName_)
{
    if (stateName_.empty())
    {
        return;
    }

    auto exists_ = std::any_of(graphNodes.begin(), graphNodes.end(), [&](const AnimGraphNode& node_) {
        return node_.stateName == stateName_;
        });
    if (exists_)
    {
        return;
    }

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

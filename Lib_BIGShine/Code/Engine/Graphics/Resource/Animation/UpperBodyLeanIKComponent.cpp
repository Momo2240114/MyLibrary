#include "PrecompiledHeader.h"

#include "UpperBodyLeanIKComponent.h"

#include "AnimationControllerComponent.h"
#include "Engine/Debug/TracyProfiler.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Graphics/Resource/GlTFModel.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <imgui.h>

using namespace DirectX;

namespace
{
    constexpr float EPSILON = 0.00001f;

    // 上半身リーンではワールド座標、ローカル方向、デバッグ描画を同じ流れで扱うための小さな補助関数群。
    XMFLOAT3 GetTranslation(const XMFLOAT4X4& matrix_)
    {
        return { matrix_._41, matrix_._42, matrix_._43 };
    }

    XMFLOAT3 Add(const XMFLOAT3& lhs_, const XMFLOAT3& rhs_)
    {
        return { lhs_.x + rhs_.x, lhs_.y + rhs_.y, lhs_.z + rhs_.z };
    }

    XMFLOAT3 Subtract(const XMFLOAT3& lhs_, const XMFLOAT3& rhs_)
    {
        return { lhs_.x - rhs_.x, lhs_.y - rhs_.y, lhs_.z - rhs_.z };
    }

    XMFLOAT3 Scale(const XMFLOAT3& value_, float scale_)
    {
        return { value_.x * scale_, value_.y * scale_, value_.z * scale_ };
    }

    float Length(const XMFLOAT3& value_)
    {
        return std::sqrt(value_.x * value_.x + value_.y * value_.y + value_.z * value_.z);
    }

    XMFLOAT3 Normalize(const XMFLOAT3& value_)
    {
        const float length_ = Length(value_);
        return length_ > EPSILON ? Scale(value_, 1.0f / length_) : XMFLOAT3{};
    }

    float SmoothToward(float current_, float target_, float speed_, float deltaTime_)
    {
        const float safeDeltaTime_ = std::clamp(deltaTime_, 0.0f, 0.1f);
        const float blend_ = 1.0f - std::exp(-(std::max)(speed_, 0.0f) * safeDeltaTime_);
        return current_ + (target_ - current_) * blend_;
    }

    void DrawBoneBetween(
        ShapeRenderSystem& renderer_,
        const XMFLOAT3& startPosition_,
        const XMFLOAT3& endPosition_,
        const XMFLOAT4& color_)
    {
        const XMFLOAT3 difference_ = Subtract(endPosition_, startPosition_);
        const float length_ = Length(difference_);
        if (length_ <= EPSILON) return;

        const XMVECTOR forward_ = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
        const XMVECTOR direction_ = XMVector3Normalize(XMLoadFloat3(&difference_));
        const float dot_ = std::clamp(
            XMVectorGetX(XMVector3Dot(forward_, direction_)),
            -1.0f,
            1.0f);

        XMMATRIX transform_ = XMMatrixIdentity();
        if (dot_ < 1.0f - EPSILON)
        {
            XMVECTOR axis_ = XMVector3Cross(forward_, direction_);
            if (XMVectorGetX(XMVector3LengthSq(axis_)) <= EPSILON)
            {
                axis_ = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
            }
            transform_ = XMMatrixRotationAxis(XMVector3Normalize(axis_), std::acos(dot_));
        }

        transform_.r[3] = XMVectorSet(startPosition_.x, startPosition_.y, startPosition_.z, 1.0f);

        XMFLOAT4X4 transformStorage_;
        XMStoreFloat4x4(&transformStorage_, transform_);
        renderer_.DrawBone(transformStorage_, length_, color_);
    }
}

const std::string& UpperBodyLeanIKComponent::GetName() const
{
    static const std::string name_ = "UpperBodyLeanIKComponent";
    return name_;
}

void UpperBodyLeanIKComponent::Start()
{
    // IKMapに上半身リーン用チェーンを用意し、移動速度を自動取得する場合の初期位置も保存する。
    ResolveDependencies();

    const std::shared_ptr<IKMapComponent> ikMap_ = ikMap.lock();
    if (ikMap_)
    {
        ikMap_->AddUpperBodyLeanChain("UpperBodyLean");
        if (ikChainName == "UPPER_BODY") ikChainName = "UpperBodyLean";
    }

    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (owner_ && owner_->transform)
    {
        previousOwnerPosition = owner_->transform->GetWorldPosition();
        hasPreviousOwnerPosition = true;
    }
}

bool UpperBodyLeanIKComponent::IsLoadReady() const
{
    const auto owner_ = const_cast<UpperBodyLeanIKComponent*>(this)->GetGameObject();
    const auto model_ = owner_ ? owner_->GetComponent<GltfModelCommponennt>() : nullptr;
    if (!model_) return true;

    model_->UpdateLoadState();
    return !model_->IsModelLoading();
}

bool UpperBodyLeanIKComponent::ResolveDependencies()
{
    // 同じGameObject上のIKMap、モデル、AnimationControllerを必要になった時点で解決する。
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return false;

    if (ikMap.expired()) ikMap = owner_->GetComponent<IKMapComponent>();
    if (model.expired()) model = owner_->GetComponent<GltfModelCommponennt>();
    if (animationController.expired()) animationController = owner_->GetComponent<AnimationControllerComponent>();

    return !ikMap.expired() && !model.expired();
}

void UpperBodyLeanIKComponent::SetMoveDirection(const XMFLOAT3& direction_)
{
    manualMoveDirection = direction_;
    useManualDirection = true;
}

void UpperBodyLeanIKComponent::SetMoveVelocity(const XMFLOAT3& velocity_)
{
    manualMoveVelocity = velocity_;
    hasManualVelocity = true;
    useManualDirection = false;
}

void UpperBodyLeanIKComponent::ResetLean()
{
    currentForwardLean = 0.0f;
    currentSideLean = 0.0f;
    debugLocalMoveDirection = {};
    debugMoveSpeed = 0.0f;
}

void UpperBodyLeanIKComponent::AddApplyAnimationName(const std::string& animationName_)
{
    if (animationName_.empty()) return;
    if (std::find(applyAnimationNames.begin(), applyAnimationNames.end(), animationName_) != applyAnimationNames.end())
    {
        return;
    }
    applyAnimationNames.push_back(animationName_);
}

void UpperBodyLeanIKComponent::ClearApplyAnimationNames()
{
    applyAnimationNames.clear();
}

std::vector<std::string> UpperBodyLeanIKComponent::GetCurrentAnimationNames() const
{
    // 現在ステート名、Sequencer名、参照中クリップ名を集め、アニメーションフィルタの判定材料にする。
    std::vector<std::string> names_;

    auto addName_ = [&names_](const std::string& name_)
        {
            if (name_.empty()) return;
            if (std::find(names_.begin(), names_.end(), name_) != names_.end()) return;
            names_.push_back(name_);
        };

    const std::shared_ptr<AnimationControllerComponent> animationController_ = animationController.lock();
    if (!animationController_) return names_;

    const AnimState* currentState_ = animationController_->GetCurrentState();
    if (!currentState_) return names_;

    addName_(currentState_->name);
    addName_(currentState_->sequencerDataName);

    const std::shared_ptr<GltfModelCommponennt> model_ = model.lock();
    if (!model_) return names_;

    const std::vector<GltfModelCommponennt::Animation>& animations_ = model_->GetAnimations();
    auto addClipName_ = [&animations_, &addName_](int animationIndex_)
        {
            if (animationIndex_ < 0 || animationIndex_ >= static_cast<int>(animations_.size())) return;
            addName_(animations_[animationIndex_].name);
        };

    addClipName_(currentState_->animationIndex);
    addClipName_(currentState_->animIndexA);
    addClipName_(currentState_->animIndexB);
    addClipName_(currentState_->blend2DForwardAnimationIndex);
    addClipName_(currentState_->blend2DBackwardAnimationIndex);
    addClipName_(currentState_->blend2DLeftAnimationIndex);
    addClipName_(currentState_->blend2DRightAnimationIndex);
    addClipName_(currentState_->blend2DForwardLeftAnimationIndex);
    addClipName_(currentState_->blend2DForwardRightAnimationIndex);
    addClipName_(currentState_->blend2DBackwardLeftAnimationIndex);
    addClipName_(currentState_->blend2DBackwardRightAnimationIndex);
    addClipName_(currentState_->blend2DForwardLeft90AnimationIndex);
    addClipName_(currentState_->blend2DForwardRight90AnimationIndex);
    addClipName_(currentState_->blend2DBackwardLeft90AnimationIndex);
    addClipName_(currentState_->blend2DBackwardRight90AnimationIndex);
    addClipName_(currentState_->blend2DIdleAnimationIndex);

    return names_;
}

bool UpperBodyLeanIKComponent::MatchesAnimationName(const std::string& currentName_) const
{
    if (currentName_.empty()) return false;

    for (const std::string& animationName_ : applyAnimationNames)
    {
        if (animationName_.empty()) continue;
        if (usePartialNameMatch)
        {
            if (currentName_.find(animationName_) != std::string::npos) return true;
        }
        else if (currentName_ == animationName_)
        {
            return true;
        }
    }

    return false;
}

bool UpperBodyLeanIKComponent::ShouldApplyForCurrentAnimation() const
{
    // 歩き/走りなど、指定したアニメーション中だけ体を傾けたい場合のON/OFF判定。
    if (!useAnimationFilter) return true;

    const std::shared_ptr<AnimationControllerComponent> animationController_ = animationController.lock();
    if (!animationController_) return !disableWhenNoAnimationMatched;

    const std::vector<std::string> currentNames_ = GetCurrentAnimationNames();
    if (currentNames_.empty()) return !disableWhenNoAnimationMatched;

    const bool matched_ = std::any_of(
        currentNames_.begin(),
        currentNames_.end(),
        [this](const std::string& currentName_)
        {
            return MatchesAnimationName(currentName_);
        });
    return invertAnimationFilter ? !matched_ : matched_;
}

XMFLOAT3 UpperBodyLeanIKComponent::ResolveMoveVelocity(float deltaTime_)
{
    // 手動方向、手動速度、Ownerの移動差分の順に移動情報を決め、リーン角度の入力にする。
    if (useManualDirection)
    {
        return Normalize(manualMoveDirection);
    }

    if (hasManualVelocity)
    {
        return manualMoveVelocity;
    }

    if (!useOwnerVelocity) return {};

    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_ || !owner_->transform) return {};

    const XMFLOAT3 currentPosition_ = owner_->transform->GetWorldPosition();
    XMFLOAT3 velocity_{};

    if (hasPreviousOwnerPosition && deltaTime_ > EPSILON)
    {
        velocity_ = Scale(Subtract(currentPosition_, previousOwnerPosition), 1.0f / deltaTime_);
    }

    previousOwnerPosition = currentPosition_;
    hasPreviousOwnerPosition = true;
    return velocity_;
}

XMFLOAT3 UpperBodyLeanIKComponent::ToOwnerLocalDirection(const XMFLOAT3& worldDirection_) const
{
    // ワールド移動方向をキャラクター基準の前後左右へ変換し、前傾/横傾きへ割り当てやすくする。
    const std::shared_ptr<GameObject> owner_ = gameObject.lock();
    if (!owner_ || !owner_->transform) return {};

    XMMATRIX ownerWorld_ = owner_->transform->GetWorldMatrix();
    ownerWorld_.r[3] = XMVectorSet(0, 0, 0, 1);

    XMFLOAT3 localDirection_{};
    XMStoreFloat3(
        &localDirection_,
        XMVector3TransformNormal(XMLoadFloat3(&worldDirection_), XMMatrixInverse(nullptr, ownerWorld_)));
    return Normalize(localDirection_);
}

void UpperBodyLeanIKComponent::UpdateLeanAngles(
    const XMFLOAT3& localMoveDirection_,
    float speed_,
    float deltaTime_,
    bool shouldApply_)
{
    // 移動方向と速度から目標リーン角を作り、停止時はreturnSpeed、移動時はsmoothSpeedで追従する。
    float targetForwardLean_ = 0.0f;
    float targetSideLean_ = 0.0f;

    if (shouldApply_ && speed_ >= (std::max)(minMoveSpeed, 0.0f))
    {
        const float speedRate_ = useManualDirection
            ? 1.0f
            : std::clamp(speed_ / (std::max)(minMoveSpeed * 4.0f, EPSILON), 0.0f, 1.0f);
        const float forwardScale_ = localMoveDirection_.z < 0.0f && !allowBackwardLean
            ? 0.0f
            : (localMoveDirection_.z < 0.0f ? backwardLeanScale : 1.0f);

        targetForwardLean_ = localMoveDirection_.z
            * (std::max)(maxForwardLeanAngle, 0.0f)
            * forwardScale_
            * speedRate_
            * std::clamp(leanStrength, 0.0f, 4.0f);
        targetSideLean_ = -localMoveDirection_.x
            * (std::max)(maxSideLeanAngle, 0.0f)
            * speedRate_
            * std::clamp(leanStrength, 0.0f, 4.0f);
    }

    const float blendSpeed_ = std::abs(targetForwardLean_) <= EPSILON && std::abs(targetSideLean_) <= EPSILON
        ? returnSpeed
        : smoothSpeed;

    currentForwardLean = SmoothToward(currentForwardLean, targetForwardLean_, blendSpeed_, deltaTime_);
    currentSideLean = SmoothToward(currentSideLean, targetSideLean_, blendSpeed_, deltaTime_);
}

void UpperBodyLeanIKComponent::LateUpdate(float deltaTime_)
{
    BS_PROFILE_SCOPE("UpperBodyLeanIKComponent::LateUpdate");

    // アニメーション姿勢が決まった後に、移動方向に応じた上半身の前後/左右傾きを追加する。
    if (!ResolveDependencies())
    {
        ResetLean();
        return;
    }
    const std::shared_ptr<GltfModelCommponennt> modelReady_ = model.lock();
    if (modelReady_) modelReady_->UpdateLoadState();
    if (!modelReady_ || !modelReady_->IsModelReady())
    {
        ResetLean();
        return;
    }

    const XMFLOAT3 moveVelocity_ = ResolveMoveVelocity(deltaTime_);
    const float speed_ = useManualDirection ? Length(manualMoveDirection) : Length(moveVelocity_);
    const XMFLOAT3 moveDirection_ = Normalize(moveVelocity_);
    const XMFLOAT3 localMoveDirection_ = ToOwnerLocalDirection(moveDirection_);
    debugLocalMoveDirection = localMoveDirection_;
    debugMoveSpeed = speed_;

    const bool shouldApply_ = leanEnabled && ShouldApplyForCurrentAnimation();
    UpdateLeanAngles(localMoveDirection_, speed_, deltaTime_, shouldApply_);

    const std::shared_ptr<IKMapComponent> ikMap_ = ikMap.lock();
    if (!ikMap_) return;

    IKMapComponent::IKChain* chain_ = ikMap_->FindChain(ikChainName);
    if (chain_ && chain_->type != IKMapComponent::IK_TYPE::UPPER_BODY_LEAN) chain_ = nullptr;
    chainMissingWarning = chain_ == nullptr;
    if (!chain_) return;
    if (!chain_->enabled || !ResolveChain(*chain_)) return;

    if (std::abs(currentForwardLean) > EPSILON || std::abs(currentSideLean) > EPSILON)
    {
        ApplyLeanChain(*chain_);
    }

    DrawDebugShapes(*chain_);
}

bool UpperBodyLeanIKComponent::ResolveChain(IKMapComponent::IKChain& chain_)
{
    // IKMapに登録されたチェーン名から、実際のボーンindex列を解決する。
    if (chain_.rootNodeName.empty() || chain_.tipNodeName.empty()) return false;
    if (!chain_.nodeIndices.empty()) return chain_.nodeIndices.size() >= 2;

    const std::shared_ptr<IKMapComponent> ikMap_ = ikMap.lock();
    if (!ikMap_) return false;

    return ikMap_->ResolveChainByName(chain_.name);
}

void UpperBodyLeanIKComponent::ApplyLeanChain(IKMapComponent::IKChain& chain_)
{
    // チェーン上の各ボーンに重み付きでローカル回転を足し、腰から胸へ自然に傾きを分配する。
    const std::shared_ptr<GltfModelCommponennt> model_ = model.lock();
    if (!model_) return;

    std::vector<GltfModelCommponennt::Node>& nodes_ = model_->GetNodes();
    if (chain_.nodeIndices.empty()) return;

    std::vector<float> weights_ = boneWeights;
    if (weights_.size() != chain_.nodeIndices.size())
    {
        weights_.assign(chain_.nodeIndices.size(), 1.0f);
    }

    if (autoNormalizeWeights)
    {
        float weightSum_ = 0.0f;
        for (const float weight_ : weights_) weightSum_ += (std::max)(weight_, 0.0f);
        if (weightSum_ > EPSILON)
        {
            for (float& weight_ : weights_) weight_ = (std::max)(weight_, 0.0f) / weightSum_;
        }
    }

    bool chainChanged_ = false;

    for (size_t pointIndex_ = 0; pointIndex_ < chain_.nodeIndices.size(); ++pointIndex_)
    {
        const int nodeIndex_ = chain_.nodeIndices[pointIndex_];
        model_->UpdateNodeTransformOnly(nodeIndex_);

        GltfModelCommponennt::Node& node_ = nodes_.at(nodeIndex_);
        const float chainWeight_ = pointIndex_ < weights_.size() ? weights_[pointIndex_] : 0.0f;
        if (chainWeight_ <= EPSILON) continue;

        const float forwardRadians_ = XMConvertToRadians(currentForwardLean * chainWeight_ * chain_.influence);
        const float sideRadians_ = XMConvertToRadians(currentSideLean * chainWeight_ * chain_.influence);

        const XMMATRIX currentLocalRotation_ = XMMatrixRotationQuaternion(XMLoadFloat4(&node_.rotation));
        const XMMATRIX leanRotation_ = XMMatrixRotationRollPitchYaw(forwardRadians_, 0.0f, sideRadians_);
        const XMMATRIX desiredLocalRotation_ = currentLocalRotation_ * leanRotation_;

        XMVECTOR scale_;
        XMVECTOR rotation_;
        XMVECTOR translation_;
        if (XMMatrixDecompose(&scale_, &rotation_, &translation_, desiredLocalRotation_))
        {
            XMStoreFloat4(&node_.rotation, XMQuaternionNormalize(rotation_));
            model_->UpdateNodeTransformOnly(nodeIndex_);
            chainChanged_ = true;
        }
    }

    if (chainChanged_)
    {
        model_->UpdateNodeTransformFrom(chain_.nodeIndices.front());
    }
}

void UpperBodyLeanIKComponent::DrawDebugShapes(const IKMapComponent::IKChain& chain_) const
{
    if (!showDebugShapes || chain_.nodeIndices.size() < 2) return;

    const std::shared_ptr<GltfModelCommponennt> model_ = model.lock();
    if (!model_) return;

    ShapeRenderSystem& renderer_ = GraphicsDeviceManager::Instance().GetShapeRendererSystem();
    const std::vector<GltfModelCommponennt::Node>& nodes_ = model_->GetNodes();
    const XMFLOAT4 chainColor_{ 0.25f, 0.85f, 1.0f, 0.9f };

    for (size_t pointIndex_ = 0; pointIndex_ + 1 < chain_.nodeIndices.size(); ++pointIndex_)
    {
        DrawBoneBetween(
            renderer_,
            GetTranslation(nodes_.at(chain_.nodeIndices[pointIndex_]).worldTransform),
            GetTranslation(nodes_.at(chain_.nodeIndices[pointIndex_ + 1]).worldTransform),
            chainColor_);
    }
}

void UpperBodyLeanIKComponent::Gui()
{
    // Inspectorではリーン角度、適用アニメーション、ボーン別重み、デバッグ表示を調整する。
    ResolveDependencies();

    ImGui::Checkbox("Enabled", &leanEnabled);
    ImGui::Checkbox("Use Owner Velocity", &useOwnerVelocity);
    ImGui::Checkbox("Use Manual Direction", &useManualDirection);

    const std::shared_ptr<IKMapComponent> ikMap_ = ikMap.lock();
    IKMapComponent::IKChain* selectedChain_ = nullptr;
    if (ikMap_)
    {
        selectedChain_ = ikMap_->FindChain(ikChainName);
        if (selectedChain_ && selectedChain_->type != IKMapComponent::IK_TYPE::UPPER_BODY_LEAN)
        {
            selectedChain_ = nullptr;
        }
    }

    const char* chainPreview_ = selectedChain_ ? selectedChain_->name.c_str() : "Select Upper Body Lean Chain";
    if (ImGui::BeginCombo("IK Chain", chainPreview_))
    {
        if (ikMap_)
        {
            for (IKMapComponent::IKChain& chain_ : ikMap_->GetChains())
            {
                if (chain_.type != IKMapComponent::IK_TYPE::UPPER_BODY_LEAN) continue;
                if (ImGui::Selectable(chain_.name.c_str(), chain_.name == ikChainName))
                {
                    ikChainName = chain_.name;
                }
            }
        }
        ImGui::EndCombo();
    }

    if (ImGui::Button("Add Upper Body Lean Chain") && ikMap_)
    {
        IKMapComponent::IKChain& chain_ = ikMap_->AddUpperBodyLeanChain("UpperBodyLean");
        ikChainName = chain_.name;
    }

    ImGui::DragFloat("Max Forward Lean Angle", &maxForwardLeanAngle, 0.25f, 0.0f, 45.0f);
    ImGui::DragFloat("Max Side Lean Angle", &maxSideLeanAngle, 0.25f, 0.0f, 45.0f);
    ImGui::DragFloat("Lean Strength", &leanStrength, 0.01f, 0.0f, 4.0f);
    ImGui::DragFloat("Smooth Speed", &smoothSpeed, 0.1f, 0.0f, 60.0f);
    ImGui::DragFloat("Return Speed", &returnSpeed, 0.1f, 0.0f, 60.0f);
    ImGui::DragFloat("Min Move Speed", &minMoveSpeed, 0.01f, 0.0f, 10.0f);
    ImGui::Checkbox("Allow Backward Lean", &allowBackwardLean);
    ImGui::DragFloat("Backward Lean Scale", &backwardLeanScale, 0.01f, 0.0f, 1.0f);

    ImGui::DragFloat3("Manual Move Direction", &manualMoveDirection.x, 0.01f, -1.0f, 1.0f);
    ImGui::DragFloat3("Manual Move Velocity", &manualMoveVelocity.x, 0.01f, -20.0f, 20.0f);

    ImGui::SeparatorText("Animation Filter");
    // ステート名やクリップ名で適用対象を絞り、攻撃や待機中に不要な傾きが出ないようにする。
    ImGui::Checkbox("Use Animation Filter", &useAnimationFilter);
    ImGui::Checkbox("Use Partial Name Match", &usePartialNameMatch);
    ImGui::Checkbox("Invert Animation Filter", &invertAnimationFilter);
    ImGui::Checkbox("Disable When No Animation Matched", &disableWhenNoAnimationMatched);
    const std::vector<std::string> currentAnimationNames_ = GetCurrentAnimationNames();
    if (currentAnimationNames_.empty())
    {
        ImGui::TextUnformatted("Current Animation: None");
    }
    else
    {
        ImGui::TextUnformatted("Current Animation:");
        for (const std::string& currentName_ : currentAnimationNames_)
        {
            ImGui::BulletText("%s", currentName_.c_str());
        }
    }
    ImGui::Text("Animation Matched: %s", ShouldApplyForCurrentAnimation() ? "Yes" : "No");

    char animationNameBuffer_[128]{};
    strncpy_s(animationNameBuffer_, animationNameInput.c_str(), sizeof(animationNameBuffer_));
    if (ImGui::InputText("Animation Name##UpperBodyLeanAnimationNameInput", animationNameBuffer_, sizeof(animationNameBuffer_)))
    {
        animationNameInput = animationNameBuffer_;
    }
    if (ImGui::Button("Add##UpperBodyLeanAnimationNameAdd"))
    {
        AddApplyAnimationName(animationNameInput);
        animationNameInput.clear();
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear##UpperBodyLeanAnimationNamesClear"))
    {
        ClearApplyAnimationNames();
    }

    const std::shared_ptr<AnimationControllerComponent> animationController_ = animationController.lock();
    if (animationController_ && ImGui::CollapsingHeader("Animation States", ImGuiTreeNodeFlags_DefaultOpen))
    {
        std::vector<std::string> stateNames_;
        stateNames_.reserve(animationController_->GetStates().size());
        for (const auto& statePair_ : animationController_->GetStates())
        {
            if (statePair_.second) stateNames_.push_back(statePair_.first);
        }
        std::sort(stateNames_.begin(), stateNames_.end());

        for (const std::string& stateName_ : stateNames_)
        {
            ImGui::PushID(stateName_.c_str());
            ImGui::TextUnformatted(stateName_.c_str());
            ImGui::SameLine(260.0f);
            if (ImGui::Button("Add##State")) AddApplyAnimationName(stateName_);
            ImGui::PopID();
        }
    }

    const std::shared_ptr<GltfModelCommponennt> filterModel_ = model.lock();
    if (filterModel_ && ImGui::CollapsingHeader("Animation Clips"))
    {
        const std::vector<GltfModelCommponennt::Animation>& animations_ = filterModel_->GetAnimations();
        for (size_t animationIndex_ = 0; animationIndex_ < animations_.size(); ++animationIndex_)
        {
            const std::string& clipName_ = animations_[animationIndex_].name;
            if (clipName_.empty()) continue;

            ImGui::PushID(static_cast<int>(animationIndex_));
            ImGui::TextUnformatted(clipName_.c_str());
            ImGui::SameLine(260.0f);
            if (ImGui::Button("Add##Clip")) AddApplyAnimationName(clipName_);
            ImGui::PopID();
        }
    }

    int removeAnimationIndex_ = -1;
    for (size_t animationIndex_ = 0; animationIndex_ < applyAnimationNames.size(); ++animationIndex_)
    {
        ImGui::PushID(static_cast<int>(animationIndex_));
        ImGui::TextUnformatted(applyAnimationNames[animationIndex_].c_str());
        ImGui::SameLine();
        if (ImGui::Button("Remove##ApplyAnimationName")) removeAnimationIndex_ = static_cast<int>(animationIndex_);
        ImGui::PopID();
    }
    if (removeAnimationIndex_ >= 0)
    {
        applyAnimationNames.erase(applyAnimationNames.begin() + removeAnimationIndex_);
    }

    ImGui::SeparatorText("Bone Weights");
    // チェーン解決後の各ボーンへ、どれだけ傾きを分配するかを個別に調整する。
    ImGui::Checkbox("Auto Normalize Weights", &autoNormalizeWeights);
    IKMapComponent::IKChain* chain_ = ikMap_ ? ikMap_->FindChain(ikChainName) : nullptr;
    if (chain_ && chain_->type != IKMapComponent::IK_TYPE::UPPER_BODY_LEAN) chain_ = nullptr;
    if (chain_ && ResolveChain(*chain_))
    {
        if (boneWeights.size() != chain_->nodeIndices.size())
        {
            boneWeights.assign(chain_->nodeIndices.size(), 1.0f);
        }
        const std::shared_ptr<GltfModelCommponennt> model_ = model.lock();
        for (size_t pointIndex_ = 0; pointIndex_ < boneWeights.size(); ++pointIndex_)
        {
            ImGui::PushID(static_cast<int>(pointIndex_));
            const char* nodeName_ = model_
                ? model_->GetNodes().at(chain_->nodeIndices[pointIndex_]).name.c_str()
                : "Node";
            ImGui::DragFloat(nodeName_, &boneWeights[pointIndex_], 0.01f, 0.0f, 1.0f);
            ImGui::PopID();
        }
    }
    else
    {
        ImGui::TextDisabled("IK chain is not found or not resolved.");
    }

    ImGui::SeparatorText("Debug");
    ImGui::Checkbox("Debug Draw", &showDebugShapes);
    ImGui::Checkbox("Debug Text", &showDebugText);
    if (showDebugText)
    {
        ImGui::Text("Local Move Direction: %.2f, %.2f, %.2f",
            debugLocalMoveDirection.x,
            debugLocalMoveDirection.y,
            debugLocalMoveDirection.z);
        ImGui::Text("Move Speed: %.3f", debugMoveSpeed);
        ImGui::Text("Current Forward Lean: %.2f", currentForwardLean);
        ImGui::Text("Current Side Lean: %.2f", currentSideLean);
        if (chainMissingWarning) ImGui::TextDisabled("IK chain was not found.");
    }
}

void UpperBodyLeanIKComponent::Serialize(nlohmann::json& json_) const
{
    // リーン挙動の調整値とアニメーションフィルタを保存し、キャラクターごとの味付けを保持する。
    json_["LeanEnabled"] = leanEnabled;
    json_["IKChainName"] = ikChainName;
    json_["MaxForwardLeanAngle"] = maxForwardLeanAngle;
    json_["MaxSideLeanAngle"] = maxSideLeanAngle;
    json_["LeanStrength"] = leanStrength;
    json_["SmoothSpeed"] = smoothSpeed;
    json_["ReturnSpeed"] = returnSpeed;
    json_["MinMoveSpeed"] = minMoveSpeed;
    json_["UseOwnerVelocity"] = useOwnerVelocity;
    json_["UseManualDirection"] = useManualDirection;
    json_["ManualMoveDirection"] = { manualMoveDirection.x, manualMoveDirection.y, manualMoveDirection.z };
    json_["AllowBackwardLean"] = allowBackwardLean;
    json_["BackwardLeanScale"] = backwardLeanScale;
    json_["BoneWeights"] = boneWeights;
    json_["AutoNormalizeWeights"] = autoNormalizeWeights;
    json_["ShowDebugShapes"] = showDebugShapes;
    json_["ShowDebugText"] = showDebugText;
    json_["UseAnimationFilter"] = useAnimationFilter;
    json_["ApplyAnimationNames"] = applyAnimationNames;
    json_["UsePartialNameMatch"] = usePartialNameMatch;
    json_["InvertAnimationFilter"] = invertAnimationFilter;
    json_["DisableWhenNoAnimationMatched"] = disableWhenNoAnimationMatched;
}

void UpperBodyLeanIKComponent::Deserialize(const nlohmann::json& json_)
{
    // 旧データに項目が無くても既定値で復元し、読み込み直後は補間履歴をリセットする。
    leanEnabled = json_.value("LeanEnabled", true);
    ikChainName = json_.value("IKChainName", std::string("UpperBodyLean"));
    maxForwardLeanAngle = json_.value("MaxForwardLeanAngle", 15.0f);
    maxSideLeanAngle = json_.value("MaxSideLeanAngle", 10.0f);
    leanStrength = json_.value("LeanStrength", 1.0f);
    smoothSpeed = json_.value("SmoothSpeed", 10.0f);
    returnSpeed = json_.value("ReturnSpeed", 7.0f);
    minMoveSpeed = json_.value("MinMoveSpeed", 0.05f);
    useOwnerVelocity = json_.value("UseOwnerVelocity", true);
    useManualDirection = json_.value("UseManualDirection", false);
    if (json_.contains("ManualMoveDirection"))
    {
        manualMoveDirection = {
            json_["ManualMoveDirection"][0],
            json_["ManualMoveDirection"][1],
            json_["ManualMoveDirection"][2]
        };
    }
    allowBackwardLean = json_.value("AllowBackwardLean", true);
    backwardLeanScale = json_.value("BackwardLeanScale", 0.45f);
    boneWeights = json_.value("BoneWeights", std::vector<float>{ 0.15f, 0.35f, 0.35f, 0.15f });
    autoNormalizeWeights = json_.value("AutoNormalizeWeights", true);
    showDebugShapes = json_.value("ShowDebugShapes", false);
    showDebugText = json_.value("ShowDebugText", true);
    useAnimationFilter = json_.value("UseAnimationFilter", true);
    applyAnimationNames = json_.value("ApplyAnimationNames", std::vector<std::string>{ "Walk", "Run", "Dash" });
    usePartialNameMatch = json_.value("UsePartialNameMatch", true);
    invertAnimationFilter = json_.value("InvertAnimationFilter", false);
    disableWhenNoAnimationMatched = json_.value("DisableWhenNoAnimationMatched", true);
    ResetLean();
    hasManualVelocity = false;
    hasPreviousOwnerPosition = false;
}

REGISTER_COMPONENT(UpperBodyLeanIKComponent, "UpperBodyLeanIKComponent")

#include "HumanLookAtIKComponent.h"

#include "AnimationControllerComponent.h"
#include "Engine/Debug/TracyProfiler.h"
#include "Engine/GamePlay/Camera/CameraLockOnComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
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
    constexpr float EPSILON = 0.00001f; // 0除算や極小ベクトル判定に使う許容値。

    // 行列から平行移動成分を取り出す。
    XMFLOAT3 GetTranslation(const XMFLOAT4X4& matrix_)
    {
        return { matrix_._41, matrix_._42, matrix_._43 };
    }

    // 2つのベクトルを加算する。
    XMFLOAT3 Add(const XMFLOAT3& lhs_, const XMFLOAT3& rhs_)
    {
        return { lhs_.x + rhs_.x, lhs_.y + rhs_.y, lhs_.z + rhs_.z };
    }

    // 2つのベクトルを減算する。
    XMFLOAT3 Subtract(const XMFLOAT3& lhs_, const XMFLOAT3& rhs_)
    {
        return { lhs_.x - rhs_.x, lhs_.y - rhs_.y, lhs_.z - rhs_.z };
    }

    // ベクトルを指定倍率で拡大する。
    XMFLOAT3 Scale(const XMFLOAT3& value_, float scale_)
    {
        return { value_.x * scale_, value_.y * scale_, value_.z * scale_ };
    }

    // ベクトルの長さを計算する。
    float Length(const XMFLOAT3& value_)
    {
        return std::sqrt(value_.x * value_.x + value_.y * value_.y + value_.z * value_.z);
    }

    // ベクトルを正規化する。
    XMFLOAT3 Normalize(const XMFLOAT3& value_)
    {
        const float length_ = Length(value_);
        return length_ > EPSILON ? Scale(value_, 1.0f / length_) : XMFLOAT3{};
    }

    // 2つのベクトルを線形補間する。
    XMFLOAT3 Lerp(const XMFLOAT3& from_, const XMFLOAT3& to_, float amount_)
    {
        return Add(from_, Scale(Subtract(to_, from_), std::clamp(amount_, 0.0f, 1.0f)));
    }

    // 2点間にボーン形状を描画する。
    void DrawBoneBetween(
        ShapeRenderSystem& renderer_,
        const XMFLOAT3& startPosition_,
        const XMFLOAT3& endPosition_,
        const XMFLOAT4& color_)
    {
        const XMFLOAT3 difference_ = Subtract(endPosition_, startPosition_);
        const float length_ = Length(difference_);
        if (length_ <= EPSILON) return;

        // Z軸向きのボーンを2点間の方向へ回転させる。
        const XMVECTOR forward_ = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
        const XMVECTOR direction_ = XMVector3Normalize(XMLoadFloat3(&difference_));
        const float dot_ = std::clamp(
            XMVectorGetX(XMVector3Dot(forward_, direction_)),
            -1.0f,
            1.0f);

        XMMATRIX transform_ = XMMatrixIdentity();

        // 正面方向と描画方向が違う場合だけ回転行列を作る。
        if (dot_ < 1.0f - EPSILON)
        {
            XMVECTOR axis_ = XMVector3Cross(forward_, direction_);
            if (XMVectorGetX(XMVector3LengthSq(axis_)) <= EPSILON)
            {
                axis_ = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
            }
            transform_ = XMMatrixRotationAxis(XMVector3Normalize(axis_), std::acos(dot_));
        }

        // ボーン形状の開始位置を設定する。
        transform_.r[3] = XMVectorSet(
            startPosition_.x,
            startPosition_.y,
            startPosition_.z,
            1.0f);

        XMFLOAT4X4 transformStorage_;
        XMStoreFloat4x4(&transformStorage_, transform_);
        renderer_.DrawBone(transformStorage_, length_, color_);
    }
}

// Component名を返す。
const std::string& HumanLookAtIKComponent::GetName() const
{
    static const std::string name_ = "HumanLookAtIKComponent";
    return name_;
}

// 必要Componentを取得し、LookAtチェーンとターゲットを初期化する。
void HumanLookAtIKComponent::Start()
{
    if (!ResolveDependencies()) return;

    const std::shared_ptr<IKMapComponent> ikMap_ = ikMap.lock();
    if (!ikMap_) return;

    ikMap_->AddLookAtChain("LookAt");
    ResolveTarget();
}

// 注視対象名を変更し、既存ターゲット参照をリセットする。
void HumanLookAtIKComponent::SetTargetName(const std::string& targetName_)
{
    targetName = targetName_;
    target.reset();
}

// Ownerから必要なComponent参照を取得する。
bool HumanLookAtIKComponent::ResolveDependencies()
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return false;

    if (ikMap.expired()) ikMap = owner_->GetComponent<IKMapComponent>();
    if (model.expired()) model = owner_->GetComponent<GltfModelCommponennt>();

    if (animationController.expired())
    {
        animationController = owner_->GetComponent<AnimationControllerComponent>();
    }

    return !ikMap.expired() && !model.expired();
}

// targetNameから注視対象Objectを検索する。
std::shared_ptr<CameraLockOnComponent>
HumanLookAtIKComponent::ResolveCameraLockOnComponent()
{
    if (const std::shared_ptr<CameraLockOnComponent> currentLockOn_ =
        cameraLockOn.lock())
    {
        return currentLockOn_;
    }

    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_ || !owner_->manager) return nullptr;

    for (const std::shared_ptr<GameObject>& object_ : owner_->manager->GetObjects())
    {
        if (!object_) continue;
        if (const std::shared_ptr<CameraLockOnComponent> lockOn_ =
            object_->GetComponent<CameraLockOnComponent>())
        {
            cameraLockOn = lockOn_;
            return lockOn_;
        }
    }

    return nullptr;
}

std::shared_ptr<GameObject> HumanLookAtIKComponent::ResolveTarget()
{
    if (useLockOnTarget)
    {
        const std::shared_ptr<CameraLockOnComponent> lockOn_ =
            ResolveCameraLockOnComponent();
        if (lockOn_ && lockOn_->IsLockOnEnabled())
        {
            if (const std::shared_ptr<GameObject> lockOnTarget_ =
                lockOn_->GetLockOnTarget())
            {
                return lockOnTarget_;
            }
        }
    }

    if (const std::shared_ptr<GameObject> currentTarget_ = target.lock()) return currentTarget_;

    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_ || !owner_->manager || targetName.empty()) return nullptr;

    const std::shared_ptr<GameObject> resolvedTarget_ = owner_->manager->Find(targetName);
    target = resolvedTarget_;
    return resolvedTarget_;
}

// 現在のAnimationStateでLookAtを許可するか判定する。
bool HumanLookAtIKComponent::IsAllowedAnimationState() const
{
    if (!idleOnly) return true;

    const std::shared_ptr<AnimationControllerComponent> animationController_ = animationController.lock();
    if (!animationController_) return false;

    const AnimState* currentState_ = animationController_->GetCurrentState();
    return currentState_ && currentState_->name == idleStateName;
}

// 現在のAnimationStateに対応するLookModeを返す。
HumanLookAtIKComponent::LOOK_MODE HumanLookAtIKComponent::GetCurrentLookMode() const
{
    const std::shared_ptr<AnimationControllerComponent> animationController_ = animationController.lock();
    if (!animationController_) return LOOK_MODE::HEAD_ONLY;

    const AnimState* currentState_ = animationController_->GetCurrentState();
    if (!currentState_) return LOOK_MODE::HEAD_ONLY;

    const auto modeIterator_ = stateLookModes.find(currentState_->name);
    return modeIterator_ != stateLookModes.end() ? modeIterator_->second : LOOK_MODE::HEAD_ONLY;
}

// アニメーション更新後にLookAt IKを適用する。
void HumanLookAtIKComponent::LateUpdate(float deltaTime_)
{
    BS_PROFILE_SCOPE("HumanLookAtIKComponent::LateUpdate");

    if (!ResolveDependencies()) return;

    const std::shared_ptr<IKMapComponent> ikMap_ = ikMap.lock();
    if (!ikMap_) return;

    std::vector<IKMapComponent::IKChain>& chains_ = ikMap_->GetChains();
    if (lookStates.size() != chains_.size()) lookStates.resize(chains_.size());

    const std::shared_ptr<GameObject> target_ = ResolveTarget();

    XMFLOAT3 targetPosition_{};

    // ターゲットObjectのWorld座標に注視オフセットを加算する。
    if (target_ && target_->transform)
    {
        XMFLOAT4X4 targetWorld_;
        XMStoreFloat4x4(&targetWorld_, target_->transform->GetWorldMatrix());
        targetPosition_ = Add(GetTranslation(targetWorld_), targetOffset);
    }

    const bool shouldLook_ = lookEnabled && target_ && IsAllowedAnimationState();
    const LOOK_MODE lookMode_ = GetCurrentLookMode();

    for (size_t chainIndex_ = 0; chainIndex_ < chains_.size(); ++chainIndex_)
    {
        IKMapComponent::IKChain& chain_ = chains_[chainIndex_];

        // LOOK_AT以外のチェーンは処理しない。
        if (!chain_.enabled || chain_.type != IKMapComponent::IK_TYPE::LOOK_AT) continue;
        if (!ResolveChain(chain_)) continue;

        LookRuntimeState& state_ = lookStates[chainIndex_];

        // フレーム落ち時の急激な補間を防ぐ。
        const float safeDeltaTime_ = std::clamp(deltaTime_, 0.0f, 0.1f);

        // lookBlendSpeedを使って現在の注視率を滑らかに補間する。
        const float blend_ = 1.0f - std::exp(
            -(std::max)(chain_.lookBlendSpeed, 0.0f) * safeDeltaTime_);

        const float desiredWeight_ = shouldLook_ ? 1.0f : 0.0f;
        state_.activeWeight += (desiredWeight_ - state_.activeWeight) * blend_;

        if (state_.activeWeight <= EPSILON || !target_) continue;

        ApplyLookChain(chain_, state_, targetPosition_, safeDeltaTime_, lookMode_);
        DrawDebugShapes(chain_, targetPosition_);
    }
}

// IKチェーンのRootからTipまでのNodeIndex列を作成する。
bool HumanLookAtIKComponent::ResolveChain(IKMapComponent::IKChain& chain_)
{
    if (chain_.rootNodeName.empty() || chain_.tipNodeName.empty()) return false;

    // すでに解決済みの場合はNode数だけ確認する。
    if (!chain_.nodeIndices.empty()) return chain_.nodeIndices.size() >= 2;

    const std::shared_ptr<GltfModelCommponennt> model_ = model.lock();
    if (!model_) return false;

    const int rootIndex_ = model_->GetNodeIndex(chain_.rootNodeName.c_str());
    const int tipIndex_ = model_->GetNodeIndex(chain_.tipNodeName.c_str());
    if (rootIndex_ < 0 || tipIndex_ < 0) return false;

    const std::vector<GltfModelCommponennt::Node>& nodes_ = model_->GetNodes();

    std::vector<int> reverseIndices_;

    // Tipから親を辿り、RootまでのNodeIndexを逆順で集める。
    for (int currentIndex_ = tipIndex_; currentIndex_ >= 0;
        currentIndex_ = nodes_.at(currentIndex_).parentIndex)
    {
        reverseIndices_.push_back(currentIndex_);
        if (currentIndex_ == rootIndex_) break;
    }

    if (reverseIndices_.empty() || reverseIndices_.back() != rootIndex_) return false;

    // RootからTipの順番になるように格納する。
    chain_.nodeIndices.assign(reverseIndices_.rbegin(), reverseIndices_.rend());
    return chain_.nodeIndices.size() >= 2;
}

// 指定チェーンの各ボーンをターゲット方向へ回転させる。
void HumanLookAtIKComponent::ApplyLookChain(
    IKMapComponent::IKChain& chain_,
    LookRuntimeState& state_,
    const XMFLOAT3& targetPosition_,
    float deltaTime_,
    LOOK_MODE lookMode_)
{
    (void)deltaTime_;

    const std::shared_ptr<GltfModelCommponennt> model_ = model.lock();
    if (!model_) return;

    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_ || !owner_->transform) return;

    // キャラクター基準のRight/Up/Forwardを取得する。
    const XMMATRIX ownerWorld_ = owner_->transform->GetWorldMatrix();
    const XMVECTOR ownerRight_ = XMVector3Normalize(ownerWorld_.r[0]);
    const XMVECTOR ownerUp_ = XMVector3Normalize(ownerWorld_.r[1]);
    const XMVECTOR ownerForward_ = XMVector3Normalize(ownerWorld_.r[2]);

    std::vector<GltfModelCommponennt::Node>& nodes_ = model_->GetNodes();

    // 各ボーンのローカル正面軸を初回だけ保存する。
    if (state_.localForwardAxes.size() != chain_.nodeIndices.size())
    {
        state_.localForwardAxes.clear();
        state_.localForwardAxes.reserve(chain_.nodeIndices.size());

        for (const int nodeIndex_ : chain_.nodeIndices)
        {
            const XMMATRIX nodeWorld_ = XMLoadFloat4x4(&nodes_.at(nodeIndex_).worldTransform);

            const XMVECTOR localForward_ = XMVector3Normalize(
                XMVector3TransformNormal(ownerForward_, XMMatrixInverse(nullptr, nodeWorld_)));

            XMFLOAT3 localForwardStorage_;
            XMStoreFloat3(&localForwardStorage_, localForward_);
            state_.localForwardAxes.push_back(localForwardStorage_);
        }
    }

    bool chainChanged_ = false;

    // HEAD_ONLY時は末端側のボーンだけを使う。
    const size_t firstPointIndex_ = lookMode_ == LOOK_MODE::HEAD_ONLY && chain_.nodeIndices.size() > 2
        ? chain_.nodeIndices.size() - 2
        : 0;

    for (size_t pointIndex_ = firstPointIndex_; pointIndex_ < chain_.nodeIndices.size(); ++pointIndex_)
    {
        // 現在ボーンのWorld姿勢を最新化する。
        model_->UpdateNodeTransformOnly(chain_.nodeIndices[pointIndex_]);

        GltfModelCommponennt::Node& node_ = nodes_.at(chain_.nodeIndices[pointIndex_]);

        const XMFLOAT3 nodePosition_ = GetTranslation(node_.worldTransform);
        const XMFLOAT3 targetDifference_ = Normalize(Subtract(targetPosition_, nodePosition_));
        if (Length(targetDifference_) <= EPSILON) continue;

        const XMVECTOR targetDirection_ = XMLoadFloat3(&targetDifference_);

        // ターゲット方向をキャラクター基準のYaw/Pitchに分解する。
        const float forwardAmount_ = XMVectorGetX(XMVector3Dot(targetDirection_, ownerForward_));
        const float rightAmount_ = XMVectorGetX(XMVector3Dot(targetDirection_, ownerRight_));
        const float upAmount_ = std::clamp(
            XMVectorGetX(XMVector3Dot(targetDirection_, ownerUp_)),
            -1.0f,
            1.0f);

        float yaw_ = std::atan2(rightAmount_, forwardAmount_);
        float pitch_ = std::asin(upAmount_);

        const float maxYawDegrees_ = lookMode_ == LOOK_MODE::HEAD_ONLY
            ? headOnlyMaxYawDegrees
            : chain_.lookMaxYawDegrees;

        const float maxPitchDegrees_ = lookMode_ == LOOK_MODE::HEAD_ONLY
            ? headOnlyMaxPitchDegrees
            : chain_.lookMaxPitchDegrees;

        yaw_ = std::clamp(
            yaw_,
            -XMConvertToRadians((std::max)(maxYawDegrees_, 0.0f)),
            XMConvertToRadians((std::max)(maxYawDegrees_, 0.0f)));

        pitch_ = std::clamp(
            pitch_,
            -XMConvertToRadians((std::max)(maxPitchDegrees_, 0.0f)),
            XMConvertToRadians((std::max)(maxPitchDegrees_, 0.0f)));

        // チェーン内でのRootからTipまでの正規化位置。
        const float normalizedPoint_ = chain_.nodeIndices.size() <= 1
            ? 1.0f
            : static_cast<float>(pointIndex_)
            / static_cast<float>(chain_.nodeIndices.size() - 1);

        // Root側ほど肩方向の補正を強くする。
        const float shieldBias_ = XMConvertToRadians(chain_.lookShieldShoulderYawDegrees)
            * (1.0f - normalizedPoint_);

        const float biasedYaw_ = yaw_ + shieldBias_;
        const float cosPitch_ = std::cos(pitch_);

        // 制限済みYaw/Pitchから目標方向ベクトルを作る。
        const XMVECTOR desiredDirection_ = XMVector3Normalize(
            XMVectorAdd(
                XMVectorAdd(
                    XMVectorScale(ownerForward_, std::cos(biasedYaw_) * cosPitch_),
                    XMVectorScale(ownerRight_, std::sin(biasedYaw_) * cosPitch_)),
                XMVectorScale(ownerUp_, std::sin(pitch_))));

        // 保存済みのローカル正面軸を取得する。
        XMFLOAT3 localForwardAxis_ = Normalize(state_.localForwardAxes[pointIndex_]);

        if (Length(localForwardAxis_) <= EPSILON)
        {
            localForwardAxis_ = Normalize(chain_.lookForwardAxis);
        }

        if (Length(localForwardAxis_) <= EPSILON)
        {
            localForwardAxis_ = { 0.0f, 0.0f, 1.0f };
        }

        const XMMATRIX nodeWorld_ = XMLoadFloat4x4(&node_.worldTransform);

        // 現在のボーン正面方向をWorld空間へ変換する。
        const XMVECTOR currentForward_ = XMVector3Normalize(
            XMVector3TransformNormal(XMLoadFloat3(&localForwardAxis_), nodeWorld_));

        // 現在正面から目標方向へ回すための回転軸を求める。
        XMVECTOR axis_ = XMVector3Cross(currentForward_, desiredDirection_);
        if (XMVectorGetX(XMVector3LengthSq(axis_)) <= EPSILON) continue;
        axis_ = XMVector3Normalize(axis_);

        const float dot_ = std::clamp(
            XMVectorGetX(XMVector3Dot(currentForward_, desiredDirection_)),
            -1.0f,
            1.0f);

        // RootからTipへ向けて重みを補間する。
        float chainWeight_ = chain_.lookRootWeight
            + (chain_.lookTipWeight - chain_.lookRootWeight) * normalizedPoint_;

        const size_t remainingNodeCount_ = chain_.nodeIndices.size() - pointIndex_;

        if (remainingNodeCount_ == 2)
        {
            // 首は強めに追従させる。
            chainWeight_ = (std::max)(chainWeight_, 0.85f);
        }
        else if (remainingNodeCount_ == 1)
        {
            // 頭は最後に残った角度をすべて受け持つ。
            // HEAD_ONLY時は頭だけで回し切らないように少し抑える。
            chainWeight_ = lookMode_ == LOOK_MODE::HEAD_ONLY
                ? 0.75f
                : 1.0f;

        }

        // 最終的に適用する回転角度を計算する。
        const float angle_ = std::acos(dot_)
            * std::clamp(chainWeight_, 0.0f, 1.0f)
            * std::clamp(chain_.influence, 0.0f, 1.0f)
            * state_.activeWeight;

        XMMATRIX nodeWorldRotation_ = nodeWorld_;
        nodeWorldRotation_.r[3] = XMVectorSet(0, 0, 0, 1);

        XMMATRIX parentWorldRotation_ = ownerWorld_;
        if (node_.parent)
        {
            parentWorldRotation_ = XMLoadFloat4x4(&node_.parent->worldTransform);
        }
        parentWorldRotation_.r[3] = XMVectorSet(0, 0, 0, 1);

        // World回転結果を親基準のLocal回転へ戻す。
        const XMMATRIX desiredWorldRotation_ = nodeWorldRotation_
            * XMMatrixRotationAxis(axis_, angle_);

        const XMMATRIX desiredLocalRotation_ = desiredWorldRotation_
            * XMMatrixInverse(nullptr, parentWorldRotation_);

        XMVECTOR scale_;
        XMVECTOR rotation_;
        XMVECTOR translation_;

        // Local回転だけをNodeへ反映する。
        if (XMMatrixDecompose(&scale_, &rotation_, &translation_, desiredLocalRotation_))
        {
            XMStoreFloat4(&node_.rotation, XMQuaternionNormalize(rotation_));
            model_->UpdateNodeTransformOnly(chain_.nodeIndices[pointIndex_]);
            chainChanged_ = true;
        }
    }

    // チェーン変更後、Rootから子階層のWorldTransformを更新する。
    if (chainChanged_)
    {
        model_->UpdateNodeTransformFrom(chain_.nodeIndices.front());
    }
}
// LookAtチェーンとターゲット方向をデバッグ描画する。
void HumanLookAtIKComponent::DrawDebugShapes(
    const IKMapComponent::IKChain& chain_,
    const XMFLOAT3& targetPosition_) const
{
    if (!showDebugShapes || chain_.nodeIndices.size() < 2) return;

    const std::shared_ptr<GltfModelCommponennt> model_ = model.lock();
    if (!model_) return;

    ShapeRenderSystem& renderer_ = GraphicsDeviceManager::Instance().GetShapeRendererSystem();
    const std::vector<GltfModelCommponennt::Node>& nodes_ = model_->GetNodes();

    // チェーン内のボーン同士を線でつなぐ。
    for (size_t pointIndex_ = 0; pointIndex_ + 1 < chain_.nodeIndices.size(); ++pointIndex_)
    {
        DrawBoneBetween(
            renderer_,
            GetTranslation(nodes_.at(chain_.nodeIndices[pointIndex_]).worldTransform),
            GetTranslation(nodes_.at(chain_.nodeIndices[pointIndex_ + 1]).worldTransform),
            debugChainColor);
    }

    // チェーン末端からターゲット位置までを線でつなぐ。
    DrawBoneBetween(
        renderer_,
        GetTranslation(nodes_.at(chain_.nodeIndices.back()).worldTransform),
        targetPosition_,
        debugTargetColor);
}

// LookAt IKの編集UIを表示する。
void HumanLookAtIKComponent::Gui()
{
    ResolveDependencies();

    ImGui::Checkbox("Look Enabled", &lookEnabled);
    ImGui::Checkbox("Use LockOn Target", &useLockOnTarget);
    ImGui::Checkbox("Idle Only", &idleOnly);
    ImGui::Checkbox("Show Debug Shapes", &showDebugShapes);

    char targetNameBuffer_[128]{};
    strncpy_s(targetNameBuffer_, targetName.c_str(), sizeof(targetNameBuffer_));
    if (ImGui::InputText("Target Object", targetNameBuffer_, sizeof(targetNameBuffer_)))
    {
        SetTargetName(targetNameBuffer_);
    }

    char idleStateBuffer_[128]{};
    strncpy_s(idleStateBuffer_, idleStateName.c_str(), sizeof(idleStateBuffer_));
    if (ImGui::InputText("Idle State", idleStateBuffer_, sizeof(idleStateBuffer_)))
    {
        idleStateName = idleStateBuffer_;
    }

    ImGui::DragFloat3("Target Offset", &targetOffset.x, 0.01f);

    const std::shared_ptr<AnimationControllerComponent> animationController_ = animationController.lock();

    // AnimationStateごとのLookModeを編集する。
    if (animationController_ && ImGui::CollapsingHeader("Animation Look Modes", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::TextDisabled("New and unconfigured states default to Head Only.");

        std::vector<std::string> stateNames_;
        stateNames_.reserve(animationController_->GetStates().size());

        for (const auto& [stateName_, state_] : animationController_->GetStates())
        {
            if (state_) stateNames_.push_back(stateName_);
        }

        std::sort(stateNames_.begin(), stateNames_.end());

        for (const std::string& stateName_ : stateNames_)
        {
            LOOK_MODE& mode_ = stateLookModes.try_emplace(stateName_, LOOK_MODE::HEAD_ONLY).first->second;
            int selectedMode_ = static_cast<int>(mode_);

            ImGui::PushID(stateName_.c_str());
            ImGui::TextUnformatted(stateName_.c_str());
            ImGui::SameLine(220.0f);
            ImGui::SetNextItemWidth(130.0f);

            const char* modeNames_[] = { "Head Only", "Full Body" };
            if (ImGui::Combo("##LookMode", &selectedMode_, modeNames_, IM_ARRAYSIZE(modeNames_)))
            {
                mode_ = static_cast<LOOK_MODE>(selectedMode_);
            }

            ImGui::PopID();
        }
    }

    if (showDebugShapes)
    {
        ImGui::ColorEdit4("Look Chain Color", &debugChainColor.x);
        ImGui::ColorEdit4("Look Target Color", &debugTargetColor.x);
    }

    ImGui::TextDisabled("Look chain bones and limits are configured in IKMapComponent.");
}

// LookAt IKの設定をJsonへ保存する。
void HumanLookAtIKComponent::Serialize(nlohmann::json& json_) const
{
    json_["LookEnabled"] = lookEnabled;
    json_["UseLockOnTarget"] = useLockOnTarget;
    json_["IdleOnly"] = idleOnly;
    json_["IdleStateName"] = idleStateName;
    json_["TargetName"] = targetName;
    json_["TargetOffset"] = { targetOffset.x, targetOffset.y, targetOffset.z };

    nlohmann::json stateLookModesJson_ = nlohmann::json::object();

    // State名ごとのLookModeをint値で保存する。
    for (const auto& [stateName_, mode_] : stateLookModes)
    {
        stateLookModesJson_[stateName_] = static_cast<int>(mode_);
    }

    json_["StateLookModes"] = std::move(stateLookModesJson_);
    json_["ShowDebugShapes"] = showDebugShapes;
    json_["DebugChainColor"] = { debugChainColor.x, debugChainColor.y, debugChainColor.z, debugChainColor.w };
    json_["DebugTargetColor"] = { debugTargetColor.x, debugTargetColor.y, debugTargetColor.z, debugTargetColor.w };
}

// JsonからLookAt IKの設定を復元する。
void HumanLookAtIKComponent::Deserialize(const nlohmann::json& json_)
{
    lookEnabled = json_.value("LookEnabled", true);
    useLockOnTarget = json_.value("UseLockOnTarget", true);
    idleOnly = json_.value("IdleOnly", true);
    idleStateName = json_.value("IdleStateName", std::string("SS_Idle"));
    targetName = json_.value("TargetName", std::string("TestTarget"));

    if (json_.contains("TargetOffset"))
    {
        targetOffset = {
            json_["TargetOffset"][0],
            json_["TargetOffset"][1],
            json_["TargetOffset"][2]
        };
    }

    stateLookModes.clear();

    // 保存済みのState別LookModeを復元する。
    if (json_.contains("StateLookModes") && json_["StateLookModes"].is_object())
    {
        for (const auto& [stateName_, modeJson_] : json_["StateLookModes"].items())
        {
            const int modeValue_ = modeJson_.get<int>();
            stateLookModes[stateName_] = modeValue_ == static_cast<int>(LOOK_MODE::FULL_BODY)
                ? LOOK_MODE::FULL_BODY
                : LOOK_MODE::HEAD_ONLY;
        }
    }

    showDebugShapes = json_.value("ShowDebugShapes", false);

    if (json_.contains("DebugChainColor"))
    {
        debugChainColor = {
            json_["DebugChainColor"][0],
            json_["DebugChainColor"][1],
            json_["DebugChainColor"][2],
            json_["DebugChainColor"][3]
        };
    }

    if (json_.contains("DebugTargetColor"))
    {
        debugTargetColor = {
            json_["DebugTargetColor"][0],
            json_["DebugTargetColor"][1],
            json_["DebugTargetColor"][2],
            json_["DebugTargetColor"][3]
        };
    }

    // 復元後は実行時参照と補間状態を作り直す。
    cameraLockOn.reset();
    target.reset();
    lookStates.clear();
}


REGISTER_COMPONENT(HumanLookAtIKComponent, "HumanLookAtIKComponent")

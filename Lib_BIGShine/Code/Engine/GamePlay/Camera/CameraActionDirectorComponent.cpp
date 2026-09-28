#include "PrecompiledHeader.h"

#include "CameraActionDirectorComponent.h"

#include "CameraCollisionComponent.h"
#include "CameraComponent.h"
#include "CameraLockOnComponent.h"
#include "ThirdPersonCameraComponent.h"
#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Serialize/ActionRegistry.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
#include "Engine/GamePlay/Transform/Transform.h"
#include "Engine/Debug/TracyProfiler.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
    // 対象の拡大率を除外し、Z+を正面とする位置・回転だけの座標系を作る。
    bool BuildFollowFrame(const std::shared_ptr<GameObject>& target_, DirectX::XMFLOAT4X4& frame_)
    {
        if (!target_ || !target_->transform) return false;
        DirectX::XMVECTOR scale_, rotation_, position_;
        if (!DirectX::XMMatrixDecompose(&scale_, &rotation_, &position_, target_->transform->GetWorldMatrix())) return false;
        DirectX::XMStoreFloat4x4(&frame_, DirectX::XMMatrixRotationQuaternion(rotation_) *
            DirectX::XMMatrixTranslationFromVector(position_));
        return true;
    }

    // 保存した対象ローカルの構図を、現在の対象の位置と回転でワールド空間へ戻す。
    bool ResolveFollowPose(const CameraActionPreset& preset_, const std::shared_ptr<GameObject>& target_,
        DirectX::XMFLOAT3& eye_, DirectX::XMFLOAT3& focus_, DirectX::XMFLOAT3& up_)
    {
        BS_PROFILE_SCOPE("CameraAction::ResolveFollowPose");
        DirectX::XMFLOAT4X4 frame_;
        if (!BuildFollowFrame(target_, frame_)) return false;
        const auto matrix_ = DirectX::XMLoadFloat4x4(&frame_);
        DirectX::XMStoreFloat3(&eye_, DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&preset_.cameraOffset), matrix_));
        DirectX::XMStoreFloat3(&focus_, DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&preset_.focusOffset), matrix_));
        DirectX::XMStoreFloat3(&up_, DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&preset_.cameraUp), matrix_));
        return true;
    }

    const char* ToString(CAMERA_ACTION_PHASE phase_)
    {
        switch (phase_)
        {
        case CAMERA_ACTION_PHASE::ENTER: return "ENTER";
        case CAMERA_ACTION_PHASE::HOLD: return "HOLD";
        case CAMERA_ACTION_PHASE::RETURN: return "RETURN";
        case CAMERA_ACTION_PHASE::IDLE:
        default:
            return "IDLE";
        }
    }

    const char* ToString(CAMERA_ACTION_TARGET_MODE mode_)
    {
        switch (mode_)
        {
        case CAMERA_ACTION_TARGET_MODE::GAME_OBJECT: return "GAME_OBJECT";
        case CAMERA_ACTION_TARGET_MODE::GAME_OBJECT_OFFSET: return "GAME_OBJECT_OFFSET";
        case CAMERA_ACTION_TARGET_MODE::HIT_POSITION: return "HIT_POSITION";
        case CAMERA_ACTION_TARGET_MODE::PLAYER: return "PLAYER";
        case CAMERA_ACTION_TARGET_MODE::LOCK_ON_TARGET: return "LOCK_ON_TARGET";
        case CAMERA_ACTION_TARGET_MODE::WORLD_POSITION:
        default:
            return "WORLD_POSITION";
        }
    }

    const char* ToString(CAMERA_ACTION_POSITION_MODE mode_)
    {
        switch (mode_)
        {
        case CAMERA_ACTION_POSITION_MODE::TARGET_LOCAL_RELATIVE: return "TARGET_LOCAL_RELATIVE";
        case CAMERA_ACTION_POSITION_MODE::PLAYER_FORWARD_RELATIVE: return "PLAYER_FORWARD_RELATIVE";
        case CAMERA_ACTION_POSITION_MODE::TPS_VIEW_RELATIVE: return "TPS_VIEW_RELATIVE";
        case CAMERA_ACTION_POSITION_MODE::WORLD_POSITION: return "WORLD_POSITION";
        case CAMERA_ACTION_POSITION_MODE::TPS_TARGET_RELATIVE: return "TPS_TARGET_RELATIVE";
        case CAMERA_ACTION_POSITION_MODE::CURRENT_CAMERA_RELATIVE:
        default:
            return "CURRENT_CAMERA_RELATIVE";
        }
    }

    CAMERA_ACTION_TARGET_MODE TargetModeFromString(const std::string& value_)
    {
        if (value_ == "GAME_OBJECT") return CAMERA_ACTION_TARGET_MODE::GAME_OBJECT;
        if (value_ == "GAME_OBJECT_OFFSET") return CAMERA_ACTION_TARGET_MODE::GAME_OBJECT_OFFSET;
        if (value_ == "HIT_POSITION") return CAMERA_ACTION_TARGET_MODE::HIT_POSITION;
        if (value_ == "PLAYER") return CAMERA_ACTION_TARGET_MODE::PLAYER;
        if (value_ == "LOCK_ON_TARGET") return CAMERA_ACTION_TARGET_MODE::LOCK_ON_TARGET;
        return CAMERA_ACTION_TARGET_MODE::WORLD_POSITION;
    }

    CAMERA_ACTION_POSITION_MODE PositionModeFromString(const std::string& value_)
    {
        if (value_ == "TARGET_LOCAL_RELATIVE") return CAMERA_ACTION_POSITION_MODE::TARGET_LOCAL_RELATIVE;
        if (value_ == "PLAYER_FORWARD_RELATIVE") return CAMERA_ACTION_POSITION_MODE::PLAYER_FORWARD_RELATIVE;
        if (value_ == "TPS_VIEW_RELATIVE") return CAMERA_ACTION_POSITION_MODE::TPS_VIEW_RELATIVE;
        if (value_ == "WORLD_POSITION") return CAMERA_ACTION_POSITION_MODE::WORLD_POSITION;
        if (value_ == "TPS_TARGET_RELATIVE") return CAMERA_ACTION_POSITION_MODE::TPS_TARGET_RELATIVE;
        return CAMERA_ACTION_POSITION_MODE::CURRENT_CAMERA_RELATIVE;
    }

    DirectX::XMFLOAT3 AddFloat3(
        const DirectX::XMFLOAT3& left_,
        const DirectX::XMFLOAT3& right_)
    {
        return { left_.x + right_.x, left_.y + right_.y, left_.z + right_.z };
    }

    DirectX::XMFLOAT3 SubtractFloat3(
        const DirectX::XMFLOAT3& left_,
        const DirectX::XMFLOAT3& right_)
    {
        return { left_.x - right_.x, left_.y - right_.y, left_.z - right_.z };
    }

    DirectX::XMFLOAT3 ScaleFloat3(const DirectX::XMFLOAT3& value_, float scale_)
    {
        return { value_.x * scale_, value_.y * scale_, value_.z * scale_ };
    }

    DirectX::XMFLOAT3 LerpFloat3(
        const DirectX::XMFLOAT3& start_,
        const DirectX::XMFLOAT3& end_,
        float rate_)
    {
        return
        {
            start_.x + (end_.x - start_.x) * rate_,
            start_.y + (end_.y - start_.y) * rate_,
            start_.z + (end_.z - start_.z) * rate_
        };
    }

    // 対象を中心とする水平円弧で移動し、反対側への直線補間が対象を横切るのを防ぐ。
    DirectX::XMFLOAT3 OrbitAroundTarget(
        const DirectX::XMFLOAT3& start_, const DirectX::XMFLOAT3& end_,
        const DirectX::XMFLOAT3& center_, float rate_)
    {
        const auto from_ = SubtractFloat3(start_, center_);
        const auto to_ = SubtractFloat3(end_, center_);
        const float startRadius_ = std::sqrt(from_.x * from_.x + from_.z * from_.z);
        const float endRadius_ = std::sqrt(to_.x * to_.x + to_.z * to_.z);
        const float startAngle_ = startRadius_ > 0.0001f ? std::atan2(from_.x, from_.z) : std::atan2(to_.x, to_.z);
        const float endAngle_ = endRadius_ > 0.0001f ? std::atan2(to_.x, to_.z) : startAngle_;
        // 最短側へ回り込み、始点・終点より内側へ距離が縮む弦の経路を避ける。
        const float angle_ = startAngle_ + std::remainder(endAngle_ - startAngle_, DirectX::XM_2PI) * rate_;
        const float radius_ = startRadius_ + (endRadius_ - startRadius_) * rate_;
        return { center_.x + std::sin(angle_) * radius_, start_.y + (end_.y - start_.y) * rate_,
            center_.z + std::cos(angle_) * radius_ };
    }

    DirectX::XMFLOAT3 NormalizeOr(
        const DirectX::XMFLOAT3& value_,
        const DirectX::XMFLOAT3& fallback_)
    {
        const float length_ = std::sqrt(
            value_.x * value_.x +
            value_.y * value_.y +
            value_.z * value_.z);
        if (length_ <= 0.0001f) return fallback_;
        return { value_.x / length_, value_.y / length_, value_.z / length_ };
    }

    float LengthFloat3(const DirectX::XMFLOAT3& value_)
    {
        return std::sqrt(
            value_.x * value_.x +
            value_.y * value_.y +
            value_.z * value_.z);
    }

    DirectX::XMFLOAT3 CrossFloat3(
        const DirectX::XMFLOAT3& left_,
        const DirectX::XMFLOAT3& right_)
    {
        DirectX::XMFLOAT3 result_{};
        DirectX::XMStoreFloat3(
            &result_,
            DirectX::XMVector3Cross(
                DirectX::XMLoadFloat3(&left_),
                DirectX::XMLoadFloat3(&right_)));
        return result_;
    }

    DirectX::XMFLOAT3 LoadFloat3(
        const nlohmann::json& json_,
        const char* key_,
        const DirectX::XMFLOAT3& defaultValue_)
    {
        if (!json_.contains(key_) ||
            !json_[key_].is_array() ||
            json_[key_].size() < 3)
        {
            return defaultValue_;
        }
        return
        {
            json_[key_][0].get<float>(),
            json_[key_][1].get<float>(),
            json_[key_][2].get<float>()
        };
    }
}

void CameraActionDirectorComponent::Start()
{
    ResolveReferences();
    if (presets.empty())
    {
        // JSON未設定でも戦闘確認ができるよう、ジャストガード/回避の標準プリセットを用意する。
        CameraActionPreset justGuard_;
        justGuard_.actionName = "JustGuard";
        justGuard_.targetMode = CAMERA_ACTION_TARGET_MODE::HIT_POSITION;
        justGuard_.positionMode = CAMERA_ACTION_POSITION_MODE::TPS_VIEW_RELATIVE;
        justGuard_.cameraOffset = { 0.6f, 0.4f, 1.0f };
        justGuard_.focusOffset = { 0.0f, 0.15f, 0.0f };
        justGuard_.targetFov = 42.0f;
        justGuard_.shakeStrength = 0.035f;
        justGuard_.priority = 20;
        justGuard_.blackboardCategory = "Guard";
        justGuard_.focusPositionKey = "Position";
        justGuard_.attackDirectionKey = "Normal";
        presets.push_back(justGuard_);

        CameraActionPreset justDodge_;
        justDodge_.actionName = "JustDodge";
        justDodge_.targetMode = CAMERA_ACTION_TARGET_MODE::PLAYER;
        justDodge_.positionMode = CAMERA_ACTION_POSITION_MODE::PLAYER_FORWARD_RELATIVE;
        justDodge_.cameraOffset = { 0.7f, 0.45f, -1.4f };
        justDodge_.focusOffset = { 0.0f, 1.1f, 0.0f };
        justDodge_.enterDuration = 0.06f;
        justDodge_.holdDuration = 0.10f;
        justDodge_.returnDuration = 0.16f;
        justDodge_.targetFov = 50.0f;
        justDodge_.shakeStrength = 0.015f;
        justDodge_.priority = 15;
        justDodge_.blackboardCategory = "Dodge";
        justDodge_.focusPositionKey = "Position";
        justDodge_.attackDirectionKey = "Normal";
        presets.push_back(justDodge_);
    }
    RegisterActions();
}

void CameraActionDirectorComponent::LateUpdate(float deltaTime_)
{
    BS_PROFILE_SCOPE("CameraActionDirectorComponent::LateUpdate");
    ResolveReferences();
    if (phase == CAMERA_ACTION_PHASE::IDLE) return;

    // ENTER/HOLD/RETURNの3段階で通常カメラへ上書きし、終了時に自然に元の制御へ戻す。
    const std::shared_ptr<CameraComponent> camera_ = camera.lock();
    if (!camera_)
    {
        StopAction(true);
        return;
    }

    // 演出中も同じ対象の現在姿勢から復元し、移動や旋回で構図が取り残されるのを防ぐ。
    if (activePreset.positionMode == CAMERA_ACTION_POSITION_MODE::TPS_TARGET_RELATIVE &&
        phase != CAMERA_ACTION_PHASE::RETURN &&
        !ResolveFollowPose(activePreset, activeFollowTarget.lock(), actionCameraPosition, actionFocusPosition, actionCameraUp))
    {
        StopAction(false);
        return;
    }
    phaseTimer += deltaTime_;
    float duration_ = GetPhaseDuration();
    const float rate_ = duration_ <= 0.0001f
        ? 1.0f
        : SmoothStep(phaseTimer / duration_);

    DirectX::XMFLOAT3 cameraPosition_ = actionCameraPosition;
    DirectX::XMFLOAT3 focusPosition_ = actionFocusPosition;
    float fov_ = activeFov;
    DirectX::XMFLOAT3 up_ = actionCameraUp;
    const auto followTarget_ = activeFollowTarget.lock();
    const bool useOrbit_ = activePreset.positionMode == CAMERA_ACTION_POSITION_MODE::TPS_TARGET_RELATIVE &&
        followTarget_ && followTarget_->transform;
    const auto orbitCenter_ = useOrbit_ ? followTarget_->transform->GetWorldPosition() : DirectX::XMFLOAT3{};

    if (phase == CAMERA_ACTION_PHASE::ENTER)
    {
        cameraPosition_ = LerpFloat3(startCameraPosition, actionCameraPosition, rate_);
        if (useOrbit_) cameraPosition_ = OrbitAroundTarget(startCameraPosition, actionCameraPosition, orbitCenter_, rate_);
        focusPosition_ = LerpFloat3(startFocusPosition, actionFocusPosition, rate_);
        fov_ = startFov + (activeFov - startFov) * rate_;
        if (activePreset.positionMode == CAMERA_ACTION_POSITION_MODE::TPS_TARGET_RELATIVE)
            up_ = NormalizeOr(LerpFloat3(startCameraUp, actionCameraUp, rate_), actionCameraUp);
    }
    else if (phase == CAMERA_ACTION_PHASE::RETURN)
    {
        const DirectX::XMFLOAT3 normalCamera_ = camera_->GetEye();
        const DirectX::XMFLOAT3 normalFocus_ = camera_->GetFocus();
        const float normalFov_ = camera_->GetFovY();
        cameraPosition_ = LerpFloat3(returnStartCameraPosition, normalCamera_, rate_);
        if (useOrbit_) cameraPosition_ = OrbitAroundTarget(returnStartCameraPosition, normalCamera_, orbitCenter_, rate_);
        focusPosition_ = LerpFloat3(returnStartFocusPosition, normalFocus_, rate_);
        fov_ = returnStartFov + (normalFov_ - returnStartFov) * rate_;
        if (activePreset.positionMode == CAMERA_ACTION_POSITION_MODE::TPS_TARGET_RELATIVE)
            up_ = NormalizeOr(LerpFloat3(returnStartCameraUp, camera_->GetUp(), rate_), camera_->GetUp());
    }

    if (activePreset.shakeStrength > 0.0001f)
    {
        const float shake_ =
            std::sin(phaseTimer * activePreset.shakeFrequency * 6.2831853f) *
            activePreset.shakeStrength *
            (1.0f - (phase == CAMERA_ACTION_PHASE::RETURN ? rate_ : 0.0f));
        cameraPosition_.x += shake_;
        focusPosition_.y += shake_ * 0.35f;
    }

    ApplyCameraPose(cameraPosition_, focusPosition_, fov_, up_);

    if (phaseTimer < duration_) return;

    if (phase == CAMERA_ACTION_PHASE::ENTER)
    {
        phase = CAMERA_ACTION_PHASE::HOLD;
        phaseTimer = 0.0f;
        return;
    }

    if (phase == CAMERA_ACTION_PHASE::HOLD)
    {
        returnStartCameraPosition = camera_->GetEye();
        returnStartFocusPosition = camera_->GetFocus();
        returnStartFov = camera_->GetFovY();
        returnStartCameraUp = camera_->GetUp();
        ReleaseSuppression();
        phase = CAMERA_ACTION_PHASE::RETURN;
        phaseTimer = 0.0f;
        return;
    }

    StopAction(true);
}

void CameraActionDirectorComponent::PlayActionByIndex(int index_)
{
    if (index_ < 0 || index_ >= static_cast<int>(presets.size())) return;

    DirectX::XMFLOAT3 focusPosition_{};
    DirectX::XMFLOAT3 attackDirection_{ 0.0f, 0.0f, 0.0f };
    if (!ResolvePresetFocus(presets[index_], focusPosition_, attackDirection_))
    {
        return;
    }
    BeginAction(presets[index_], focusPosition_, attackDirection_, nullptr);
}

void CameraActionDirectorComponent::PlayActionByName(
    const std::string& actionName_)
{
    PlayActionByIndex(FindPresetIndex(actionName_));
}

void CameraActionDirectorComponent::PlayActionAtPosition(
    const std::string& actionName_,
    const DirectX::XMFLOAT3& focusPosition_)
{
    PlayActionAtPosition(
        actionName_,
        focusPosition_,
        { 0.0f, 0.0f, 0.0f });
}

void CameraActionDirectorComponent::PlayActionAtPosition(
    const std::string& actionName_,
    const DirectX::XMFLOAT3& focusPosition_,
    const DirectX::XMFLOAT3& attackDirection_)
{
    const int index_ = FindPresetIndex(actionName_);
    if (index_ < 0) return;
    BeginAction(presets[index_], focusPosition_, attackDirection_, nullptr);
}

void CameraActionDirectorComponent::PlayActionForTarget(
    const std::string& actionName_,
    const std::shared_ptr<GameObject>& target_)
{
    const int index_ = FindPresetIndex(actionName_);
    if (index_ < 0 || !target_ || !target_->transform) return;
    BeginAction(
        presets[index_],
        target_->transform->GetWorldPosition(),
        { 0.0f, 0.0f, 0.0f },
        target_);
}

void CameraActionDirectorComponent::StopAction(bool immediate_)
{
    if (immediate_)
    {
        phase = CAMERA_ACTION_PHASE::IDLE;
        phaseTimer = 0.0f;
        activePriority = -1;
        activeFollowTarget.reset();
        ReleaseSuppression();
        return;
    }

    if (phase == CAMERA_ACTION_PHASE::IDLE) return;

    const std::shared_ptr<CameraComponent> camera_ = camera.lock();
    returnStartCameraPosition = camera_ ? camera_->GetEye() : actionCameraPosition;
    returnStartFocusPosition = camera_ ? camera_->GetFocus() : actionFocusPosition;
    returnStartFov = camera_ ? camera_->GetFovY() : activeFov;
    returnStartCameraUp = camera_ ? camera_->GetUp() : actionCameraUp;
    phase = CAMERA_ACTION_PHASE::RETURN;
    phaseTimer = 0.0f;
    ReleaseSuppression();
}

void CameraActionDirectorComponent::RegisterActions()
{
    ActionRegistry& registry_ = ActionRegistry::Instance();
    for (const CameraActionPreset& preset_ : presets)
    {
        registry_.RemoveAction("CameraAction", preset_.actionName);
        registry_.RegisterAction("CameraAction", preset_.actionName, [this, actionName_ = preset_.actionName]()
        {
            PlayActionByName(actionName_);
        });
    }
}

void CameraActionDirectorComponent::ResolveReferences()
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;

    if (camera.expired()) camera = owner_->GetComponent<CameraComponent>();
    if (thirdPersonCamera.expired())
    {
        thirdPersonCamera = owner_->GetComponent<ThirdPersonCameraComponent>();
    }
    if (lockOnCamera.expired())
    {
        lockOnCamera = owner_->GetComponent<CameraLockOnComponent>();
    }
    if (collisionCamera.expired())
    {
        collisionCamera = owner_->GetComponent<CameraCollisionComponent>();
    }
    if (blackboard.expired())
    {
        blackboard = owner_->GetComponent<BlackboardComponent>();
    }
}

void CameraActionDirectorComponent::BeginAction(
    const CameraActionPreset& preset_,
    const DirectX::XMFLOAT3& focusPosition_,
    const DirectX::XMFLOAT3& attackDirection_,
    const std::shared_ptr<GameObject>& target_)
{
    ResolveReferences();
    const std::shared_ptr<CameraComponent> camera_ = camera.lock();
    if (!camera_) return;

    // 低優先度の演出で、再生中の重要なカメラ演出を上書きしないようにする。
    if (phase != CAMERA_ACTION_PHASE::IDLE &&
        preset_.priority < activePriority)
    {
        return;
    }

    // 追従対象が取得できない場合は、再生中の演出や通常カメラを変更しない。
    std::shared_ptr<GameObject> followTarget_;
    DirectX::XMFLOAT3 relativeEye_, relativeFocus_, relativeUp_;
    if (preset_.positionMode == CAMERA_ACTION_POSITION_MODE::TPS_TARGET_RELATIVE)
    {
        const auto thirdPerson_ = thirdPersonCamera.lock();
        followTarget_ = thirdPerson_ ? thirdPerson_->GetFollowTarget() : nullptr;
        if (!ResolveFollowPose(preset_, followTarget_, relativeEye_, relativeFocus_, relativeUp_)) return;
    }

    if (phase != CAMERA_ACTION_PHASE::IDLE)
    {
        ReleaseSuppression();
    }

    activePreset = preset_;
    activeTarget = target_;
    activeFollowTarget = followTarget_;
    activePriority = preset_.priority;
    startCameraPosition = camera_->GetEye();
    startFocusPosition = camera_->GetFocus();
    startFov = camera_->GetFovY();
    startCameraUp = camera_->GetUp();
    actionFocusPosition = AddFloat3(focusPosition_, preset_.focusOffset);
    actionCameraPosition = CalculateCameraPosition(
        preset_,
        actionFocusPosition,
        attackDirection_);
    activeFov = ClampFov(preset_.targetFov);
    actionCameraUp = { 0.0f, 1.0f, 0.0f };
    if (preset_.positionMode == CAMERA_ACTION_POSITION_MODE::TPS_TARGET_RELATIVE)
    {
        // 接触位置やTargetModeによる注視点上書きを避け、キャプチャした構図を優先する。
        actionCameraPosition = relativeEye_;
        actionFocusPosition = relativeFocus_;
        actionCameraUp = relativeUp_;
    }

    // 演出中にTPS/LockOn/Collisionが同じCameraComponentを書き換えないよう必要分だけ抑制する。
    if (preset_.suppressThirdPersonControl)
    {
        if (const std::shared_ptr<ThirdPersonCameraComponent> thirdPerson_ =
            thirdPersonCamera.lock())
        {
            thirdPerson_->SetControlSuppressed(true);
            ownsThirdPersonSuppression = true;
        }
    }
    if (preset_.suppressLockOnControl)
    {
        if (const std::shared_ptr<CameraLockOnComponent> lockOn_ =
            lockOnCamera.lock())
        {
            lockOn_->SetActionSuppressed(true);
            ownsLockOnSuppression = true;
        }
    }
    if (preset_.suppressCameraCollision)
    {
        if (const std::shared_ptr<CameraCollisionComponent> collision_ =
            collisionCamera.lock())
        {
            collision_->SetCollisionSuppressed(true);
            ownsCollisionSuppression = true;
        }
    }

    phase = CAMERA_ACTION_PHASE::ENTER;
    phaseTimer = 0.0f;
}

bool CameraActionDirectorComponent::ResolvePresetFocus(
    const CameraActionPreset& preset_,
    DirectX::XMFLOAT3& focusPosition_,
    DirectX::XMFLOAT3& attackDirection_)
{
    // Guard/DodgeなどはSystemがBlackboardへ書いた接触位置を優先し、なければ通常の対象位置へ戻す。
    if (TryReadBlackboardVector3(
        preset_.blackboardCategory,
        preset_.focusPositionKey,
        focusPosition_))
    {
        TryReadBlackboardVector3(
            preset_.blackboardCategory,
            preset_.attackDirectionKey,
            attackDirection_);
        return true;
    }

    focusPosition_ = ResolveTargetPosition(
        preset_,
        camera.lock() ? camera.lock()->GetFocus() : DirectX::XMFLOAT3{});
    return true;
}

bool CameraActionDirectorComponent::TryReadBlackboardVector3(
    const std::string& category_,
    const std::string& key_,
    DirectX::XMFLOAT3& value_) const
{
    const std::shared_ptr<BlackboardComponent> blackboard_ = blackboard.lock();
    if (!blackboard_) return false;

    DirectX::SimpleMath::Vector3 simpleValue_{};
    if (!blackboard_->GetVector3(category_, key_, simpleValue_)) return false;

    value_ = { simpleValue_.x, simpleValue_.y, simpleValue_.z };
    return true;
}

DirectX::XMFLOAT3 CameraActionDirectorComponent::ResolveTargetPosition(
    const CameraActionPreset& preset_,
    const DirectX::XMFLOAT3& fallbackPosition_) const
{
    if (const std::shared_ptr<GameObject> target_ = activeTarget.lock();
        target_ && target_->transform)
    {
        return AddFloat3(target_->transform->GetWorldPosition(), preset_.focusOffset);
    }

    if (preset_.targetMode == CAMERA_ACTION_TARGET_MODE::LOCK_ON_TARGET)
    {
        if (const std::shared_ptr<CameraLockOnComponent> lockOn_ =
            lockOnCamera.lock())
        {
            const std::shared_ptr<GameObject> lockOnTarget_ =
                lockOn_->GetLockOnTarget();
            if (lockOnTarget_ && lockOnTarget_->transform)
            {
                return lockOnTarget_->transform->GetWorldPosition();
            }
        }
    }

    if (preset_.targetMode == CAMERA_ACTION_TARGET_MODE::PLAYER)
    {
        if (const std::shared_ptr<CameraLockOnComponent> lockOn_ =
            lockOnCamera.lock())
        {
            const std::shared_ptr<GameObject> player_ = lockOn_->GetPlayerTarget();
            if (player_ && player_->transform)
            {
                return player_->transform->GetWorldPosition();
            }
        }
    }

    return fallbackPosition_;
}

DirectX::XMFLOAT3 CameraActionDirectorComponent::CalculateCameraPosition(
    const CameraActionPreset& preset_,
    const DirectX::XMFLOAT3& focusPosition_,
    const DirectX::XMFLOAT3& attackDirection_) const
{
    if (preset_.positionMode == CAMERA_ACTION_POSITION_MODE::WORLD_POSITION)
    {
        return preset_.cameraOffset;
    }

    // プリセットのPositionModeごとに、offsetをどの基準軸で解釈するかを切り替える。
    const std::shared_ptr<CameraComponent> camera_ = camera.lock();
    const DirectX::XMFLOAT3 defaultForward_ =
        camera_ ? camera_->GetFront() : DirectX::XMFLOAT3{ 0.0f, 0.0f, 1.0f };
    DirectX::XMFLOAT3 forward_ = defaultForward_;
    DirectX::XMFLOAT3 right_ =
        camera_ ? camera_->GetRight() : DirectX::XMFLOAT3{ 1.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 up_{ 0.0f, 1.0f, 0.0f };

    if (preset_.positionMode == CAMERA_ACTION_POSITION_MODE::TARGET_LOCAL_RELATIVE)
    {
        forward_ = NormalizeOr(attackDirection_, defaultForward_);
        right_ = NormalizeOr(CrossFloat3(up_, forward_), right_);
    }
    else if (preset_.positionMode == CAMERA_ACTION_POSITION_MODE::PLAYER_FORWARD_RELATIVE)
    {
        if (const std::shared_ptr<CameraLockOnComponent> lockOn_ =
            lockOnCamera.lock())
        {
            const std::shared_ptr<GameObject> player_ = lockOn_->GetPlayerTarget();
            if (player_ && player_->transform)
            {
                const DirectX::SimpleMath::Vector3 playerForward_ =
                    player_->transform->GetFront();
                forward_ = NormalizeOr(
                    {
                        playerForward_.x,
                        playerForward_.y,
                        playerForward_.z
                    },
                    defaultForward_);
                right_ = NormalizeOr(CrossFloat3(up_, forward_), right_);
            }
        }
    }
    else if (preset_.positionMode == CAMERA_ACTION_POSITION_MODE::TPS_VIEW_RELATIVE)
    {
        DirectX::XMFLOAT3 viewBackDirection_ =
            SubtractFloat3(startCameraPosition, focusPosition_);
        const float cameraFocusDistance_ = LengthFloat3(viewBackDirection_);

        viewBackDirection_.y = 0.0f;
        viewBackDirection_ = NormalizeOr(
            viewBackDirection_,
            NormalizeOr(ScaleFloat3(defaultForward_, -1.0f), { 0.0f, 0.0f, -1.0f }));
        right_ = NormalizeOr(CrossFloat3(up_, viewBackDirection_), right_);

        const float baseDistance_ = std::max(cameraFocusDistance_, 1.5f);
        const float finalDistance_ = std::max(baseDistance_ + preset_.cameraOffset.z, 0.75f);
        return AddFloat3(
            AddFloat3(
                AddFloat3(focusPosition_, ScaleFloat3(viewBackDirection_, finalDistance_)),
                ScaleFloat3(right_, preset_.cameraOffset.x)),
            ScaleFloat3(up_, preset_.cameraOffset.y));
    }

    DirectX::XMFLOAT3 basePosition_ = camera_ ? camera_->GetEye() : focusPosition_;
    if (preset_.positionMode != CAMERA_ACTION_POSITION_MODE::CURRENT_CAMERA_RELATIVE)
    {
        basePosition_ = focusPosition_;
    }

    return AddFloat3(
        AddFloat3(
            AddFloat3(basePosition_, ScaleFloat3(right_, preset_.cameraOffset.x)),
            ScaleFloat3(up_, preset_.cameraOffset.y)),
        ScaleFloat3(forward_, preset_.cameraOffset.z));
}

void CameraActionDirectorComponent::ApplyCameraPose(
    const DirectX::XMFLOAT3& cameraPosition_,
    const DirectX::XMFLOAT3& lookAtPosition_,
    float fov_,
    const DirectX::XMFLOAT3& up_)
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_ || !owner_->transform) return;

    // 補間中に上方向と視線が平行になっても、特異なView行列を生成しない。
    const auto forward_ = NormalizeOr(SubtractFloat3(lookAtPosition_, cameraPosition_), { 0.0f, 0.0f, 1.0f });
    auto safeUp_ = NormalizeOr(up_, { 0.0f, 1.0f, 0.0f });
    if (LengthFloat3(CrossFloat3(safeUp_, forward_)) < 0.0001f)
        safeUp_ = std::abs(forward_.y) < 0.99f ? DirectX::XMFLOAT3{ 0.0f, 1.0f, 0.0f } : DirectX::XMFLOAT3{ 1.0f, 0.0f, 0.0f };
    const auto safeFocus_ = LengthFloat3(SubtractFloat3(lookAtPosition_, cameraPosition_)) > 0.0001f
        ? lookAtPosition_ : AddFloat3(cameraPosition_, forward_);
    const DirectX::XMMATRIX viewMatrix_ = DirectX::XMMatrixLookAtLH(
        DirectX::XMLoadFloat3(&cameraPosition_),
        DirectX::XMLoadFloat3(&safeFocus_),
        DirectX::XMLoadFloat3(&safeUp_));
    DirectX::XMVECTOR determinant_;
    owner_->transform->ApplyWorldMatrix(
        DirectX::XMMatrixInverse(&determinant_, viewMatrix_));

    if (const std::shared_ptr<CameraComponent> camera_ = camera.lock())
    {
        camera_->SetLookAt(
            cameraPosition_,
            safeFocus_,
            safeUp_);
        camera_->SetFov(ClampFov(fov_));
    }
}

void CameraActionDirectorComponent::ReleaseSuppression()
{
    // 抑制を解除するとき、TPS側のyaw/pitchも現在カメラへ同期して急な跳ね戻りを防ぐ。
    if (ownsThirdPersonSuppression)
    {
        if (const std::shared_ptr<ThirdPersonCameraComponent> thirdPerson_ =
            thirdPersonCamera.lock())
        {
            thirdPerson_->SetControlSuppressed(false);
            thirdPerson_->SynchronizeViewAngles(
                returnStartCameraPosition,
                returnStartFocusPosition);
            thirdPerson_->BeginReturnBlendFromCurrentCamera();
        }
        ownsThirdPersonSuppression = false;
    }
    if (ownsLockOnSuppression)
    {
        if (const std::shared_ptr<CameraLockOnComponent> lockOn_ =
            lockOnCamera.lock())
        {
            lockOn_->SetActionSuppressed(false);
        }
        ownsLockOnSuppression = false;
    }
    if (ownsCollisionSuppression)
    {
        if (const std::shared_ptr<CameraCollisionComponent> collision_ =
            collisionCamera.lock())
        {
            collision_->SetCollisionSuppressed(false);
        }
        ownsCollisionSuppression = false;
    }
}

void CameraActionDirectorComponent::CaptureCurrentCameraAsPreset(
    CameraActionPreset& preset_) const
{
    BS_PROFILE_SCOPE("CameraActionDirectorComponent::CaptureCurrentCameraAsPreset");
    const std::shared_ptr<CameraComponent> camera_ = camera.lock();
    if (!camera_) return;

    const auto thirdPerson_ = thirdPersonCamera.lock();
    const auto target_ = thirdPerson_ ? thirdPerson_->GetFollowTarget() : nullptr;
    DirectX::XMFLOAT4X4 frame_;
    if (!BuildFollowFrame(target_, frame_)) return;

    // ワールド位置・姿勢を対象の逆変換で保存し、別の位置や向きでも同じ構図を再現する。
    const auto inverse_ = DirectX::XMMatrixInverse(nullptr, DirectX::XMLoadFloat4x4(&frame_));
    DirectX::XMStoreFloat3(&preset_.cameraOffset, DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&camera_->GetEye()), inverse_));
    DirectX::XMStoreFloat3(&preset_.focusOffset, DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&camera_->GetFocus()), inverse_));
    DirectX::XMStoreFloat3(&preset_.cameraUp, DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&camera_->GetUp()), inverse_));
    preset_.targetFov = camera_->GetFovY();
    preset_.positionMode = CAMERA_ACTION_POSITION_MODE::TPS_TARGET_RELATIVE;
}

int CameraActionDirectorComponent::FindPresetIndex(
    const std::string& actionName_) const
{
    for (int index_ = 0; index_ < static_cast<int>(presets.size()); ++index_)
    {
        if (presets[index_].actionName == actionName_) return index_;
    }
    return -1;
}

float CameraActionDirectorComponent::GetPhaseDuration() const
{
    if (phase == CAMERA_ACTION_PHASE::ENTER) return activePreset.enterDuration;
    if (phase == CAMERA_ACTION_PHASE::HOLD) return activePreset.holdDuration;
    if (phase == CAMERA_ACTION_PHASE::RETURN) return activePreset.returnDuration;
    return 0.0f;
}

float CameraActionDirectorComponent::SmoothStep(float value_) const
{
    const float clamped_ = std::clamp(value_, 0.0f, 1.0f);
    return clamped_ * clamped_ * (3.0f - 2.0f * clamped_);
}

float CameraActionDirectorComponent::ClampFov(float fov_) const
{
    return std::clamp(fov_, 10.0f, 120.0f);
}

void CameraActionDirectorComponent::Gui()
{
    ResolveReferences();

    ImGui::Text("Phase: %s", ToString(phase));
    ImGui::Text("Active: %s", activePreset.actionName.c_str());
    ImGui::Text("Camera: %.2f %.2f %.2f",
        actionCameraPosition.x,
        actionCameraPosition.y,
        actionCameraPosition.z);
    ImGui::Text("Focus: %.2f %.2f %.2f",
        actionFocusPosition.x,
        actionFocusPosition.y,
        actionFocusPosition.z);

    if (ImGui::Button("Add Preset"))
    {
        CameraActionPreset preset_;
        preset_.actionName = "NewCameraAction";
        presets.push_back(preset_);
        RegisterActions();
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop"))
    {
        StopAction(false);
    }

    for (int index_ = 0; index_ < static_cast<int>(presets.size()); ++index_)
    {
        CameraActionPreset& preset_ = presets[index_];
        ImGui::PushID(index_);
        if (ImGui::TreeNode(preset_.actionName.c_str()))
        {
            char nameBuffer_[128]{};
            strcpy_s(nameBuffer_, preset_.actionName.c_str());
            if (ImGui::InputText("Action Name", nameBuffer_, sizeof(nameBuffer_)))
            {
                preset_.actionName = nameBuffer_;
                RegisterActions();
            }

            int targetMode_ = static_cast<int>(preset_.targetMode);
            if (ImGui::Combo(
                "Target Mode",
                &targetMode_,
                "WORLD_POSITION\0GAME_OBJECT\0GAME_OBJECT_OFFSET\0HIT_POSITION\0PLAYER\0LOCK_ON_TARGET\0"))
            {
                preset_.targetMode =
                    static_cast<CAMERA_ACTION_TARGET_MODE>(targetMode_);
            }

            int positionMode_ = static_cast<int>(preset_.positionMode);
            if (ImGui::Combo(
                "Position Mode",
                &positionMode_,
                "CURRENT_CAMERA_RELATIVE\0TARGET_LOCAL_RELATIVE\0PLAYER_FORWARD_RELATIVE\0TPS_VIEW_RELATIVE\0WORLD_POSITION\0TPS_TARGET_RELATIVE\0"))
            {
                preset_.positionMode =
                    static_cast<CAMERA_ACTION_POSITION_MODE>(positionMode_);
            }

            if (preset_.positionMode == CAMERA_ACTION_POSITION_MODE::TPS_VIEW_RELATIVE)
            {
                ImGui::TextWrapped("TPS_VIEW_RELATIVE: X is side offset, Y is height, Z moves farther from the focus along the camera view at action start.");
            }
            ImGui::DragFloat3("Camera Offset", &preset_.cameraOffset.x, 0.05f);
            ImGui::DragFloat3("Focus Offset", &preset_.focusOffset.x, 0.05f);
            if (preset_.positionMode == CAMERA_ACTION_POSITION_MODE::TPS_TARGET_RELATIVE)
            {
                ImGui::TextWrapped("Camera and focus are relative to the TPS follow object (+Z forward). Target Mode is ignored. Object scale does not change the distance.");
                ImGui::DragFloat3("Local Camera Up", &preset_.cameraUp.x, 0.01f);
            }
            ImGui::DragFloat("Enter", &preset_.enterDuration, 0.01f, 0.0f, 5.0f);
            ImGui::DragFloat("Hold", &preset_.holdDuration, 0.01f, 0.0f, 5.0f);
            ImGui::DragFloat("Return", &preset_.returnDuration, 0.01f, 0.0f, 5.0f);
            ImGui::DragFloat("FOV", &preset_.targetFov, 0.1f, 10.0f, 120.0f);
            ImGui::DragFloat("Shake Strength", &preset_.shakeStrength, 0.005f, 0.0f, 2.0f);
            ImGui::DragFloat("Shake Frequency", &preset_.shakeFrequency, 0.1f, 0.0f, 120.0f);
            ImGui::DragInt("Priority", &preset_.priority);
            ImGui::Checkbox("Suppress TPS", &preset_.suppressThirdPersonControl);
            ImGui::Checkbox("Suppress LockOn", &preset_.suppressLockOnControl);
            ImGui::Checkbox("Suppress Collision", &preset_.suppressCameraCollision);
            ImGui::Checkbox("Use Unscaled Time", &preset_.useUnscaledTime);

            char categoryBuffer_[64]{};
            strcpy_s(categoryBuffer_, preset_.blackboardCategory.c_str());
            if (ImGui::InputText("Blackboard Category", categoryBuffer_, sizeof(categoryBuffer_)))
            {
                preset_.blackboardCategory = categoryBuffer_;
            }
            char focusKeyBuffer_[64]{};
            strcpy_s(focusKeyBuffer_, preset_.focusPositionKey.c_str());
            if (ImGui::InputText("Focus Key", focusKeyBuffer_, sizeof(focusKeyBuffer_)))
            {
                preset_.focusPositionKey = focusKeyBuffer_;
            }
            char directionKeyBuffer_[64]{};
            strcpy_s(directionKeyBuffer_, preset_.attackDirectionKey.c_str());
            if (ImGui::InputText("Direction Key", directionKeyBuffer_, sizeof(directionKeyBuffer_)))
            {
                preset_.attackDirectionKey = directionKeyBuffer_;
            }

            if (ImGui::Button("Capture Current Camera"))
            {
                CaptureCurrentCameraAsPreset(preset_);
            }
            ImGui::SameLine();
            if (ImGui::Button("Play Preview"))
            {
                PlayActionByIndex(index_);
            }
            ImGui::SameLine();
            if (ImGui::Button("Delete") && presets.size() > 1)
            {
                const std::string actionName_ = preset_.actionName;
                presets.erase(presets.begin() + index_);
                ActionRegistry::Instance().RemoveAction("CameraAction", actionName_);
                ImGui::TreePop();
                ImGui::PopID();
                break;
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
}

void CameraActionDirectorComponent::Serialize(nlohmann::json& json_) const
{
    json_["presets"] = nlohmann::json::array();
    for (const CameraActionPreset& preset_ : presets)
    {
        nlohmann::json presetJson_;
        presetJson_["actionName"] = preset_.actionName;
        presetJson_["targetMode"] = ToString(preset_.targetMode);
        presetJson_["positionMode"] = ToString(preset_.positionMode);
        presetJson_["cameraOffset"] =
        {
            preset_.cameraOffset.x,
            preset_.cameraOffset.y,
            preset_.cameraOffset.z
        };
        presetJson_["focusOffset"] =
        {
            preset_.focusOffset.x,
            preset_.focusOffset.y,
            preset_.focusOffset.z
        };
        presetJson_["enterDuration"] = preset_.enterDuration;
        presetJson_["holdDuration"] = preset_.holdDuration;
        presetJson_["returnDuration"] = preset_.returnDuration;
        presetJson_["targetFov"] = preset_.targetFov;
        presetJson_["cameraUp"] = { preset_.cameraUp.x, preset_.cameraUp.y, preset_.cameraUp.z };
        presetJson_["shakeStrength"] = preset_.shakeStrength;
        presetJson_["shakeFrequency"] = preset_.shakeFrequency;
        presetJson_["priority"] = preset_.priority;
        presetJson_["suppressThirdPersonControl"] =
            preset_.suppressThirdPersonControl;
        presetJson_["suppressLockOnControl"] =
            preset_.suppressLockOnControl;
        presetJson_["suppressCameraCollision"] =
            preset_.suppressCameraCollision;
        presetJson_["useUnscaledTime"] = preset_.useUnscaledTime;
        presetJson_["blackboardCategory"] = preset_.blackboardCategory;
        presetJson_["focusPositionKey"] = preset_.focusPositionKey;
        presetJson_["attackDirectionKey"] = preset_.attackDirectionKey;
        json_["presets"].push_back(presetJson_);
    }
}

void CameraActionDirectorComponent::Deserialize(const nlohmann::json& json_)
{
    presets.clear();
    if (json_.contains("presets") && json_["presets"].is_array())
    {
        for (const nlohmann::json& presetJson_ : json_["presets"])
        {
            CameraActionPreset preset_;
            preset_.actionName =
                presetJson_.value("actionName", std::string{ "CameraAction" });
            preset_.targetMode = TargetModeFromString(
                presetJson_.value("targetMode", std::string{ "WORLD_POSITION" }));
            preset_.positionMode = PositionModeFromString(
                presetJson_.value("positionMode", std::string{ "CURRENT_CAMERA_RELATIVE" }));
            preset_.cameraOffset = LoadFloat3(
                presetJson_,
                "cameraOffset",
                preset_.cameraOffset);
            preset_.focusOffset = LoadFloat3(
                presetJson_,
                "focusOffset",
                preset_.focusOffset);
            preset_.enterDuration = presetJson_.value("enterDuration", 0.08f);
            preset_.holdDuration = presetJson_.value("holdDuration", 0.12f);
            preset_.returnDuration = presetJson_.value("returnDuration", 0.18f);
            preset_.targetFov = presetJson_.value("targetFov", 45.0f);
            preset_.cameraUp = LoadFloat3(presetJson_, "cameraUp", { 0.0f, 1.0f, 0.0f });
            preset_.shakeStrength = presetJson_.value("shakeStrength", 0.0f);
            preset_.shakeFrequency = presetJson_.value("shakeFrequency", 20.0f);
            preset_.priority = presetJson_.value("priority", 0);
            preset_.suppressThirdPersonControl =
                presetJson_.value("suppressThirdPersonControl", true);
            preset_.suppressLockOnControl =
                presetJson_.value("suppressLockOnControl", true);
            preset_.suppressCameraCollision =
                presetJson_.value("suppressCameraCollision", false);
            preset_.useUnscaledTime =
                presetJson_.value("useUnscaledTime", true);
            preset_.blackboardCategory =
                presetJson_.value("blackboardCategory", std::string{ "Guard" });
            preset_.focusPositionKey =
                presetJson_.value("focusPositionKey", std::string{ "Position" });
            preset_.attackDirectionKey =
                presetJson_.value("attackDirectionKey", std::string{ "Normal" });
            presets.push_back(preset_);
        }
    }
}

REGISTER_COMPONENT(CameraActionDirectorComponent, "CameraActionDirectorComponent")

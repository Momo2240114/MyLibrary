#include "PrecompiledHeader.h"

#include "UIWorldAnchorComponent.h"

#include "Engine/Debug/TracyProfiler.h"
#include "Engine/GamePlay/Camera/CameraComponent.h"
#include "Engine/GamePlay/Camera/CameraLockOnComponent.h"
#include "Engine/GamePlay/Camera/CameraManager.h"
#include "Engine/GamePlay/Camera/LockOnTargetComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
#include "Engine/GamePlay/Transform/Transform.h"
#include "Engine/UI/UIWidgetComponent.h"
#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace
{
    const char* UI_WORLD_ANCHOR_TARGET_MODE_NAMES[] =
    {
        "NAMED_OBJECT",
        "LOCK_ON_TARGET"
    };

    const char* UI_WORLD_ANCHOR_ROTATION_MODE_NAMES[] =
    {
        "KEEP_LOCAL",
        "COPY_TARGET_Z",
        "PROJECT_TARGET_RIGHT"
    };

    constexpr int UI_WORLD_ANCHOR_TARGET_MODE_COUNT =
        static_cast<int>(sizeof(UI_WORLD_ANCHOR_TARGET_MODE_NAMES) / sizeof(UI_WORLD_ANCHOR_TARGET_MODE_NAMES[0]));
    constexpr int UI_WORLD_ANCHOR_ROTATION_MODE_COUNT =
        static_cast<int>(sizeof(UI_WORLD_ANCHOR_ROTATION_MODE_NAMES) / sizeof(UI_WORLD_ANCHOR_ROTATION_MODE_NAMES[0]));

    const char* ToString(UiWorldAnchorTargetMode mode_)
    {
        const int index_ = static_cast<int>(mode_);
        if (index_ < 0 || index_ >= UI_WORLD_ANCHOR_TARGET_MODE_COUNT)
        {
            return UI_WORLD_ANCHOR_TARGET_MODE_NAMES[0];
        }
        return UI_WORLD_ANCHOR_TARGET_MODE_NAMES[index_];
    }

    const char* ToString(UiWorldAnchorRotationMode mode_)
    {
        const int index_ = static_cast<int>(mode_);
        if (index_ < 0 || index_ >= UI_WORLD_ANCHOR_ROTATION_MODE_COUNT)
        {
            return UI_WORLD_ANCHOR_ROTATION_MODE_NAMES[0];
        }
        return UI_WORLD_ANCHOR_ROTATION_MODE_NAMES[index_];
    }

    UiWorldAnchorTargetMode TargetModeFromString(const std::string& name_)
    {
        for (int index_ = 0; index_ < UI_WORLD_ANCHOR_TARGET_MODE_COUNT; ++index_)
        {
            if (name_ == UI_WORLD_ANCHOR_TARGET_MODE_NAMES[index_])
            {
                return static_cast<UiWorldAnchorTargetMode>(index_);
            }
        }
        return UiWorldAnchorTargetMode::NAMED_OBJECT;
    }

    UiWorldAnchorRotationMode RotationModeFromString(const std::string& name_)
    {
        for (int index_ = 0; index_ < UI_WORLD_ANCHOR_ROTATION_MODE_COUNT; ++index_)
        {
            if (name_ == UI_WORLD_ANCHOR_ROTATION_MODE_NAMES[index_])
            {
                return static_cast<UiWorldAnchorRotationMode>(index_);
            }
        }
        return UiWorldAnchorRotationMode::KEEP_LOCAL;
    }

    void SerializeFloat2(nlohmann::json& json_, const char* key_, const DirectX::XMFLOAT2& value_)
    {
        json_[key_] = { value_.x, value_.y };
    }

    void SerializeFloat3(nlohmann::json& json_, const char* key_, const DirectX::XMFLOAT3& value_)
    {
        json_[key_] = { value_.x, value_.y, value_.z };
    }

    void DeserializeFloat2(const nlohmann::json& json_, const char* key_, DirectX::XMFLOAT2& value_)
    {
        if (!json_.contains(key_) || !json_[key_].is_array() || json_[key_].size() < 2)
        {
            return;
        }
        value_.x = json_[key_][0].get<float>();
        value_.y = json_[key_][1].get<float>();
    }

    void DeserializeFloat3(const nlohmann::json& json_, const char* key_, DirectX::XMFLOAT3& value_)
    {
        if (!json_.contains(key_) || !json_[key_].is_array() || json_[key_].size() < 3)
        {
            return;
        }
        value_.x = json_[key_][0].get<float>();
        value_.y = json_[key_][1].get<float>();
        value_.z = json_[key_][2].get<float>();
    }
}

void UIWorldAnchorComponent::Start()
{
    BS_PROFILE_SCOPE("UIWorldAnchorComponent::Start");
    NormalizeSettings();
    ResolveReferences();
}

void UIWorldAnchorComponent::LateUpdate(float deltaTime_)
{
    BS_PROFILE_SCOPE("UIWorldAnchorComponent::LateUpdate");
    (void)deltaTime_;

    std::shared_ptr<UIWidgetComponent> widget_ = widget.lock();
    if (!widget_)
    {
        ResolveReferences();
        widget_ = widget.lock();
    }
    if (!widget_)
    {
        return;
    }

    const std::shared_ptr<GameObject> targetObject_ = ResolveTargetObject();
    const std::shared_ptr<CameraComponent> camera_ = ResolveCamera();
    if (!targetObject_ || !camera_)
    {
        ApplyWidgetVisibility(!hideWhenTargetMissing);
        return;
    }

    const DirectX::XMFLOAT3 anchorWorldPosition_ = CalculateAnchorWorldPosition(targetObject_);
    DirectX::XMFLOAT2 screenPosition_{};
    float depth_ = 0.0f;
    const bool projected_ = ProjectWorldToScreen(
        *camera_,
        anchorWorldPosition_,
        screenPosition_,
        depth_);
    if (!projected_)
    {
        ApplyWidgetVisibility(!hideWhenBehindCamera);
        return;
    }

    const bool outsideScreen_ =
        screenPosition_.x < 0.0f ||
        screenPosition_.x > screenSize.x ||
        screenPosition_.y < 0.0f ||
        screenPosition_.y > screenSize.y ||
        depth_ < 0.0f ||
        depth_ > 1.0f;
    if (outsideScreen_ && hideWhenOutsideScreen)
    {
        ApplyWidgetVisibility(false);
        return;
    }

    if (clampToScreen)
    {
        // 画面外に出たTargetでも、端に残して方向を伝えられるようにする。
        screenPosition_.x = std::clamp(screenPosition_.x, 0.0f, screenSize.x);
        screenPosition_.y = std::clamp(screenPosition_.y, 0.0f, screenSize.y);
    }

    UIRectTransform& rectTransform_ = widget_->GetRectTransform();
    rectTransform_.anchoredPosition =
    {
        screenPosition_.x + screenOffset.x,
        screenPosition_.y + screenOffset.y
    };

    ApplyProjectedRotation(*camera_, targetObject_, anchorWorldPosition_);
    widget_->MarkLayoutDirty();
    ApplyWidgetVisibility(true);
}

void UIWorldAnchorComponent::Gui()
{
    BS_PROFILE_SCOPE("UIWorldAnchorComponent::Gui");

    int currentTargetMode_ = static_cast<int>(targetMode);
    if (ImGui::Combo(
        "Target Mode",
        &currentTargetMode_,
        UI_WORLD_ANCHOR_TARGET_MODE_NAMES,
        UI_WORLD_ANCHOR_TARGET_MODE_COUNT))
    {
        targetMode = static_cast<UiWorldAnchorTargetMode>(currentTargetMode_);
        targetObject.reset();
    }

    int currentRotationMode_ = static_cast<int>(rotationMode);
    if (ImGui::Combo(
        "Rotation Mode",
        &currentRotationMode_,
        UI_WORLD_ANCHOR_ROTATION_MODE_NAMES,
        UI_WORLD_ANCHOR_ROTATION_MODE_COUNT))
    {
        rotationMode = static_cast<UiWorldAnchorRotationMode>(currentRotationMode_);
    }

    char targetObjectNameBuffer_[128]{};
    strncpy_s(targetObjectNameBuffer_, sizeof(targetObjectNameBuffer_), targetObjectName.c_str(), _TRUNCATE);
    if (ImGui::InputText("Target Object", targetObjectNameBuffer_, sizeof(targetObjectNameBuffer_)))
    {
        targetObjectName = targetObjectNameBuffer_;
        targetObject.reset();
    }

    char lockOnSourceObjectNameBuffer_[128]{};
    strncpy_s(
        lockOnSourceObjectNameBuffer_,
        sizeof(lockOnSourceObjectNameBuffer_),
        lockOnSourceObjectName.c_str(),
        _TRUNCATE);
    if (ImGui::InputText("Lock On Source", lockOnSourceObjectNameBuffer_, sizeof(lockOnSourceObjectNameBuffer_)))
    {
        lockOnSourceObjectName = lockOnSourceObjectNameBuffer_;
        targetObject.reset();
    }

    char cameraObjectNameBuffer_[128]{};
    strncpy_s(cameraObjectNameBuffer_, sizeof(cameraObjectNameBuffer_), cameraObjectName.c_str(), _TRUNCATE);
    if (ImGui::InputText("Camera Object", cameraObjectNameBuffer_, sizeof(cameraObjectNameBuffer_)))
    {
        cameraObjectName = cameraObjectNameBuffer_;
    }

    ImGui::DragFloat3("World Offset", &worldOffset.x, 0.05f);
    ImGui::DragFloat2("Screen Offset", &screenOffset.x, 1.0f);
    if (ImGui::DragFloat2("Screen Size", &screenSize.x, 1.0f, 1.0f, 8192.0f))
    {
        NormalizeSettings();
    }
    if (ImGui::DragFloat("Rotation Sample Length", &rotationSampleLength, 0.05f, 0.01f, 100.0f))
    {
        NormalizeSettings();
    }

    ImGui::Checkbox("Use LockOn Focus Position", &useLockOnFocusPosition);
    ImGui::Checkbox("Hide Target Missing", &hideWhenTargetMissing);
    ImGui::Checkbox("Hide Behind Camera", &hideWhenBehindCamera);
    ImGui::Checkbox("Hide Outside Screen", &hideWhenOutsideScreen);
    ImGui::Checkbox("Clamp To Screen", &clampToScreen);
}

void UIWorldAnchorComponent::Serialize(nlohmann::json& json_) const
{
    BS_PROFILE_SCOPE("UIWorldAnchorComponent::Serialize");

    json_["Version"] = 1;
    json_["TargetMode"] = ToString(targetMode);
    json_["RotationMode"] = ToString(rotationMode);
    json_["TargetObjectName"] = targetObjectName;
    json_["LockOnSourceObjectName"] = lockOnSourceObjectName;
    json_["CameraObjectName"] = cameraObjectName;
    SerializeFloat3(json_, "WorldOffset", worldOffset);
    SerializeFloat2(json_, "ScreenOffset", screenOffset);
    SerializeFloat2(json_, "ScreenSize", screenSize);
    json_["UseLockOnFocusPosition"] = useLockOnFocusPosition;
    json_["HideWhenTargetMissing"] = hideWhenTargetMissing;
    json_["HideWhenBehindCamera"] = hideWhenBehindCamera;
    json_["HideWhenOutsideScreen"] = hideWhenOutsideScreen;
    json_["ClampToScreen"] = clampToScreen;
    json_["RotationSampleLength"] = rotationSampleLength;
}

void UIWorldAnchorComponent::Deserialize(const nlohmann::json& json_)
{
    BS_PROFILE_SCOPE("UIWorldAnchorComponent::Deserialize");

    targetMode = TargetModeFromString(json_.value("TargetMode", std::string(ToString(targetMode))));
    rotationMode = RotationModeFromString(json_.value("RotationMode", std::string(ToString(rotationMode))));
    targetObjectName = json_.value("TargetObjectName", targetObjectName);
    lockOnSourceObjectName = json_.value("LockOnSourceObjectName", lockOnSourceObjectName);
    cameraObjectName = json_.value("CameraObjectName", cameraObjectName);
    DeserializeFloat3(json_, "WorldOffset", worldOffset);
    DeserializeFloat2(json_, "ScreenOffset", screenOffset);
    DeserializeFloat2(json_, "ScreenSize", screenSize);
    useLockOnFocusPosition = json_.value("UseLockOnFocusPosition", useLockOnFocusPosition);
    hideWhenTargetMissing = json_.value("HideWhenTargetMissing", hideWhenTargetMissing);
    hideWhenBehindCamera = json_.value("HideWhenBehindCamera", hideWhenBehindCamera);
    hideWhenOutsideScreen = json_.value("HideWhenOutsideScreen", hideWhenOutsideScreen);
    clampToScreen = json_.value("ClampToScreen", clampToScreen);
    rotationSampleLength = json_.value("RotationSampleLength", rotationSampleLength);
    NormalizeSettings();
    targetObject.reset();
}

void UIWorldAnchorComponent::SetTargetObject(const std::shared_ptr<GameObject>& targetObject_)
{
    targetObject = targetObject_;
    targetObjectName = targetObject_ ? targetObject_->GetName() : std::string{};
}

std::shared_ptr<GameObject> UIWorldAnchorComponent::GetTargetObject() const
{
    return targetObject.lock();
}

void UIWorldAnchorComponent::ResolveReferences()
{
    BS_PROFILE_SCOPE("UIWorldAnchorComponent::ResolveReferences");

    if (const std::shared_ptr<GameObject> owner_ = GetGameObject())
    {
        widget = owner_->GetComponent<UIWidgetComponent>();
    }
}

std::shared_ptr<GameObject> UIWorldAnchorComponent::ResolveTargetObject()
{
    BS_PROFILE_SCOPE("UIWorldAnchorComponent::ResolveTargetObject");

    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    GameObjectManager* manager_ = owner_ ? owner_->manager : nullptr;
    if (!manager_)
    {
        return nullptr;
    }

    if (targetMode == UiWorldAnchorTargetMode::LOCK_ON_TARGET)
    {
        targetObject.reset();

        if (lockOnSourceObjectName.empty())
        {
            // Source名が未指定なら、Scene内で有効なLockOn制御を探してUIを簡単に接続できるようにする。
            const std::vector<std::shared_ptr<CameraLockOnComponent>> lockOnComponents_ =
                manager_->FindComponents<CameraLockOnComponent>();
            for (const std::shared_ptr<CameraLockOnComponent>& lockOn_ : lockOnComponents_)
            {
                if (!lockOn_ || !lockOn_->IsLockOnEnabled())
                {
                    continue;
                }
                targetObject = lockOn_->GetLockOnTarget();
                if (const std::shared_ptr<GameObject> currentTarget_ = targetObject.lock())
                {
                    return currentTarget_;
                }
            }
        }
        else
        {
            const std::shared_ptr<GameObject> lockOnSourceObject_ = manager_->Find(lockOnSourceObjectName);
            if (lockOnSourceObject_)
            {
                if (const std::shared_ptr<CameraLockOnComponent> lockOn_ =
                    lockOnSourceObject_->GetComponent<CameraLockOnComponent>())
                {
                    targetObject = lockOn_->GetLockOnTarget();
                    return targetObject.lock();
                }
            }
        }
    }

    if (const std::shared_ptr<GameObject> currentTarget_ = targetObject.lock())
    {
        return currentTarget_;
    }

    if (!targetObjectName.empty())
    {
        targetObject = manager_->Find(targetObjectName);
    }
    return targetObject.lock();
}

std::shared_ptr<CameraComponent> UIWorldAnchorComponent::ResolveCamera() const
{
    BS_PROFILE_SCOPE("UIWorldAnchorComponent::ResolveCamera");

    const std::shared_ptr<GameObject> owner_ = gameObject.lock();
    GameObjectManager* manager_ = owner_ ? owner_->manager : nullptr;
    if (!manager_)
    {
        return nullptr;
    }

    if (!cameraObjectName.empty())
    {
        const std::shared_ptr<GameObject> cameraObject_ = manager_->Find(cameraObjectName);
        return cameraObject_ ? cameraObject_->GetComponent<CameraComponent>() : nullptr;
    }

    CameraManager* cameraManager_ = manager_->GetCameraManager();
    return cameraManager_ ? cameraManager_->GetActiveCamera() : nullptr;
}

DirectX::XMFLOAT3 UIWorldAnchorComponent::CalculateAnchorWorldPosition(
    const std::shared_ptr<GameObject>& targetObject_) const
{
    BS_PROFILE_SCOPE("UIWorldAnchorComponent::CalculateAnchorWorldPosition");

    DirectX::XMFLOAT3 position_ = worldOffset;
    if (!targetObject_)
    {
        return position_;
    }

    if (useLockOnFocusPosition)
    {
        if (const std::shared_ptr<LockOnTargetComponent> lockOnTarget_ =
            targetObject_->GetComponent<LockOnTargetComponent>())
        {
            position_ = lockOnTarget_->GetFocusPosition();
            position_.x += worldOffset.x;
            position_.y += worldOffset.y;
            position_.z += worldOffset.z;
            return position_;
        }
    }

    if (!targetObject_->transform)
    {
        return position_;
    }

    const DirectX::XMFLOAT3 targetPosition_ = targetObject_->transform->GetWorldPosition();
    position_.x += targetPosition_.x;
    position_.y += targetPosition_.y;
    position_.z += targetPosition_.z;
    return position_;
}

bool UIWorldAnchorComponent::ProjectWorldToScreen(
    const CameraComponent& camera_,
    const DirectX::XMFLOAT3& worldPosition_,
    DirectX::XMFLOAT2& screenPosition_,
    float& depth_) const
{
    BS_PROFILE_SCOPE("UIWorldAnchorComponent::ProjectWorldToScreen");

    const DirectX::XMMATRIX viewProjection_ = camera_.GetViewProjection();
    const DirectX::XMVECTOR worldVector_ =
        DirectX::XMVectorSet(worldPosition_.x, worldPosition_.y, worldPosition_.z, 1.0f);
    const DirectX::XMVECTOR clipVector_ = DirectX::XMVector4Transform(worldVector_, viewProjection_);

    DirectX::XMFLOAT4 clipPosition_{};
    DirectX::XMStoreFloat4(&clipPosition_, clipVector_);
    if (clipPosition_.w <= 0.0001f)
    {
        return false;
    }

    const float ndcX_ = clipPosition_.x / clipPosition_.w;
    const float ndcY_ = clipPosition_.y / clipPosition_.w;
    const float ndcZ_ = clipPosition_.z / clipPosition_.w;

    screenPosition_.x = (ndcX_ + 1.0f) * screenSize.x * 0.5f;
    screenPosition_.y = (1.0f - ndcY_) * screenSize.y * 0.5f;
    depth_ = ndcZ_;
    return true;
}

bool UIWorldAnchorComponent::ApplyProjectedRotation(
    const CameraComponent& camera_,
    const std::shared_ptr<GameObject>& targetObject_,
    const DirectX::XMFLOAT3& anchorWorldPosition_)
{
    BS_PROFILE_SCOPE("UIWorldAnchorComponent::ApplyProjectedRotation");

    std::shared_ptr<UIWidgetComponent> widget_ = widget.lock();
    if (!widget_ || !targetObject_ || !targetObject_->transform)
    {
        return false;
    }

    UIRectTransform& rectTransform_ = widget_->GetRectTransform();
    switch (rotationMode)
    {
    case UiWorldAnchorRotationMode::COPY_TARGET_Z:
        rectTransform_.localRotation = targetObject_->transform->GetWorldRotation().z;
        return true;
    case UiWorldAnchorRotationMode::PROJECT_TARGET_RIGHT:
    {
        const DirectX::XMMATRIX worldMatrix_ = targetObject_->transform->GetWorldMatrix();
        DirectX::XMFLOAT3 rightDirection_{};
        DirectX::XMStoreFloat3(
            &rightDirection_,
            DirectX::XMVector3Normalize(worldMatrix_.r[0]));

        const DirectX::XMFLOAT3 rightWorldPosition_ =
        {
            anchorWorldPosition_.x + rightDirection_.x * rotationSampleLength,
            anchorWorldPosition_.y + rightDirection_.y * rotationSampleLength,
            anchorWorldPosition_.z + rightDirection_.z * rotationSampleLength
        };

        DirectX::XMFLOAT2 anchorScreen_{};
        DirectX::XMFLOAT2 rightScreen_{};
        float anchorDepth_ = 0.0f;
        float rightDepth_ = 0.0f;
        if (!ProjectWorldToScreen(camera_, anchorWorldPosition_, anchorScreen_, anchorDepth_) ||
            !ProjectWorldToScreen(camera_, rightWorldPosition_, rightScreen_, rightDepth_))
        {
            return false;
        }

        const float deltaX_ = rightScreen_.x - anchorScreen_.x;
        const float deltaY_ = rightScreen_.y - anchorScreen_.y;
        if (std::abs(deltaX_) < 0.001f && std::abs(deltaY_) < 0.001f)
        {
            return false;
        }

        rectTransform_.localRotation = DirectX::XMConvertToDegrees(std::atan2(deltaY_, deltaX_));
        return true;
    }
    case UiWorldAnchorRotationMode::KEEP_LOCAL:
    default:
        return false;
    }
}

void UIWorldAnchorComponent::ApplyWidgetVisibility(bool visible_)
{
    BS_PROFILE_SCOPE("UIWorldAnchorComponent::ApplyWidgetVisibility");

    if (const std::shared_ptr<UIWidgetComponent> widget_ = widget.lock())
    {
        widget_->SetEnabled(visible_);
    }
}

void UIWorldAnchorComponent::NormalizeSettings()
{
    screenSize.x = (std::max)(1.0f, screenSize.x);
    screenSize.y = (std::max)(1.0f, screenSize.y);
    rotationSampleLength = (std::max)(0.01f, rotationSampleLength);
}

REGISTER_COMPONENT(UIWorldAnchorComponent, "UIWorldAnchorComponent")

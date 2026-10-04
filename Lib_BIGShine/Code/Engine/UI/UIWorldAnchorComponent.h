#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"

#include <DirectXMath.h>
#include <memory>
#include <string>

class CameraComponent;
class GameObject;
class UIWidgetComponent;

enum class UiWorldAnchorTargetMode
{
    // TargetObjectNameで指定したObjectへ追従する。
    NAMED_OBJECT,
    // LockOnSourceObjectNameで指定したCameraLockOnComponentの現在Targetへ追従する。
    LOCK_ON_TARGET
};

enum class UiWorldAnchorRotationMode
{
    // UIWidgetComponentに設定された回転を維持する。
    KEEP_LOCAL,
    // TargetのWorld Z回転をUIの2D回転へコピーする。
    COPY_TARGET_Z,
    // TargetのWorld右方向を画面へ投影し、見た目の角度をUIへ反映する。
    PROJECT_TARGET_RIGHT
};

// World上のObject位置をScreen座標へ変換し、UIWidgetComponentを追従させる。
class UIWorldAnchorComponent : public Component
{
public:
    void Start() override;
    void LateUpdate(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    const std::string& GetName() const override
    {
        static std::string name_ = "UIWorldAnchorComponent";
        return name_;
    }

    // 外部処理から追従対象を直接差し替える。
    void SetTargetObject(const std::shared_ptr<GameObject>& targetObject_);
    std::shared_ptr<GameObject> GetTargetObject() const;

private:
    void ResolveReferences();
    std::shared_ptr<GameObject> ResolveTargetObject();
    std::shared_ptr<CameraComponent> ResolveCamera() const;
    DirectX::XMFLOAT3 CalculateAnchorWorldPosition(const std::shared_ptr<GameObject>& targetObject_) const;
    bool ProjectWorldToScreen(
        const CameraComponent& camera_,
        const DirectX::XMFLOAT3& worldPosition_,
        DirectX::XMFLOAT2& screenPosition_,
        float& depth_) const;
    bool ApplyProjectedRotation(
        const CameraComponent& camera_,
        const std::shared_ptr<GameObject>& targetObject_,
        const DirectX::XMFLOAT3& anchorWorldPosition_);
    void ApplyWidgetVisibility(bool visible_);
    void NormalizeSettings();

private:
    std::weak_ptr<UIWidgetComponent> widget;
    std::weak_ptr<GameObject> targetObject;

    UiWorldAnchorTargetMode targetMode = UiWorldAnchorTargetMode::NAMED_OBJECT;
    UiWorldAnchorRotationMode rotationMode = UiWorldAnchorRotationMode::KEEP_LOCAL;

    std::string targetObjectName;
    std::string lockOnSourceObjectName;
    std::string cameraObjectName;

    // Target原点からUIを出したいWorld Offset。
    DirectX::XMFLOAT3 worldOffset = { 0.0f, 1.2f, 0.0f };
    // 投影後に加えるScreen Offset。
    DirectX::XMFLOAT2 screenOffset = { 0.0f, 0.0f };
    // 投影に使う画面サイズ。RenderSystemのViewportと同じ値に合わせる。
    DirectX::XMFLOAT2 screenSize = { 1920.0f, 1080.0f };

    bool useLockOnFocusPosition = true;
    bool hideWhenTargetMissing = true;
    bool hideWhenBehindCamera = true;
    bool hideWhenOutsideScreen = false;
    bool clampToScreen = false;

    // PROJECT_TARGET_RIGHT時に画面上の角度を求めるためのWorld上の距離。
    float rotationSampleLength = 1.0f;
};

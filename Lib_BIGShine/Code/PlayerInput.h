#pragma once

#include "ComponentBase.h"
#include "KeyConfig.h"
#include "CameraComponent.h"

#include <DirectXMath.h>

class PlayerInput : public Component
{
public:
    const std::string& GetName() const override { static std::string name = "PlayerInput"; return name; }

    float moveSpeed = 5.0f;
    float rotateSpeed = 90.0f; // Yaw回転速度（度/秒）

    // Frontの設定用（どの軸を前方にするか）
    enum class FrontPattern
    {
        XPlus,
        XMinus,
        ZPlus,
        ZMinus
    } frontPattern = FrontPattern::ZPlus;

    // カメラを弱参照で保持
    std::weak_ptr<CameraComponent> mainCamera;

    void Update(float deltaTime) override;

    void Gui() override;
};

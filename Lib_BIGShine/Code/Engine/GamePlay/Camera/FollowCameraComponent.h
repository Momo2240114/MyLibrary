#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include <DirectXMath.h>

class CameraComponent;

class FollowCameraComponent : public Component
{
public:
    const std::string& GetName() const override
    {
        static std::string name = "FollowCameraComponent";
        return name;
    }

    // 追従対象（プレイヤー）
    std::weak_ptr<GameObject> target;

    // 所有カメラ（弱参照）
    std::weak_ptr<CameraComponent> camera;

    // カメラ位置オフセット（プレイヤー後方・上）
    DirectX::XMFLOAT3 offset{ 0.0f, 5.0f, -10.0f };

    // 補間速度
    float followSpeed = 5.0f;

    void Update(float deltaTime) override;
    void Gui() override;

    // カメラの回転角度（Pitch、Yaw）
    DirectX::XMFLOAT3 cameraAngle = { 0.0f, 0.0f, 0.0f };  // 初期角度（x = Pitch, y = Yaw, z = Roll）


    // カメラの回転速度
    float cameraPitchSpeed = 0.005f;   // ピッチ（上下回転）の感度
    float cameraYawSpeed = 0.005f;     // ヨー（左右回転）の感度

    // カメラの距離（注視点からの距離）
    float cameraDistance = 5.0f;       // 初期距離
    float cameraMinDistance = 2.0f;    // 最小距離
    float cameraMaxDistance = 10.0f;   // 最大距離

    // ピッチの最小・最大制限（上下回転の制限）
    float cameraMinPitch = -DirectX::XM_PIDIV2 + 0.1f;  // 最小ピッチ（-90度 - 少しの余裕）
    float cameraMaxPitch = DirectX::XM_PIDIV2 - 0.1f;   // 最大ピッチ（90度 - 少しの余裕）
};

#pragma once

#include  "Engine/GamePlay/Core/Component/ComponentBase.h"

class CameraFreeController : public Component
{
public:
    CameraFreeController() = default;
    ~CameraFreeController() override = default;

    // コンポーネントの名前を返す儀式
    const std::string& GetName() const override
    {
        static std::string name = "CameraFreeController";
        return name;
    }

    //初期化
    void Start() override;

    // 更新処理
    void Update(float deltaTime) override;

    // デバッグの覗き窓
    void Gui() override;

private:
    // マウスの相対的な移動量を算出する関数
    void UpdateRotation(float deltaTime);

    // キーボードの入力をから、三次元の座標を移動する処理
    void UpdateMovement(float deltaTime);

private:
    float moveSpeed = 10.0f;  // カメラが風を駆ける基本速度
    float lookSpeed = 0.001f;  // 視線を動かす際の感度 (Degree)
    float speedMultiplier = 1.0f; // ホイールの魔術によって変動する速度倍率

    float currentYaw = 0.0f;   // Y軸の周りを巡る方位角 (度数法)
    float currentPitch = 0.0f;   // X軸の上下を睨む仰俯角 (度数法)

    // 前フレームのマウス位置を記憶するための変数
    DirectX::XMFLOAT2 lastMousePos{ 0.0f, 0.0f };
};
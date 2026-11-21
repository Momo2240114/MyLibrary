#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"

#include <DirectXMath.h>
#include <memory>
#include <string>

class CameraComponent;
class GameObject;

class ThirdPersonCameraComponent : public Component
{
public:
    const std::string& GetName() const override
    {
        static std::string name_ = "ThirdPersonCameraComponent";
        return name_;
    }

    void Start() override;
    void LateUpdate(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;
    // 第二形態の設定を実行時だけ使用し、保存時は元の設定を維持する。
    void ApplyRuntimeSettings(const nlohmann::json& settings_);
    // プレビュー終了時に、切替前の設定へ戻す。
    void ClearRuntimeSettings();

    // LockOnなど別ComponentがCamera制御を握る間、TPS入力更新を止める。
    void SetControlSuppressed(bool suppressed_);
    // 外部Camera位置からPitch/Yawを逆算し、TPS制御へ戻る角度を同期する。
    void SynchronizeViewAngles(
        const DirectX::XMFLOAT3& cameraPosition_,
        const DirectX::XMFLOAT3& lookAtPosition_);
    // 現在Camera位置からTPS通常位置へ滑らかに戻すBlendを開始する。
    void BeginReturnBlendFromCurrentCamera();
    // 復帰Blendの進行率をSmoothStepで返す。
    float CalculateReturnBlendRate() const;
    // 演出カメラがTPSと同じ追従対象を基準にできるよう、参照を解決して返す。
    std::shared_ptr<GameObject> GetFollowTarget();
    float GetBaseDistance() const { return baseDistance; }
    DirectX::XMFLOAT3 GetTargetOffset() const { return targetOffset; }

private:
    // Camera入力Axisが未登録ならDefault設定を追加する。
    void RegisterDefaultInputBindings();
    // targetNameから追従対象GameObjectを探す。
    void ResolveTarget();
    // 同じGameObject上のCameraComponentを取得する。
    void ResolveCamera();
    // 追従中心のSmoothing状態を初期化する。
    void ResetFollowTracking();
    // 追従対象を設定し、targetNameも同期する。
    void SetTarget(std::shared_ptr<GameObject> target_);

private:
    // 追従対象と実際にViewを持つCameraComponent。
    std::weak_ptr<GameObject> target;
    std::weak_ptr<CameraComponent> camera;
    std::string targetName;

    // Camera距離と現在のPitch/Yaw角度。
    float baseDistance = 6.0f;
    float distance = 6.0f;
    float currentPitch = 20.0f;
    float currentYaw = 0.0f;
    float minPitch = -60.0f;
    float maxPitch = 60.0f;
    float lookSpeedX = 300.0f;
    float lookSpeedY = 200.0f;
    float mouseSensitivity = 0.1f;

    // Target追従のSmoothing速度。横と縦で別にし、上下揺れだけ弱められるようにする。
    float horizontalFollowSpeed = 5.0f;
    float verticalFollowSpeed = 3.0f;

    // Target位置から少し上を注視するためのOffset。
    DirectX::XMFLOAT3 targetOffset{ 0.0f, 1.5f, 0.0f };
    // Smoothing済みの注視中心。
    DirectX::XMFLOAT3 smoothedCameraCenter{ 0.0f, 0.0f, 0.0f };
    bool hasCameraCenter = false;
    bool controlSuppressed = false;
    // LockOn解除などで通常TPS位置へ戻るためのBlend状態。
    bool returnBlendActive = false;
    float returnBlendTimer = 0.0f;
    float returnBlendDuration = 0.35f;
    DirectX::XMFLOAT3 returnBlendStartPosition{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 returnBlendStartLookAt{ 0.0f, 0.0f, 0.0f };
    bool runtimeSettingsActive = false;
    nlohmann::json originalRuntimeSettings;
};

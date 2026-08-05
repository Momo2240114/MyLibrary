#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "IKMapComponent.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class AnimationControllerComponent;
class CameraLockOnComponent;
class GameObject;
class GltfModelCommponennt;

// Applies human LookAt IK to head / upper-body chains.
class HumanLookAtIKComponent : public Component
{
public:
    enum class LOOK_MODE
    {
        // 頭だけを向ける軽いLookAt。
        HEAD_ONLY,
        // 上半身チェーンも含めて向きを補正するLookAt。
        FULL_BODY
    };

    enum class LOOK_TARGET_MODE
    {
        // 名前指定のGameObjectを注視する。
        TARGET_OBJECT,
        // LockOn中の対象を注視する。
        LOCK_ON_TARGET,
        // カメラ前方の仮想点を見る。
        CAMERA_FORWARD,
        // LockOn対象を優先し、無い場合はカメラ前方の仮想点を見る。
        LOCK_ON_OR_CAMERA_FORWARD
    };

    const std::string& GetName() const override;
    void Start() override;
    bool IsLoadReady() const override;
    void LateUpdate(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    void SetLookEnabled(bool enabled_) { lookEnabled = enabled_; }
    bool IsLookEnabled() const { return lookEnabled; }
    void SetUseLockOnTarget(bool enabled_) { lookTargetMode = enabled_ ? LOOK_TARGET_MODE::LOCK_ON_TARGET : LOOK_TARGET_MODE::TARGET_OBJECT; }
    void SetLookTargetMode(LOOK_TARGET_MODE mode_) { lookTargetMode = mode_; }
    void SetTargetName(const std::string& targetName_);

private:
    struct LookRuntimeState
    {
        // チェーンごとのブレンド重みと、初期ローカル前方向を保持するランタイム状態。
        float activeWeight = 0.0f;
        std::vector<DirectX::XMFLOAT3> localForwardAxes;
    };

    bool ResolveDependencies();
    bool ResolveChain(IKMapComponent::IKChain& chain_);
    std::shared_ptr<CameraLockOnComponent> ResolveCameraLockOnComponent();
    std::shared_ptr<GameObject> ResolveTarget();
    bool ResolveLookTargetPosition(DirectX::XMFLOAT3& targetPosition_);
    bool TryResolveLockOnTargetPosition(DirectX::XMFLOAT3& targetPosition_);
    bool TryResolveObjectTargetPosition(DirectX::XMFLOAT3& targetPosition_);
    bool TryResolveCameraForwardTargetPosition(DirectX::XMFLOAT3& targetPosition_);
    bool IsAllowedAnimationState() const;
    LOOK_MODE GetCurrentLookMode() const;

    void ApplyLookChain(
        IKMapComponent::IKChain& chain_,
        LookRuntimeState& state_,
        const DirectX::XMFLOAT3& targetPosition_,
        float deltaTime_,
        LOOK_MODE lookMode_);

    void DrawDebugShapes(
        const IKMapComponent::IKChain& chain_,
        const DirectX::XMFLOAT3& targetPosition_) const;

private:
    std::weak_ptr<IKMapComponent> ikMap;
    // LookAtに必要なModel/Animation/LockOn/Targetは弱参照で保持し、必要時に解決し直す。
    std::weak_ptr<GltfModelCommponennt> model;
    std::weak_ptr<AnimationControllerComponent> animationController;
    std::weak_ptr<CameraLockOnComponent> cameraLockOn;
    std::weak_ptr<GameObject> target;
    std::vector<LookRuntimeState> lookStates;
    // AnimationState名ごとにHEAD_ONLY/FULL_BODYを切り替えるためのマップ。
    std::unordered_map<std::string, LOOK_MODE> stateLookModes;

    std::string targetName = "TestTarget";
    std::string idleStateName = "SS_Idle";

    DirectX::XMFLOAT3 targetOffset = { 0.0f, 1.0f, 0.0f };
    // 注視対象の決め方と、デバッグ表示に使う色設定。
    LOOK_TARGET_MODE lookTargetMode = LOOK_TARGET_MODE::LOCK_ON_TARGET;
    DirectX::XMFLOAT4 debugChainColor = { 0.8f, 0.3f, 1.0f, 0.9f };
    DirectX::XMFLOAT4 debugTargetColor = { 1.0f, 0.25f, 0.15f, 0.9f };

    float headOnlyMaxYawDegrees = 50.0f;
    // 頭だけの可動上限と、カメラ前方注視時の仮想ターゲット距離。
    float headOnlyMaxPitchDegrees = 35.0f;
    float cameraForwardLookDistance = 20.0f;

    bool lookEnabled = true;
    // idleOnlyならIdle系状態でだけLookAtを有効化し、攻撃中のポーズ崩れを避ける。
    bool idleOnly = true;
    bool showDebugShapes = false;
};

#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "IKMapComponent.h"

#include <unordered_map>

class AnimationControllerComponent;
class CameraLockOnComponent;
class GameObject;
class GltfModelCommponennt;

// LOOK_ATチェーンを使って頭や上半身をターゲット方向へ向けるComponent。
class HumanLookAtIKComponent : public Component
{
public:
    // 注視回転を適用するボーン範囲。
    enum class LOOK_MODE
    {
        HEAD_ONLY, // 頭・首付近だけでターゲットを見る。
        FULL_BODY  // 胴体を含めたチェーン全体でターゲットを見る。
    };

    const std::string& GetName() const override;
    void Start() override;
    void LateUpdate(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    void SetLookEnabled(bool enabled_) { lookEnabled = enabled_; }
    bool IsLookEnabled() const { return lookEnabled; }
    void SetUseLockOnTarget(bool enabled_) { useLockOnTarget = enabled_; }
    void SetTargetName(const std::string& targetName_);

private:
    // チェーンごとの実行時状態。
    struct LookRuntimeState
    {
        float activeWeight = 0.0f; // 現在の注視ブレンド率。
        std::vector<DirectX::XMFLOAT3> localForwardAxes; // 各ボーンのローカル正面軸。
    };

    bool ResolveDependencies(); // 必要なComponent参照を取得する。
    bool ResolveChain(IKMapComponent::IKChain& chain_); // RootからTipまでのNodeIndex列を解決する。
    std::shared_ptr<CameraLockOnComponent> ResolveCameraLockOnComponent();
    std::shared_ptr<GameObject> ResolveTarget(); // targetNameから注視対象Objectを取得する。
    bool IsAllowedAnimationState() const; // 現在のStateで注視を許可するか判定する。
    LOOK_MODE GetCurrentLookMode() const; // 現在のStateに対応する注視モードを取得する。

    // LOOK_ATチェーンへターゲット方向の回転を適用する。
    void ApplyLookChain(
        IKMapComponent::IKChain& chain_,
        LookRuntimeState& state_,
        const DirectX::XMFLOAT3& targetPosition_,
        float deltaTime_,
        LOOK_MODE lookMode_);

    // 注視チェーンとターゲット方向をデバッグ描画する。
    void DrawDebugShapes(
        const IKMapComponent::IKChain& chain_,
        const DirectX::XMFLOAT3& targetPosition_) const;

private:
    std::weak_ptr<IKMapComponent> ikMap; // LOOK_ATチェーン設定を持つIKMapComponent。
    std::weak_ptr<GltfModelCommponennt> model; // ボーンNodeの取得・更新に使うモデルComponent。
    std::weak_ptr<AnimationControllerComponent> animationController; // 現在のAnimationState取得に使うComponent。
    std::weak_ptr<CameraLockOnComponent> cameraLockOn;
    std::weak_ptr<GameObject> target; // 解決済みの注視対象Object。
    std::vector<LookRuntimeState> lookStates; // 各LOOK_ATチェーンに対応する実行時状態。
    std::unordered_map<std::string, LOOK_MODE> stateLookModes; // State名ごとの注視モード設定。

    std::string targetName = "TestTarget"; // 注視対象として検索するObject名。
    std::string idleStateName = "SS_Idle"; // idleOnly時に注視を許可するState名。

    DirectX::XMFLOAT3 targetOffset = { 0.0f, 1.0f, 0.0f }; // ターゲット位置に加算する注視位置補正。
    DirectX::XMFLOAT4 debugChainColor = { 0.8f, 0.3f, 1.0f, 0.9f }; // チェーン描画用の色。
    DirectX::XMFLOAT4 debugTargetColor = { 1.0f, 0.25f, 0.15f, 0.9f }; // ターゲット方向描画用の色。

    float headOnlyMaxYawDegrees = 50.0f;   // HEAD_ONLY時の左右注視角度制限
    float headOnlyMaxPitchDegrees = 35.0f; // HEAD_ONLY時の上下注視角度制限

    bool lookEnabled = true; // LookAt IKを有効にするか。
    bool useLockOnTarget = true;
    bool idleOnly = true; // Idle状態の時だけ注視するか。
    bool showDebugShapes = false; // デバッグ形状を描画するか。
};

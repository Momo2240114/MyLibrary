#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "IKMapComponent.h"

#include <memory>
#include <vector>

class GltfModelCommponennt;
class HitBox;

class ShieldBlockIKComponent : public Component
{
public:
    const std::string& GetName() const override;
    void Start() override;
    bool IsLoadReady() const override;
    void LateUpdate(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

private:
    struct ShieldRuntimeState
    {
        // 攻撃方向へShield姿勢を寄せる現在Weight。
        float activeWeight = 0.0f;
    };

    // IKMap/GltfModelなど必要Componentを解決する。
    bool ResolveDependencies();
    // 設定されたIK ChainのNode参照を使用可能な状態にする。
    bool ResolveChain(IKMapComponent::IKChain& chain_);
    // Chain基準位置から最も近い攻撃HitBoxを探す。
    bool FindNearestAttack(
        const IKMapComponent::IKChain& chain_,
        const DirectX::XMFLOAT3& origin_,
        DirectX::XMFLOAT3& attackPosition_) const;
    // 攻撃方向をChainごとの制限内へ丸める。
    DirectX::XMFLOAT3 ClampDirection(
        const IKMapComponent::IKChain& chain_,
        const DirectX::XMFLOAT3& from_,
        const DirectX::XMFLOAT3& to_) const;
    // 2Bone IKでShield用の解決済みBone位置を作る。
    bool SolveTwoBone(
        const IKMapComponent::IKChain& chain_,
        const std::vector<DirectX::XMFLOAT3>& animatedPositions_,
        const DirectX::XMFLOAT3& targetDirection_,
        std::vector<DirectX::XMFLOAT3>& solvedPositions_) const;
    // 解いた姿勢をModel NodeへWeight付きで反映する。
    void ApplySolvedPose(
        const IKMapComponent::IKChain& chain_,
        const std::vector<DirectX::XMFLOAT3>& solvedPositions_,
        float weight_);
    // Editor Debug用にChainとTargetを描画する。
    void DrawDebugShapes(
        const IKMapComponent::IKChain& chain_,
        const std::vector<DirectX::XMFLOAT3>& solvedPositions_,
        const DirectX::XMFLOAT3& attackPosition_) const;

private:
    // IK定義とModel本体への参照。
    std::weak_ptr<IKMapComponent> ikMap;
    std::weak_ptr<GltfModelCommponennt> model;
    // Chainごとの実行時Weight状態。
    std::vector<ShieldRuntimeState> shieldStates;
    // Shield IK全体の有効状態。
    bool enabled = true;
    // Debug描画設定。
    bool showDebugShapes = false;
    DirectX::XMFLOAT4 debugChainColor = { 0.1f, 0.7f, 1.0f, 0.9f };
    DirectX::XMFLOAT4 debugTargetColor = { 1.0f, 0.2f, 0.1f, 0.9f };
};

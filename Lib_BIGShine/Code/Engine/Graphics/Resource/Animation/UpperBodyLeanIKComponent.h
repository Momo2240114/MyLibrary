#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "IKMapComponent.h"

#include <string>
#include <vector>

class AnimationControllerComponent;
class GltfModelCommponennt;

class UpperBodyLeanIKComponent : public Component
{
public:
    const std::string& GetName() const override;
    void Start() override;
    bool IsLoadReady() const override;
    void LateUpdate(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    // 上半身Leanの有効状態を外部から切り替える。
    void SetLeanEnabled(bool enabled_) { leanEnabled = enabled_; }
    // 手動の移動方向を指定する。
    void SetMoveDirection(const DirectX::XMFLOAT3& direction_);
    // 手動の移動速度を指定する。
    void SetMoveVelocity(const DirectX::XMFLOAT3& velocity_);
    void SetLeanStrength(float strength_) { leanStrength = strength_; }
    // 現在のLean角度と履歴を初期化する。
    void ResetLean();

    // Animation名による適用Filterを切り替える。
    void SetUseAnimationFilter(bool enabled_) { useAnimationFilter = enabled_; }
    // Leanを適用するAnimation名を追加する。
    void AddApplyAnimationName(const std::string& animationName_);
    void ClearApplyAnimationNames();
    // 現在再生中AnimationがFilter条件を満たすか判定する。
    bool ShouldApplyForCurrentAnimation() const;

private:
    // IKMap/Model/AnimationControllerを解決する。
    bool ResolveDependencies();
    // 指定Chain名のIK Chainを解決する。
    bool ResolveChain(IKMapComponent::IKChain& chain_);
    // Owner移動または手動指定からLean用速度を求める。
    DirectX::XMFLOAT3 ResolveMoveVelocity(float deltaTime_);
    // World方向をOwnerローカル方向へ変換する。
    DirectX::XMFLOAT3 ToOwnerLocalDirection(const DirectX::XMFLOAT3& worldDirection_) const;
    // 移動方向と速度から現在Lean角度を更新する。
    void UpdateLeanAngles(const DirectX::XMFLOAT3& localMoveDirection_, float speed_, float deltaTime_, bool shouldApply_);
    // Chain内BoneへLean回転をWeight付きで反映する。
    void ApplyLeanChain(IKMapComponent::IKChain& chain_);
    void DrawDebugShapes(const IKMapComponent::IKChain& chain_) const;
    // AnimationControllerから現在再生中のAnimation名一覧を取得する。
    std::vector<std::string> GetCurrentAnimationNames() const;
    // Filter文字列と現在Animation名の一致を判定する。
    bool MatchesAnimationName(const std::string& currentName_) const;

    // Leanに必要な関連Component。
    std::weak_ptr<IKMapComponent> ikMap;
    std::weak_ptr<GltfModelCommponennt> model;
    std::weak_ptr<AnimationControllerComponent> animationController;

    // Leanを適用するIK Chain名とBoneごとのWeight。
    std::string ikChainName = "UpperBodyLean";
    std::vector<float> boneWeights = { 0.15f, 0.35f, 0.35f, 0.15f };
    // Animation Filter用の名前一覧。
    std::vector<std::string> applyAnimationNames = { "Walk", "Run", "Dash" };
    std::string animationNameInput;

    // 手動入力とOwner移動から求める移動情報。
    DirectX::XMFLOAT3 manualMoveDirection = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 manualMoveVelocity = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 previousOwnerPosition = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 debugLocalMoveDirection = { 0.0f, 0.0f, 0.0f };

    // Lean角度の上限と補間速度。
    float maxForwardLeanAngle = 15.0f;
    float maxSideLeanAngle = 10.0f;
    float leanStrength = 1.0f;
    float smoothSpeed = 10.0f;
    float returnSpeed = 7.0f;
    float minMoveSpeed = 0.05f;
    float currentForwardLean = 0.0f;
    float currentSideLean = 0.0f;
    float debugMoveSpeed = 0.0f;

    // Leanの入力/Debug/Weight正規化設定。
    bool leanEnabled = true;
    bool useOwnerVelocity = true;
    bool useManualDirection = false;
    bool hasManualVelocity = false;
    bool hasPreviousOwnerPosition = false;
    bool allowBackwardLean = true;
    float backwardLeanScale = 0.45f;
    bool autoNormalizeWeights = true;
    bool showDebugShapes = false;
    bool showDebugText = true;
    bool chainMissingWarning = false;

    // Animation名Filterの挙動設定。
    bool useAnimationFilter = true;
    bool usePartialNameMatch = true;
    bool invertAnimationFilter = false;
    bool disableWhenNoAnimationMatched = true;
};

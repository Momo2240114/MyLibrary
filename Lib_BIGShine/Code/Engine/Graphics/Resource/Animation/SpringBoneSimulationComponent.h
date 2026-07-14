#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "IKMapComponent.h"

class GltfModelCommponennt;

// IKMapComponentに登録された髪・布チェーンを物理計算するComponent
class SpringBoneSimulationComponent : public Component
{
public:
    // 布チェーン同士をつなぐ距離制約
    struct ClothConstraint
    {
        size_t chainIndexA = 0;     // 制約元チェーンの番号
        size_t pointIndexA = 0;     // 制約元チェーン内の点番号
        size_t chainIndexB = 0;     // 制約先チェーンの番号
        size_t pointIndexB = 0;     // 制約先チェーン内の点番号
        float targetLength = 0.0f;  // 初期姿勢から求めた目標距離
        bool diagonal = false;      // 斜め方向の制約か
    };

    const std::string& GetName() const override;
    void Start() override;
    bool IsLoadReady() const override;
    void LateUpdate(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    // シミュレーション状態を初期姿勢から作り直す
    void ResetSimulation();

    // 全体共通の風方向
    static DirectX::XMFLOAT3 windDirection;

    // 全体共通の風の強さ
    static float windStrength;

    // 風を有効にするか
    static bool windEnabled;

private:
    // IKMapComponentとモデルComponentを取得する
    bool ResolveDependencies();

    // rootNodeNameからtipNodeNameまでのボーン列を解決する
    bool ResolveChain(IKMapComponent::IKChain& chain_);

    // 1本の髪・布チェーンを物理計算する
    void SimulateSpringChain(IKMapComponent::IKChain& chain_, float deltaTime_);

    // 布チェーン同士の制約を作成する
    void BuildClothConstraints();

    // すべての布制約を解決する
    void SolveClothConstraints();

    // 1つの布制約を解決する
    void SolveClothConstraint(const ClothConstraint& constraint_);

    // 髪・布の点を体コリジョンから押し出す
    void ResolveBodyCollision(IKMapComponent::IKChain& chain_, size_t pointIndex_);

    // 体コリジョンSphereのワールド座標中心を取得する
    bool GetBodyCollisionSphereCenter(IKMapComponent::BodyCollisionSphere& sphere_, DirectX::XMFLOAT3& center_);

    // 計算結果をモデルのボーン回転へ反映する
    void ApplySpringChain(IKMapComponent::IKChain& chain_);

    // 体コリジョン形状をデバッグ描画する
    void DrawBodyCollisionShapes();

    // 髪・布側のコリジョン形状をデバッグ描画する
    void DrawHairCollisionShapes();

    // 髪・布の点が体の前側へ回り込まないように制限する
    void ConstrainHairBehindBody(IKMapComponent::IKChain& chain_);
private:
    // 髪・布チェーン設定を持つComponent
    IKMapComponent* ikMap = nullptr;

    // 揺れを反映する対象モデル
    GltfModelCommponennt* model = nullptr;

    // 布チェーン同士をつなぐ制約リスト
    std::vector<ClothConstraint> clothConstraints;

    // 布制約を作成済みか
    bool clothConstraintsInitialized = false;

    // 体コリジョンを表示するか
    bool showBodyCollisionShapes = true;

    // 体コリジョンの表示色
    DirectX::XMFLOAT4 bodyCollisionShapeColor = { 1.0f, 0.45f, 0.1f, 1.0f };

    // 髪・布コリジョンを表示するか
    bool showHairCollisionShapes = false;

    // 髪・布コリジョンの表示色
    DirectX::XMFLOAT4 hairCollisionShapeColor = { 0.2f, 0.8f, 1.0f, 0.6f };
};

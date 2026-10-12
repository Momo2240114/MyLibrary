#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/GamePlay/EventSystem/IActionProvider.h"
#include <string>

// 既存バリアと環境Particleを同じ補間率で第二形態へ遷移させる。
class StageCorruptionComponent : public Component, public IActionProvider
{
public:
    const std::string& GetName() const override
    {
        static const std::string NAME = "StageCorruptionComponent";
        return NAME;
    }
    // 遷移前の設定を保存し、開始・完了状態・復元Actionを公開する。
    void Start() override;
    // CurveまたはSmoothStepを使い、環境の侵食率を更新する。
    void Update(float deltaTime_) override;
    // 参照先と時間、色、補間CurveをEditorから調整する。
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;
    void RegisterActions() override;
    // 既存のHumanBoss用Scene設定から移行先半径を読み込む。
    bool Prepare();
    // 発生量0から第二形態の環境への補間を開始する。
    void Begin();
    // 第一形態の環境へ戻し、第二形態の残留粒子を消す。
    void Restore();
    // 途中再生の準備でも使用できる、侵食済みの端点へ切り替える。
    void Complete();

private:
    // 見た目と物理壁へ同じ半径を渡し、発生済みの粒子は寿命まで残す。
    void Apply(float rate_);
    std::string barrierName = "Wall";
    std::string firstParticleName = "Stage";
    std::string secondParticleName = "Phase2Environment";
    std::string targetBarrierAssetPath = "Data/Scenes/HumanBossTest/Objects/Wall.json";
    std::string curveName;
    std::string status;
    float duration = 4.0f;
    bool smoothTransition = true;
    DirectX::XMFLOAT4 targetColor{ 0.32f, 0.015f, 0.04f, 0.4f };
    DirectX::XMFLOAT4 targetEdgeColor{ 0.5f, 0.025f, 0.035f, 1.0f };
    float firstRadius = 0.0f;
    float secondRadius = 0.0f;
    float visualRadius = 1.0f;
    DirectX::XMFLOAT4 firstColor{ 1.0f, 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT4 firstEdgeColor{ 1.0f, 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT4 firstRingColor{ 1.0f, 1.0f, 1.0f, 1.0f };
    float elapsed = 0.0f;
    float currentRate = 0.0f;
    bool prepared = false;
    bool playing = false;
};

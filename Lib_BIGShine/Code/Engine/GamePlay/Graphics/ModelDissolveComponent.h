#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/GamePlay/EventSystem/IActionProvider.h"
#include "Engine/Graphics/Resource/ModelMaterial.h"

// モデルの消滅・形成に使う描画設定。元のglTF Materialは変更しない。
struct ModelDissolveSettings
{
    float edgeWidth = 0.06f;
    float edgeEmission = 2.0f;
    float noiseScale = 3.0f;
    float directionalWeight = 0.4f;
    DirectX::XMFLOAT3 direction{ 0.0f, 1.0f, 0.0f };
    float directionExtent = 10.0f;
    DirectX::XMFLOAT4 edgeColor{ 0.12f, 0.01f, 0.18f, 1.0f };
};

// SequenceからモデルのDissolveを補間し、完了後も消滅状態を保持する。
class ModelDissolveComponent : public Component, public IActionProvider
{
public:
    const std::string& GetName() const override
    {
        static const std::string NAME = "ModelDissolveComponent";
        return NAME;
    }

    // 保存設定を準備し、消滅・形成・復元Actionを公開する。
    void Start() override;
    // モデルのロード完了後にDissolveの進行度を更新する。
    void Update(float deltaTime_) override;
    // Noiseテクスチャを既存のMaterialローダーで準備する。
    void UpdateLoadState() override;
    // 境界、方向、Noise、再生時間を編集する。
    void Gui() override;
    // 描画設定だけを保存し、再生途中の進行度は保存しない。
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;
    void RegisterActions() override;
    // visible_で指定した端点へ、補間を中断して復元する。
    void Reset(bool visible_);
    // 表示中から消滅、または非表示から形成を開始する。
    void Play(bool reveal_);
    // 演出の直接シークは通常の消滅再生を変更せず、一時的な進行度だけを上書きする。
    void SetPresentationProgress(float progress_);

    void ClearPresentationProgress()
    {
        presentationProgress = -1.0f;
    }

    float GetProgress() const
    {
        return presentationProgress >= 0.0f ? presentationProgress : progress;
    }

    const ModelDissolveSettings& GetSettings() const
    {
        return settings;
    }

    ID3D11ShaderResourceView* GetNoiseTexture() const;

private:
    ModelDissolveSettings settings;
    ModelMaterial noiseMaterial;
    std::string noiseTexturePath;
    float disappearDuration = 4.0f;
    float revealDuration = 3.0f;
    bool startVisible = true;
    float presentationProgress = -1.0f;
    float progress = 0.0f;
    float startProgress = 0.0f;
    float targetProgress = 0.0f;
    float elapsed = 0.0f;
    float duration = 0.0f;
    bool playing = false;
};

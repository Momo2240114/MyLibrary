#pragma once
#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include <array>
#include <cstdint>
#include <deque>
#include <DirectXMath.h>
#include <d3d11.h>
#include <wrl/client.h>

// 画像とFontUIで共有する、任意に組み合わせ可能な見た目の設定。
struct UIVisualEffectSettings
{
    DirectX::XMFLOAT4 glowColor{0.6f,0.9f,1,1}, outlineColor{1,1,1,1}, shadowColor{0,0,0,0.6f};
    DirectX::XMFLOAT4 sweepColor{1,1,1,1}, gradientTop{1,1,1,1}, gradientBottom{1,1,1,1}, burnColor{1,0.2f,0.02f,1};
    DirectX::XMFLOAT4 trailColor{0.65f,0.9f,1,0.45f};
    DirectX::XMFLOAT2 shadowOffset{3,3}, uvScroll{0,0};
    float glowIntensity = 0, glowRadius = 8, outlineWidth = 0, shadowOpacity = 0, shadowSoftness = 2;
    float sweepIntensity = 0, sweepWidth = 0.15f, sweepAngle = -30, sweepSpeed = 0.6f;
    float pulseAmount = 0, pulseSpeed = 1, grayscale = 0, sepia = 0, invert = 0, pixelSize = 1;
    float blurRadius = 0, gradientStrength = 0, dissolve = 0, edgeWidth = 0.06f, noiseScale = 12;
    float paintStrength = 0.0f, inkStrength = 0.0f;
    DirectX::XMFLOAT2 maskScale{1,1}, maskOffset{0,0};
    bool maskInvert = false;
    int trailCount = 0;
    float trailLifetime = 0.25f, trailInterval = 0.035f, trailScale = 1;
};

// Rendererの頂点型に依存せず、過去の字形・色・位置を保持する。
struct UIEffectTrailSample
{
    std::array<DirectX::XMFLOAT3,4> positions;
    std::array<DirectX::XMFLOAT2,4> texcoords;
    std::array<DirectX::XMFLOAT4,4> colors;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture;
    float time = 0;
};

// SpriteRenderer経由で既存画像・UIText・ボタン装飾へ共通の効果を付加する。
class UIVisualEffectComponent : public Component
{
public:
    const std::string& GetName() const override;
    // プリセットとActionを準備し、実行時の再生を開始する。
    void Start() override;
    void OnDestroy() override;
    // ポーズから独立した効果時間と残像の寿命を更新する。
    void UpdateUnscaled(float deltaTime_) override;
    // MeshVFX共通のTexture選択と、効果の試写・保存を行う。
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;
    void Play(bool preview_ = false);
    void Stop();
    void SetProgress(float progress_);
    // 描画専用Spriteで、演出時計からシェーダーの時刻を指定する。
    void SetSampleTime(float time_);
    // 再生時間を維持したまま、選択状態に応じた効果量だけを調整する。
    void SetRuntimeStrength(float strength_);
    void SetRuntimeDissolve(float dissolve_);

    float GetRuntimeDissolve() const
    {
        return runtimeDissolve;
    }

    bool LoadPreset(const std::string& name_);

    const UIVisualEffectSettings& GetSettings() const
    {
        return parameters;
    }

    ID3D11ShaderResourceView* GetMaskTexture() const
    {
        return maskTexture.Get();
    }

    // 解像度が変わった時点で、旧NDC座標の残像を破棄する。
    void PrepareTrailView(const DirectX::XMFLOAT2& screenSize_);

    float GetTime() const
    {
        return playTime;
    }

    float GetStrength() const
    {
        return (restored ? 0.0f : tweenStrength ? progress : 1.0f) * runtimeStrength;
    }

    float GetTrailTime() const
    {
        return trailTime;
    }

    const std::deque<UIEffectTrailSample>& GetTrails() const
    {
        return trails;
    }

    // 複数Viewで描画されても、更新一回につき一つだけ履歴を記録する。
    void CaptureTrail(UIEffectTrailSample sample_,const DirectX::XMFLOAT2& screenSize_);
    void ClearTrails();

private:
    // JSONを型付き設定へ正規化し、描画中の不正値と過剰な履歴枚数を防ぐ。
    void Normalize();
    nlohmann::json settings = nlohmann::json::object();
    nlohmann::json catalog = nlohmann::json::object();
    UIVisualEffectSettings parameters;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> maskTexture;
    std::string maskTexturePath;
    std::string preset = "SoftGlow", saveName = "CustomEffect", status;
    bool restored = false;
    bool playOnStart = true, playing = false, paused = false, preview = false, tweenStrength = false;
    float runtimeStrength = 1.0f;
    float runtimeDissolve = 0.0f;
    float playTime = 0, trailTime = 0, progress = 0;
    std::uint64_t frameSerial = 0, capturedSerial = UINT64_MAX;
    DirectX::XMFLOAT2 trailScreenSize{0,0};
    std::deque<UIEffectTrailSample> trails;
};

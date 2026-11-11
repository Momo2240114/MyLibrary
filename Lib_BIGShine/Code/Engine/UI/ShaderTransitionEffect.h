#pragma once
#include "ITransitionEffect.h"
#include <d3d11.h>
#include <wrl/client.h>
// 専用PS以外の全画面描画、定数バッファとTexture読込を共有する基底実装。
class ShaderTransitionEffect : public ITransitionEffect
{
public:
    explicit ShaderTransitionEffect(const char* shaderPath_);
    bool Initialize() override;
    void LoadResources() override;
    void SetParameters(const TransitionParameters& parameters_) override;
    void SetProgress(float progress_) override { progress = progress_; }
    void Update(float elapsedTime_, bool revealing_) override { elapsedTime = elapsedTime_; revealing = revealing_; }
    bool Render(ID3D11RenderTargetView* renderTarget_) override;
    bool IsReady() const override { return ready; }
    static void ShutdownSharedResources();
protected:
    static void DrawEdges(TransitionParameters& parameters_);
    static void DrawDirection(TransitionParameters& parameters_);
    static void DrawTexture(TransitionParameters& parameters_,const char* key_,const char* label_);
    static void DrawMaskUV(TransitionParameters& parameters_);
    // 任意画像の輪郭を拡縮する設定を、中央開口とタイル演出で共有する。
    static void DrawShapeMask(TransitionParameters& parameters_);
    TransitionParameters parameters;
private:
    Microsoft::WRL::ComPtr<ID3D11Texture2D> snapshotTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> snapshotView;
    bool snapshotRevealing = false;
    std::string shaderPath;
    float progress = 0;
    float elapsedTime = 0;
    bool revealing = false;
    bool ready = false;
    bool hasTexture = false;
    bool hasGlow = false;
    float shapeTextureAspect = 1.0f;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> glassVertexShader;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> noiseTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> effectTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> glowTexture;
};

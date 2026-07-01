#pragma once

#include <windows.h>
#include <tchar.h>
#include <sstream>
#include <d3d11.h>
#include <wrl.h>

#include "Engine/Common/misc.h"
#include "Engine/Graphics/RenderSystem/ShapeRenderer.h"
#include "Engine/Graphics/RenderSystem/SpriteRenderSystem.h"
#include "Engine/Graphics/RenderSystem/ModelRenderSystem.h"
#include "RenderState.h"
#include "Engine/Graphics/RenderSystem/BloomSystem.h"
#include "Engine/Graphics/RenderSystem/PostRenderSystem.h"
#include "Engine/Graphics/RenderSystem/RadialBlurSystem.h"

#include "Engine/Graphics/RenderSystem/CascadeShadowMap.h"

// ============================================================================
// GraphicsDeviceManager
// Direct3D11デバイス、描画ターゲット、ポストエフェクトの生存期間を一元管理する。
// ============================================================================
class GraphicsDeviceManager
{
public:
    GraphicsDeviceManager() = default;
    ~GraphicsDeviceManager() = default;

    //====================================================
    // 初期化と破棄
    //====================================================
    bool Initialize(HWND windowHandle_, int width_, int height_, bool fullscreen_);

    void Shutdown();
    void OnResize(int newWidth_, int newHeight_);
    void SetFullscreen(bool enabled_);
    bool IsFullscreen() const { return fullscreen; }
    void Finalize();
    //====================================================
    // フレーム制御
    //====================================================
    void BeginFrame(const float clearColor_[4]);
    void EndFrame();

    //====================================================
    // アクセッサ
    //====================================================
    ID3D11Device* GetDevice() const { return device.Get(); }
    ID3D11DeviceContext* GetContext() const { return immediateContext.Get(); }
    IDXGISwapChain* GetSwapChain() const { return swapChain.Get(); }
    ID3D11RenderTargetView* GetRenderTargetView() const { return renderTargetView.Get(); }
    ID3D11DepthStencilView* GetDepthStencilView() const { return depthStencilView.Get(); }
    ID3D11ShaderResourceView* GetMainDepthSRV() const { return depthStencilSRV.Get(); }
    HWND GetWindowHandle() { return windowHandle; }
    ShapeRenderSystem& GetShapeRendererSystem() { return shapeRendererSystem; }
    // レンダーステート取得
    RenderState* GetRenderState() { return renderState.get(); }
    // アクセッサ欄に追加
    ID3D11ShaderResourceView* GetSceneSRV(){ return sceneSRV.Get(); }
    ID3D11ShaderResourceView* GetNormalSRV() { return gBufferNormalSRV.Get(); }
    ID3D11ShaderResourceView* GetSceneNormalSRV() { return sceneNormalSRV.Get(); }
    ID3D11ShaderResourceView* GetSceneDistortionSRV() { return sceneDistortionSRV.Get(); }
    bool IsSceneDistortionUsed() const { return sceneDistortionUsed; }
    ID3D11ShaderResourceView* GetGBufferBaseColorSRV() { return gBufferBaseColorSRV.Get(); }
    ID3D11ShaderResourceView* GetGBufferNormalSRV() { return gBufferNormalSRV.Get(); }
    ID3D11ShaderResourceView* GetGBufferMaterialSRV() { return gBufferMaterialSRV.Get(); }
    ID3D11ShaderResourceView* GetGBufferEmissiveSRV() { return gBufferEmissiveSRV.Get(); }

    ID3D11ShaderResourceView* GetIBLDiffuseSRV() { return iblDiffuseIemSRV.Get(); }
    ID3D11ShaderResourceView* GetIBLSpecularSRV() { return iblSpecularPmremSRV.Get(); }
    ID3D11ShaderResourceView* GetIBLLutGGXSRV() { return iblLutGgxSRV.Get(); }

    CascadeShadowMap& GetCascadeShadowMap() { return cascadeShadowMap; }
    PostRenderSystem& GetPostRenderSystem() { return postRenderSystem; }
    //====================================================
    // Instanceアクセッサ
    //====================================================
    static GraphicsDeviceManager& Instance() {
        static GraphicsDeviceManager instance;
        return instance;
    }

    // ブルーム設定の取得・操作用
    BloomSystem::Parameters& GetBloomParams() { return bloomParams; }
    RadialBlurSystem::Parameters& GetRadialBlurParams() { return radialBlurParams; }
    XeGtaoSystem::Parameters& GetXeGtaoParams() { return xeGtaoParams; }
    MotionBlurSystem::Parameters& GetMotionBlurParams() { return motionBlurParams; }
    VignetteSystem::Parameters& GetVignetteParams() { return vignetteParams; }
    BloomSystem::CbColorFilter& GetFilterParams() { return filterParams; }

    // パーティクルはカラーバッファにだけ影響を与えたい
    void SetParticleRenderTarget();
    void SetParticleRenderTargetWithDistortion();

    // バックバッファではなく「シーン用テクスチャ」をセットする関数
    void SetSceneRenderTarget();
    void SetSceneColorRenderTarget();
    void SetSceneDistortionRenderTarget();
    void SetGBufferRenderTargets();
    void SetBackBufferRenderTarget();
    // シーン用テクスチャからバックバッファへブルームを適用して描画
    void ResolveBloom(const RenderContext& rc);
    void RenderSkyBox(const RenderContext& rc);
    // ブルーム調整用GUI
    void ResetFilter();
    void DrawPostEffectGuiContent();
    void ShowPostEffectGui();
private:
    ShapeRenderSystem shapeRendererSystem;
    PostRenderSystem postRenderSystem;

    //====================================================
    // DirectX11の中核リソース。破棄順序を制御するため、このクラスが所有する。
    //====================================================
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> immediateContext;
    Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTargetView;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthStencilView;


public:
    Microsoft::WRL::ComPtr<ID3D11Texture2D> copyDepthStencilTexture; // 深度バッファ本体
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> copyDepthStencilSRV;

    Microsoft::WRL::ComPtr<ID3D11Texture2D>          copySceneColorTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> copySceneColorSRV;

    Microsoft::WRL::ComPtr<ID3D11Texture2D>          copyNormalTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> copyNormalSRV;

    std::unique_ptr<RenderState>					renderState;

    // --- 深度リソース本体とSRV ---
    Microsoft::WRL::ComPtr<ID3D11Texture2D> depthStencilTexture; // 深度バッファ本体
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> depthStencilSRV; // 読み取り用

    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   sceneNormalRTV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> sceneNormalSRV;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   sceneDistortionRTV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> sceneDistortionSRV;
private:

    HWND											windowHandle = nullptr;

    //====================================================
    // ウィンドウサイズと描画領域に依存する実行時状態。
    //====================================================
    D3D11_VIEWPORT viewport;
    D3D_FEATURE_LEVEL featureLevels[2] =
    {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };

    DXGI_SWAP_CHAIN_DESC swapChainDesc{};
    int width = 0;
    int height = 0;
    bool fullscreen = false;
    bool isChangingFullscreen = false;
    WINDOWPLACEMENT windowedPlacement{ sizeof(WINDOWPLACEMENT) };
    DWORD windowedStyle = WS_OVERLAPPEDWINDOW;
    bool vsyncEnabled = false;

    //====================================================
    // 内部ヘルパ関数
    //====================================================
    bool CreateDeviceAndSwapChain(HWND hwnd_);
    bool CreateRenderTarget();
    bool CreateDepthStencil();
    void ReleaseResources();
    void SetRenderTargets();
private:
    // --- ブルーム関連リソース ---
    BloomSystem bloomSystem;
    BloomSystem::Parameters bloomParams;
    BloomSystem::CbColorFilter filterParams;
    RadialBlurSystem::Parameters radialBlurParams;
    XeGtaoSystem::Parameters xeGtaoParams;
    MotionBlurSystem::Parameters motionBlurParams;
    VignetteSystem::Parameters vignetteParams;

    CascadeShadowMap cascadeShadowMap;

    // ポストプロセス用のソーステクスチャ (HDR対応)
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   sceneRTV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> sceneSRV;
    bool sceneDistortionUsed = false;

    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> gBufferBaseColorRTV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> gBufferBaseColorSRV;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> gBufferNormalRTV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> gBufferNormalSRV;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> gBufferMaterialRTV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> gBufferMaterialSRV;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> gBufferEmissiveRTV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> gBufferEmissiveSRV;

    // -------------------------------------------------------
    // IBL テクスチャ
    // register(t10) diffuse_iem
    // register(t11) specular_pmrem
    // register(t12) lut_ggx
    // -------------------------------------------------------
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> iblDiffuseIemSRV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> iblSpecularPmremSRV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> iblLutGgxSRV;

    bool LoadIBLTextures();

    // -------------------------------------------------------
    // スカイボックス
    // -------------------------------------------------------
    Microsoft::WRL::ComPtr<ID3D11VertexShader> skyBoxVertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>  skyBoxPixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>  skyBoxInputLayout;
    Microsoft::WRL::ComPtr<ID3D11Buffer> skyBoxVertexBuffer; // NDC フルスクリーンクワッド
    Microsoft::WRL::ComPtr<ID3D11Buffer> skyBoxIndexBuffer;


    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> skyBoxDepthStencilState;

    bool InitializeSkyBox();

    // 内部ヘルパー
    bool CreateSceneRenderTarget(int width_, int height_);
    bool CreateGBufferTargets(int width_, int height_);
};

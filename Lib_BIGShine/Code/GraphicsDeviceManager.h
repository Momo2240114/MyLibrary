#pragma once

#include <windows.h>
#include <tchar.h>
#include <sstream>
#include <d3d11.h> 
#include <wrl.h> 

#include "misc.h"
#include "ShapeRenderer.h"
#include "SpriteRenderSystem.h"
#include "ModelRenderSystem.h"
#include "RenderState.h"
#include "BloomSystem.h"

// ============================================================================
// GraphicsDeviceManager
//  Direct3D11 デバイスとスワップチェーン、描画制御を管理するクラス
// ============================================================================
class GraphicsDeviceManager
{
public:
    GraphicsDeviceManager() = default;
    ~GraphicsDeviceManager() = default;

    //====================================================
    // 初期化と破棄
    //====================================================
    bool Initialize(HWND hWnd_, int width_, int height_, bool fullscreen_);

    void Shutdown();
    void OnResize(int newWidth_, int newHeight_);
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
    HWND GetWindowHandle() { return hWnd; }
    SpriteRenderSystem& GetSpriteRenderSystem() { return spriteRenderSystem; }
    ModelRenderSystem& GetModelRenderSystem() { return modelRenderSystem; }
    ShapeRenderSystem& GetShapeRendererSystem() { return shapeRendererSystem; }
    // レンダーステート取得
    RenderState* GetRenderState() { return renderState.get(); }
    // アクセッサ欄に追加
    ID3D11ShaderResourceView* GetSceneSRV(){ return sceneSRV.Get(); }
    ID3D11ShaderResourceView* GetNormalSRV() { return sceneNormalSRV.Get(); }

    //====================================================
    // Instanceアクセッサ
    //====================================================
    static GraphicsDeviceManager& Instance() {
        static GraphicsDeviceManager instance;
        return instance;
    }

    // ブルーム設定の取得・操作用
    BloomSystem::Parameters& GetBloomParams() { return bloomParams; }
    BloomSystem::CbColorFilter& GetFilterParams() { return filterParams; }

    // バックバッファではなく「シーン用テクスチャ」をセットする関数
    void SetSceneRenderTarget();
    // シーン用テクスチャからバックバッファへブルームを適用して描画
    void ResolveBloom(const RenderContext& rc);
    // ブルーム調整用GUI
    void ResetFilter();
    void ShowPostEffectGui();
private:
    SpriteRenderSystem spriteRenderSystem;
    ModelRenderSystem modelRenderSystem;
    ShapeRenderSystem shapeRendererSystem; 
    //====================================================
    // Instance参照用
    //====================================================

    //====================================================
    // DirectX11 リソース
    //====================================================
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> immediateContext;
    Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTargetView;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthStencilView;


public:
    Microsoft::WRL::ComPtr<ID3D11Texture2D> copyDepthStencilTexture; // 深度バッファ本体
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> copyDepthStenciSRV;

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
private:

    HWND											hWnd = nullptr;

    //====================================================
    // 内部状態
    //====================================================
    D3D11_VIEWPORT viewPort;
    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    DXGI_SWAP_CHAIN_DESC swapChainDesc{};
    int width = 0;
    int height = 0;
    bool fullscreen = false;

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

    // ポストプロセス用のソーステクスチャ (HDR対応)
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   sceneRTV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> sceneSRV;

    // 内部ヘルパー
    bool CreateSceneRenderTarget(int width_, int height_);
};
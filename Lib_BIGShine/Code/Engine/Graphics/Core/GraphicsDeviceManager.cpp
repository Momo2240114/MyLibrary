
#include "GraphicsDeviceManager.h"
#include "IMGUI.h"

#include <d3d11.h>
#include <d3d11sdklayers.h>

//---
//@brief Direct3D 11 デバイスとスワップチェーンを初期化する
//@param hwnd_ ウィンドウハンドル
//@param width_ 描画領域の幅
//@param height_ 描画領域の高さ
//@param fullscreen_ フルスクリーンモードかどうか
//@return 初期化成功で true、失敗で false
//---
bool GraphicsDeviceManager::Initialize(HWND hWnd_, int width_, int height_, bool fullscreen_)
{
    HRESULT hr_ = S_OK;
    UINT createDeviceFlags_ = 0;
#if defined(DEBUG) || defined(_DEBUG)
    createDeviceFlags_ |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    // 引数をメンバ変数に保存
    width =  width_;
    height = height_;
    fullscreen = fullscreen_;
    hWnd = hWnd_;

    // TODO: デバイスとスワップチェーンを作成
    if (!CreateDeviceAndSwapChain(hWnd_))
    {
        _ASSERT_EXPR(false, L"Failed: CreateDeviceAndSwapChain");
        return false;
    }

    // TODO: バックバッファRTVを作成
    if (!CreateRenderTarget())
    {
        _ASSERT_EXPR(false, L"Failed: CreateRenderTarget");
        return false;
    }

    // TODO: 深度ステンシル作成
    if (!CreateDepthStencil())
    {
        _ASSERT_EXPR(false, L"Failed: CreateDepthStencil");
        return false;
    }


    //TODO: ビューポート設定
    viewPort.TopLeftX = 0;
    viewPort.TopLeftY = 0;
    viewPort.Width = width;
    viewPort.Height = height;
    viewPort.MaxDepth = 1.0f;
    viewPort.MinDepth = 0.0f;
    
    //TODO: OMSetRenderTargets でRTVとDSVをセット
    immediateContext->RSSetViewports(1, &viewPort);
    immediateContext->OMSetRenderTargets(1, renderTargetView.GetAddressOf(), depthStencilView.Get());

    
    // --- 追加：シーンバッファの作成 ---
    if (!CreateSceneRenderTarget(width_, height_)) return false;

    // --- 追加：ブルームシステムの初期化 ---
    if (!bloomSystem.Initialize(device.Get(), width_, height_)) return false;

    //modelRenderSystem.Init(device, immediateContext);
    spriteRenderSystem.Initialize(device.Get(), immediateContext.Get());
    modelRenderSystem.Initialize(device.Get());
    shapeRendererSystem.Initialize(device.Get());

    // レンダーステート生成
    renderState = std::make_unique<RenderState>(device.Get());

    //TODO: 成功したら true を返す
    
    return true;
}

/**
 * @brief Direct3Dデバイスと関連リソースを解放する
 * @details アプリケーション終了時に呼び出す。
 */
void GraphicsDeviceManager::Shutdown()
{
    // TODO: GPU待機（context->ClearState / Flush）
    if (immediateContext)
    {
        immediateContext->ClearState();
        immediateContext->Flush();
    }


    // デバイスリソースの解放
    renderTargetView.Reset();
    depthStencilView.Reset();
    sceneRTV.Reset();
    sceneSRV.Reset();
    swapChain.Reset();
    immediateContext.Reset();

    // 最後にデバイスを消す
    device.Reset();
}
/**
 * @brief ウィンドウサイズ変更時に呼び出される
 * @param newWidth_ 新しい幅
 * @param newHeight_ 新しい高さ
 */
void GraphicsDeviceManager::OnResize(int newWidth_, int newHeight_)
{
    // 追加：リサイズ中は描画を止める or コンテキストをクリア
    immediateContext->OMSetRenderTargets(0, nullptr, nullptr);
    immediateContext->ClearState(); // 念のため一度すべて解除

    // メンバ変数の更新を忘れないように
    width = newWidth_;
    height = newHeight_;

    // RTV・DSVを解放
    renderTargetView.Reset();
    depthStencilView.Reset();
    depthStencilTexture.Reset(); // 深度テクスチャ自体もリセットが必要

    // --- 【修正】シーン用バッファとブルームもリセット ---
    sceneRTV.Reset();
    sceneSRV.Reset();
    sceneNormalRTV.Reset();
    sceneNormalSRV.Reset();

    // swapChain->ResizeBuffers() を呼び出し
    HRESULT hr = swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    _ASSERT_EXPR(SUCCEEDED(hr), L"ResizeBuffers Failed");

    // リソースの再作成
    CreateRenderTarget();
    CreateDepthStencil();
    CreateSceneRenderTarget(width, height); // シーンバッファを新しいサイズで作る
    bloomSystem.Initialize(device.Get(), width, height); // ブルームも再初期化

    // ビューポートの設定
    viewPort.Width = static_cast<float>(width);
    viewPort.Height = static_cast<float>(height);
    immediateContext->RSSetViewports(1, &viewPort);
}

/**
 * @brief フレームの描画を開始する
 * @param clearColor_ RGBA形式のクリアカラー配列
 */
void GraphicsDeviceManager::BeginFrame(const float clearColor_[4])
{
    // --- 変更点：バックバッファではなく、シーン用HDRバッファをクリアする ---
    immediateContext->ClearRenderTargetView(sceneRTV.Get(), clearColor_);
    const float black[] = { 0.0f, 0.0f, 0.0f, 0.0f }; // 修正：{} を使い、変数名を black に
    immediateContext->ClearRenderTargetView(sceneNormalRTV.Get(), black);

    // 深度バッファをクリア（これは共通）
    immediateContext->ClearDepthStencilView(depthStencilView.Get(),
        D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    // --- 変更点：描画先を「シーンバッファ」に固定する ---
    // これにより、GameObjectManagerの通常の描画がすべてHDRテクスチャに描き込まれます
    ID3D11RenderTargetView* rtvs[] = { sceneRTV.Get(), sceneNormalRTV.Get() };
    immediateContext->OMSetRenderTargets(2, rtvs, depthStencilView.Get());

    // ビューポートの設定
    immediateContext->RSSetViewports(1, &viewPort);
}
//{
//    // TODO: RenderTargetViewをクリア
//    immediateContext->ClearRenderTargetView(renderTargetView.Get(), clearColor_);
//
//    // TODO: DepthStencilViewをクリア
//    immediateContext->ClearDepthStencilView(depthStencilView.Get(),
//        D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
//
//    SetRenderTargets();
//}

/**
 * @brief フレームの描画を終了して画面に反映する
 */
void GraphicsDeviceManager::EndFrame()
{
    // TODO: swapChain->Present() を呼び出し
    swapChain->Present(1, 0);
}

void GraphicsDeviceManager::SetParticleRenderTarget()
{
    ID3D11RenderTargetView* rtv = sceneRTV.Get();
    // ターゲットを1枚（sceneRTVのみ）としてセット
    immediateContext->OMSetRenderTargets(1, &rtv, depthStencilView.Get());
}

void GraphicsDeviceManager::SetSceneRenderTarget()
{
    // 描画先を「シーンカラー」と「シーン法線」の2枚に切り替える
    ID3D11RenderTargetView* rtvs[] = {
        sceneRTV.Get(),
        sceneNormalRTV.Get()
    };

    // 第一引数を 2 に変更
    immediateContext->OMSetRenderTargets(2, rtvs, depthStencilView.Get());
}

void GraphicsDeviceManager::ResolveBloom(const RenderContext& rc) {
    auto dsv = depthStencilView.Get();
    immediateContext->OMSetRenderTargets(1, renderTargetView.GetAddressOf(), dsv);

    bloomSystem.Execute(rc, sceneSRV.Get(), renderTargetView.Get(), bloomParams, filterParams);
}

void GraphicsDeviceManager::ResetFilter()
{
    filterParams.hueShift = 0.0f;
    filterParams.saturation = 1.0f;
    filterParams.brightness = 1.0f;
    filterParams.contrast = 1.0f;
    filterParams.colorBalance = { 1.0f, 1.0f, 1.0f };
    filterParams.filterIntensity = 1.0f;
}

void GraphicsDeviceManager::ShowPostEffectGui()
{
    // ポストプロセス設定ウィンドウの開始
    ImGui::Begin("GraphicsSettings");

    if (ImGui::CollapsingHeader("Bloom (Kawase Blur)", ImGuiTreeNodeFlags_DefaultOpen))
    {
        // 1. 閾値 (Threshold): どの明るさから光らせるか
        ImGui::DragFloat("Threshold", &bloomParams.threshold, 0.01f, 0.0f, 5.0f, "%.2f");
        //ImGui::SetItemTooltip("この値より明るいピクセルが抽出されます。低いほど画面全体が光ります。");

        // 2. 強度 (Intensity): 最終的な光の強さ
        ImGui::DragFloat("Intensity", &bloomParams.intensity, 0.01f, 0.0f, 10.0f, "%.2f");
        //ImGui::SetItemTooltip("合成時の光の強さです。");

        // 3. スケール (Kawase Scale): ぼかしの広がり具合
        ImGui::DragFloat("Kawase Scale", &bloomParams.kawaseScale, 0.01f, 0.0f, 2.0f, "%.2f");
        //ImGui::SetItemTooltip("光を拡散させる距離です。上げると「ぶれる」感じが強くなります。");

        // 4. パス回数 (Blur Passes): ぼかしの質
        ImGui::SliderInt("Blur Passes", &bloomParams.blurPasses, 1, 8);
        //ImGui::SetItemTooltip("ぼかしを繰り返す回数です。多いほど滑らかに遠くまで光が届きます。");

        ImGui::Separator();

        // 5. リセットボタン
        if (ImGui::Button("Reset Parameters")) {
            bloomParams.threshold = 1.0f;
            bloomParams.intensity = 1.0f;
            bloomParams.kawaseScale = 1.0f;
            bloomParams.blurPasses = 4;
        }
    }

    // --- 【追加】カラーフィルター設定 ---
    if (ImGui::CollapsingHeader("Color Filter", ImGuiTreeNodeFlags_DefaultOpen))
    {
        // 基本調整
        ImGui::DragFloat("Hue Shift", &filterParams.hueShift, 0.005f, -1.0f, 1.0f, "%.2f");
        ImGui::DragFloat("Saturation", &filterParams.saturation, 0.01f, 0.0f, 2.0f, "%.2f");
        ImGui::DragFloat("Brightness", &filterParams.brightness, 0.01f, 0.0f, 2.0f, "%.2f");
        ImGui::DragFloat("Contrast", &filterParams.contrast, 0.01f, 0.0f, 2.0f, "%.2f");

        ImGui::Separator();

        // カラーバランス (RGBの乗算)
        ImGui::ColorEdit3("Color Balance", &filterParams.colorBalance.x);

        // 適用強度
        ImGui::SliderFloat("Filter Intensity", &filterParams.filterIntensity, 0.0f, 1.0f);

        if (ImGui::Button("Reset Filter")) 
        {
            ResetFilter();
        }
    }

    ImGui::End();
}

/**
 * @brief Direct3D11デバイスとスワップチェーンを生成
 * @param hwnd_ ウィンドウハンドル
 * @return 成功時 true、失敗時 false
 */
bool GraphicsDeviceManager::CreateDeviceAndSwapChain(HWND hwnd_)
{
    HRESULT hr = S_OK;
    UINT createDeviceFlags_ = 0;
#if defined(DEBUG) || defined(_DEBUG)
    createDeviceFlags_ |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    // --- 修正箇所 ---
    swapChainDesc.BufferCount = 2; // 1から2に変更 (Flipモデルの必須条件)
    swapChainDesc.BufferDesc.Width = width;
    swapChainDesc.BufferDesc.Height = height;
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
    swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.OutputWindow = hwnd_;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.Windowed = !fullscreen;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    // TODO: D3D11CreateDeviceAndSwapChain() を呼び出し
    //device / context / swapChain を取得
    //featureLevel_ を保存
    hr = D3D11CreateDeviceAndSwapChain(
        NULL, 
        D3D_DRIVER_TYPE_HARDWARE, 
        NULL,
        createDeviceFlags_,
        &featureLevel, 
        1, 
        D3D11_SDK_VERSION,
        &swapChainDesc,
        swapChain.GetAddressOf(), 
        device.GetAddressOf(),
        NULL,
        immediateContext.GetAddressOf());

    
    // TODO: エラーチェック
    _ASSERT_EXPR(SUCCEEDED(hr), L"D3D11CreateDeviceAndSwapChain");
        if (FAILED(hr)) return false;

        return true;  // 成功時
}

/**
 * @brief バックバッファからRenderTargetViewを作成
 * @return 成功時 true、失敗時 false
 */
bool GraphicsDeviceManager::CreateRenderTarget()
{
    HRESULT hr_ = S_OK;

    // TODO: swapChain_->GetBuffer() でバックバッファ取得
    Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer_;

    hr_ = swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(backBuffer_.GetAddressOf()));
    _ASSERT_EXPR(SUCCEEDED(hr_), (L"miss GetBuffer"));
    // TODO: device_->CreateRenderTargetView() でRTV作成
    hr_ = device->CreateRenderTargetView(backBuffer_.Get(), NULL, renderTargetView.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr_), L"miss CreateRenderTargetView");


    // TODO: 成功時 true を返す
    return SUCCEEDED(hr_);
}

/**
 * @brief 深度ステンシルバッファとViewを作成
 * @return 成功時 true、失敗時 false
 */
bool GraphicsDeviceManager::CreateDepthStencil()
{
    HRESULT hr_ = S_OK;

    // --- 1. Texture2D の作成 ---
    // デカールで深度を読み取るため、DSVとSRV両方で使える TYPELESS フォーマットにする
    D3D11_TEXTURE2D_DESC texture2dDesc_{};
    texture2dDesc_.Width = width;
    texture2dDesc_.Height = height;
    texture2dDesc_.MipLevels = 1;
    texture2dDesc_.ArraySize = 1;
    texture2dDesc_.Format = DXGI_FORMAT_R24G8_TYPELESS; // 変更：汎用的な型
    texture2dDesc_.SampleDesc.Count = 1;
    texture2dDesc_.SampleDesc.Quality = 0;
    texture2dDesc_.Usage = D3D11_USAGE_DEFAULT;
    // 重要：深度書き込み用(DS)と、シェーダー読み込み用(SR)の両方を指定
    texture2dDesc_.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    texture2dDesc_.CPUAccessFlags = 0;
    texture2dDesc_.MiscFlags = 0;

    // メンバ変数の depthStencilTexture に保存（SRV作成に必要）
    hr_ = device->CreateTexture2D(&texture2dDesc_, NULL, depthStencilTexture.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr_), L"Failed to CreateTexture2D for DepthStencil");

    // --- 2. DepthStencilView (書き込み用) の作成 ---
    D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc_{};
    depthStencilViewDesc_.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // 深度＋ステンシルとして解釈
    depthStencilViewDesc_.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    depthStencilViewDesc_.Flags = 0;
    depthStencilViewDesc_.Texture2D.MipSlice = 0;

    hr_ = device->CreateDepthStencilView(depthStencilTexture.Get(), &depthStencilViewDesc_,
        depthStencilView.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr_), L"Failed to CreateDepthStencilView");

    // --- 3. ShaderResourceView (デカール読み取り用) の作成 ---
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS; // 24bitの深度成分のみを読み取る
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;

    hr_ = device->CreateShaderResourceView(depthStencilTexture.Get(), &srvDesc,
        depthStencilSRV.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr_), L"Failed to CreateShaderResourceView for Depth");

    // --- レンダリングターゲットへのバインド ---
    immediateContext->OMSetRenderTargets(1, renderTargetView.GetAddressOf(), depthStencilView.Get());

    {
        // メンバ変数の depthStencilTexture に保存（SRV作成に必要）
        hr_ = device->CreateTexture2D(&texture2dDesc_, NULL, copyDepthStencilTexture.ReleaseAndGetAddressOf());
        hr_ = device->CreateShaderResourceView(copyDepthStencilTexture.Get(), &srvDesc,
            copyDepthStenciSRV.ReleaseAndGetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr_), L"Failed to CreateShaderResourceView for Depth");
    }
    return SUCCEEDED(hr_);
}
/**
 * @brief 全てのDirect3Dリソースを解放する
 */
void GraphicsDeviceManager::ReleaseResources()
{
    // 1. レンダリングターゲットの解除（読み取りと書き込みが衝突しないようにする）
    ID3D11RenderTargetView* nullRTV = nullptr;
    immediateContext->OMSetRenderTargets(1, &nullRTV, nullptr);

    // 2. RenderContext の作成（GameObjectManager でやっていたのと同様）
    RenderContext rc;
    rc.deviceContext = immediateContext.Get();
    rc.renderState = renderState.get(); // GraphicsDeviceManagerが持っているRenderState
    // ※CameraやLightはポストプロセスには直接不要な場合が多いので nullptr のままでもOK

    // 3. ブルーム実行
    // 引数1に rc を渡す（ポインタではなく実体なので変換エラーが消えます）
    bloomSystem.Execute(
        rc,
        sceneSRV.Get(),
        renderTargetView.Get(),
        bloomParams
    );
}
void GraphicsDeviceManager::SetRenderTargets()
{
    immediateContext->RSSetViewports(1, &viewPort);
    immediateContext->OMSetRenderTargets(1, renderTargetView.GetAddressOf(), depthStencilView.Get());
}

bool GraphicsDeviceManager::CreateSceneRenderTarget(int width_, int height_)
{
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width_;
    desc.Height = height_;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT; // HDR用
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    HRESULT hr = device->CreateTexture2D(&desc, nullptr, texture.GetAddressOf());
    if (FAILED(hr)) return false;

    device->CreateRenderTargetView(texture.Get(), nullptr, sceneRTV.ReleaseAndGetAddressOf());
    device->CreateShaderResourceView(texture.Get(), nullptr, sceneSRV.ReleaseAndGetAddressOf());

    // --- ② 【追加】シーン法線バッファ ---
    D3D11_TEXTURE2D_DESC nDesc = desc; // カラーの設定を流用
    nDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT; // 法線用

    Microsoft::WRL::ComPtr<ID3D11Texture2D> nTexture;
    device->CreateTexture2D(&nDesc, nullptr, nTexture.GetAddressOf());
    device->CreateRenderTargetView(nTexture.Get(), nullptr, sceneNormalRTV.ReleaseAndGetAddressOf());
    device->CreateShaderResourceView(nTexture.Get(), nullptr, sceneNormalSRV.ReleaseAndGetAddressOf());


    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    hr = device->CreateTexture2D(&desc, nullptr, copySceneColorTexture.ReleaseAndGetAddressOf());
    if (FAILED(hr)) return false;

    hr = device->CreateShaderResourceView(copySceneColorTexture.Get(), nullptr, copySceneColorSRV.ReleaseAndGetAddressOf());
    if (FAILED(hr)) return false;

    // --- 法線のコピー用 ---
    hr = device->CreateTexture2D(&desc, nullptr, copyNormalTexture.ReleaseAndGetAddressOf());
    if (FAILED(hr)) return false;

    hr = device->CreateShaderResourceView(copyNormalTexture.Get(), nullptr, copyNormalSRV.ReleaseAndGetAddressOf());
    if (FAILED(hr)) return false;

    return true;
}
void GraphicsDeviceManager::Finalize()
{
#if 1
    BOOL fullscreen = 0;
    swapChain->GetFullscreenState(&fullscreen, 0);
    if (fullscreen)
    {
        swapChain->SetFullscreenState(FALSE, 0);
    }
#endif
}
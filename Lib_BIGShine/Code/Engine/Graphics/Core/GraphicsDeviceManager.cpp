
#include "GraphicsDeviceManager.h"
#include "IMGUI.h"

#include <d3d11.h>
#include <d3d11sdklayers.h>

#include "Engine/Graphics/Core/CPUResourceLoader.h"

struct SkyBoxVertex
{
    float position[3];
    float color[4];
    float texcoord[2];
};

bool GraphicsDeviceManager::LoadIBLTextures()
{
    // ---- プロジェクトに合わせて書き換える ----
    const wchar_t* diffusePath_ = L"Data/Texture/IBL/diffuse_iem.dds";
    const wchar_t* specularPath_ = L"Data/Texture/IBL/skybox_highres.dds";
    const wchar_t* lutPath_ = L"Data/Texture/IBL/lut_ggx.dds";
    // ---------------------------------------------------------

    D3D11_TEXTURE2D_DESC textureDescription_{};

    // 拡散反射 IEM（IrradianceMap）
    if (FAILED(CPUResourceLoader::LoadTextureFromFile(
        device.Get(), diffusePath_,
        iblDiffuseIemSRV.GetAddressOf(), &textureDescription_)))
    {
        OutputDebugStringW(L"[IBL] Failed to load diffuse_iem.dds\n");
        return false;
    }

    // 鏡面反射 PMREM（RadianceMap）
    if (FAILED(CPUResourceLoader::LoadTextureFromFile(
        device.Get(), specularPath_,
        iblSpecularPmremSRV.GetAddressOf(), &textureDescription_)))
    {
        OutputDebugStringW(L"[IBL] Failed to load specular_pmrem.dds\n");
        return false;
    }

    // GGX BRDF LUT
    if (FAILED(CPUResourceLoader::LoadTextureFromFile(
        device.Get(), lutPath_,
        iblLutGgxSRV.GetAddressOf(), &textureDescription_)))
    {
        OutputDebugStringW(L"[IBL] Failed to load lut_ggx.dds\n");
        return false;
    }

    return true;
}

// ----------------------------------------------------------------
// スカイボックス用シェーダーとスプライトの初期化
// ----------------------------------------------------------------
bool GraphicsDeviceManager::InitializeSkyBox()
{
    // スプライトと同じ入力レイアウト（Position / Color / TexCoord）
    D3D11_INPUT_ELEMENT_DESC inputElementDescription_[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    // ---- CSO パスをプロジェクトに合わせて変更してください ----
    CPUResourceLoader::CreateVSFromCSO(device.Get(), "Shader/SkyBoxVS.cso",
        skyBoxVertexShader.GetAddressOf(),
        skyBoxInputLayout.GetAddressOf(),
        inputElementDescription_, _countof(inputElementDescription_));

    CPUResourceLoader::CreatePSFromCSO(device.Get(), "Shader/SkyBoxPS.cso",
        skyBoxPixelShader.GetAddressOf());
    // ----------------------------------------------------------


    SkyBoxVertex vertices_[4] =
    {
        // position (NDC)         color (白・不透明)              texcoord
        { { -1.0f,  1.0f, 1.0f }, { 1,1,1,1 }, { 0.0f, 0.0f } }, // 左上
        { {  1.0f,  1.0f, 1.0f }, { 1,1,1,1 }, { 1.0f, 0.0f } }, // 右上
        { { -1.0f, -1.0f, 1.0f }, { 1,1,1,1 }, { 0.0f, 1.0f } }, // 左下
        { {  1.0f, -1.0f, 1.0f }, { 1,1,1,1 }, { 1.0f, 1.0f } }, // 右下
    };

    D3D11_BUFFER_DESC vertexBufferDescription_ = {};
    vertexBufferDescription_.ByteWidth = sizeof(vertices_);
    vertexBufferDescription_.Usage = D3D11_USAGE_IMMUTABLE;
    vertexBufferDescription_.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vertexBufferData_ = {};
    vertexBufferData_.pSysMem = vertices_;

    if (FAILED(device->CreateBuffer(&vertexBufferDescription_, &vertexBufferData_,
        skyBoxVertexBuffer.GetAddressOf())))
    {
        OutputDebugStringW(L"[SkyBox] Failed to create vertex buffer\n");
        return false;
    }

    // インデックスバッファ（三角形 2 枚 = 6 インデックス）
    UINT indices_[6] = { 0, 1, 2, 1, 3, 2 };

    D3D11_BUFFER_DESC indexBufferDescription_ = {};
    indexBufferDescription_.ByteWidth = sizeof(indices_);
    indexBufferDescription_.Usage = D3D11_USAGE_IMMUTABLE;
    indexBufferDescription_.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_DEPTH_STENCIL_DESC depthStencilDescription_{};
    depthStencilDescription_.DepthEnable = TRUE;
    depthStencilDescription_.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    depthStencilDescription_.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    device->CreateDepthStencilState(&depthStencilDescription_, skyBoxDepthStencilState.GetAddressOf());

    D3D11_SUBRESOURCE_DATA indexBufferData_ = {};
    indexBufferData_.pSysMem = indices_;

    if (FAILED(device->CreateBuffer(&indexBufferDescription_, &indexBufferData_,
        skyBoxIndexBuffer.GetAddressOf())))
    {
        OutputDebugStringW(L"[SkyBox] Failed to create index buffer\n");
        return false;
    }


    return true;
}

//---
//@brief Direct3D 11 デバイスとスワップチェーンを初期化する
//@param hwnd_ ウィンドウハンドル
//@param width_ 描画領域の幅
//@param height_ 描画領域の高さ
//@param fullscreen_ フルスクリーンモードかどうか
//@return 初期化成功で true、失敗で false
//---
bool GraphicsDeviceManager::Initialize(HWND windowHandle_, int width_, int height_, bool fullscreen_)
{
    HRESULT hr_ = S_OK;
    UINT createDeviceFlags_ = 0;
#if defined(DEBUG) || defined(_DEBUG)
    createDeviceFlags_ |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    // 引数をメンバ変数に保存
    width = width_;
    height = height_;
    fullscreen = fullscreen_;
    windowHandle = windowHandle_;

    // デバイスとスワップチェーンを作成
    if (!CreateDeviceAndSwapChain(windowHandle_))
    {
        _ASSERT_EXPR(false, L"Failed: CreateDeviceAndSwapChain");
        return false;
    }

    // バックバッファRTVを作成
    if (!CreateRenderTarget())
    {
        _ASSERT_EXPR(false, L"Failed: CreateRenderTarget");
        return false;
    }

    // 深度ステンシル作成
    if (!CreateDepthStencil())
    {
        _ASSERT_EXPR(false, L"Failed: CreateDepthStencil");
        return false;
    }


    // ビューポート設定
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = width;
    viewport.Height = height;
    viewport.MaxDepth = 1.0f;
    viewport.MinDepth = 0.0f;

    //OMSetRenderTargets でRTVとDSVをセット
    immediateContext->RSSetViewports(1, &viewport);
    immediateContext->OMSetRenderTargets(1, renderTargetView.GetAddressOf(), depthStencilView.Get());


    if (!LoadIBLTextures())  return false;
    if (!InitializeSkyBox()) return false;

    // --- 追加：シーンバッファの作成 ---
    if (!CreateSceneRenderTarget(width_, height_)) return false;
    if (!CreateGBufferTargets(width_, height_)) return false;


    if (!postRenderSystem.Initialize(device.Get(), width_, height_)) return false;
    if (!cascadeShadowMap.Initialize(device.Get())) return false;

    //modelRenderSystem.Init(device, immediateContext);
    shapeRendererSystem.Initialize(device.Get());

    // レンダーステート生成
    renderState = std::make_unique<RenderState>(device.Get());



    //成功したら true を返す

    return true;
}

/**
 * @brief Direct3Dデバイスと関連リソースを解放する
 * @details アプリケーション終了時に呼び出す。
 */
void GraphicsDeviceManager::Shutdown()
{
    // GPU待機（context->ClearState / Flush）
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
    sceneDistortionRTV.Reset();
    sceneDistortionSRV.Reset();
    swapChain.Reset();
    immediateContext.Reset();

    skyBoxVertexBuffer.Reset();
    skyBoxIndexBuffer.Reset();

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
    sceneDistortionRTV.Reset();
    sceneDistortionSRV.Reset();
    sceneNormalRTV.Reset();
    sceneNormalSRV.Reset();
    gBufferBaseColorRTV.Reset();
    gBufferBaseColorSRV.Reset();
    gBufferNormalRTV.Reset();
    gBufferNormalSRV.Reset();
    gBufferMaterialRTV.Reset();
    gBufferMaterialSRV.Reset();
    gBufferEmissiveRTV.Reset();
    gBufferEmissiveSRV.Reset();

    // swapChain->ResizeBuffers() を呼び出し
    HRESULT result_ = swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    _ASSERT_EXPR(SUCCEEDED(result_), L"ResizeBuffers Failed");

    // リソースの再作成
    CreateRenderTarget();
    CreateDepthStencil();
    CreateSceneRenderTarget(width, height); // シーンバッファを新しいサイズで作る
    CreateGBufferTargets(width, height);
    postRenderSystem.Initialize(device.Get(), width, height); // ポストプロセスも再初期化

    // ビューポートの設定
    viewport.Width = static_cast<float>(width);
    viewport.Height = static_cast<float>(height);
    immediateContext->RSSetViewports(1, &viewport);
}

/**
 * @brief フレームの描画を開始する
 * @param clearColor_ RGBA形式のクリアカラー配列
 */
void GraphicsDeviceManager::BeginFrame(const float clearColor_[4])
{
    // --- 変更点：バックバッファではなく、シーン用HDRバッファをクリアする ---
    sceneDistortionUsed = false;
    immediateContext->ClearRenderTargetView(sceneRTV.Get(), clearColor_);
    const float black[] = { 0.0f, 0.0f, 0.0f, 0.0f }; // 修正：{} を使い、変数名を black に
    immediateContext->ClearRenderTargetView(sceneNormalRTV.Get(), black);
    if (sceneDistortionRTV)
    {
        immediateContext->ClearRenderTargetView(sceneDistortionRTV.Get(), black);
    }
    immediateContext->ClearRenderTargetView(gBufferBaseColorRTV.Get(), black);
    immediateContext->ClearRenderTargetView(gBufferNormalRTV.Get(), black);
    immediateContext->ClearRenderTargetView(gBufferMaterialRTV.Get(), black);
    immediateContext->ClearRenderTargetView(gBufferEmissiveRTV.Get(), black);

    // 深度バッファをクリア（これは共通）
    immediateContext->ClearDepthStencilView(depthStencilView.Get(),
        D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    // --- 変更点：描画先を「シーンバッファ」に固定する ---
    // これにより、GameObjectManagerの通常の描画がすべてHDRテクスチャに描き込まれます
    ID3D11RenderTargetView* renderTargets_[] = { sceneRTV.Get(), sceneNormalRTV.Get() };
    immediateContext->OMSetRenderTargets(2, renderTargets_, depthStencilView.Get());

    // ビューポートの設定
    immediateContext->RSSetViewports(1, &viewport);
}

/**
 * @brief フレームの描画を終了して画面に反映する
 */
void GraphicsDeviceManager::EndFrame()
{
    // swapChain->Present() を呼び出し
    swapChain->Present(vsyncEnabled ? 1 : 0, 0);
}

void GraphicsDeviceManager::SetParticleRenderTarget()
{
    ID3D11RenderTargetView* renderTarget_ = sceneRTV.Get();
    // ターゲットを1枚（sceneRTVのみ）としてセット
    immediateContext->OMSetRenderTargets(1, &renderTarget_, depthStencilView.Get());
}

void GraphicsDeviceManager::SetParticleRenderTargetWithDistortion()
{
    sceneDistortionUsed = true;
    ID3D11RenderTargetView* renderTargets_[] = { sceneRTV.Get(), sceneDistortionRTV.Get() };
    immediateContext->OMSetRenderTargets(2, renderTargets_, depthStencilView.Get());
}
void GraphicsDeviceManager::SetBackBufferRenderTarget()
{
    immediateContext->RSSetViewports(1, &viewport);
    immediateContext->OMSetRenderTargets(1, renderTargetView.GetAddressOf(), depthStencilView.Get());
}

void GraphicsDeviceManager::SetSceneRenderTarget()
{
    // 描画先を「シーンカラー」と「シーン法線」の2枚に切り替える
    ID3D11RenderTargetView* renderTargets_[] = {
        sceneRTV.Get(),
        sceneNormalRTV.Get()
    };

    // 第一引数を 2 に変更
    immediateContext->OMSetRenderTargets(2, renderTargets_, depthStencilView.Get());

    immediateContext->RSSetViewports(1, &viewport);
}

void GraphicsDeviceManager::SetSceneColorRenderTarget()
{
    ID3D11RenderTargetView* rtv_ = sceneRTV.Get();
    immediateContext->OMSetRenderTargets(1, &rtv_, nullptr);
    immediateContext->RSSetViewports(1, &viewport);
}

void GraphicsDeviceManager::SetSceneDistortionRenderTarget()
{
    sceneDistortionUsed = true;
    ID3D11RenderTargetView* rtv_ = sceneDistortionRTV.Get();
    immediateContext->OMSetRenderTargets(1, &rtv_, depthStencilView.Get());
    immediateContext->RSSetViewports(1, &viewport);
}

void GraphicsDeviceManager::SetGBufferRenderTargets()
{
    ID3D11RenderTargetView* rtvs_[] =
    {
        gBufferBaseColorRTV.Get(),
        gBufferNormalRTV.Get(),
        gBufferMaterialRTV.Get(),
        gBufferEmissiveRTV.Get(),
    };
    immediateContext->OMSetRenderTargets(_countof(rtvs_), rtvs_, depthStencilView.Get());
    immediateContext->RSSetViewports(1, &viewport);
}

void GraphicsDeviceManager::ResolveBloom(const RenderContext& rc) {
    auto depthStencil_ = depthStencilView.Get();
    immediateContext->OMSetRenderTargets(1, renderTargetView.GetAddressOf(), depthStencil_);

    postRenderSystem.Execute(rc, sceneSRV.Get(), renderTargetView.Get());
}


void GraphicsDeviceManager::RenderSkyBox(const RenderContext& rc)
{
    ID3D11DeviceContext* deviceContext_ = immediateContext.Get();

    // IBL specular_pmrem を t10 にバインド
    ID3D11ShaderResourceView* shaderResources_[] = { iblSpecularPmremSRV.Get() };
    deviceContext_->PSSetShaderResources(10, 1, shaderResources_);

    // 深度テスト LESS_EQUAL・書き込みOFF
    deviceContext_->OMSetDepthStencilState(skyBoxDepthStencilState.Get(), 0);

    // カリング OFF
    deviceContext_->RSSetState(renderState->GetRasterizerState(RasterizerState::SolidCullNone));

    // アルファブレンド OFF
    deviceContext_->OMSetBlendState(renderState->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);

    // シェーダー・入力レイアウト
    deviceContext_->VSSetShader(skyBoxVertexShader.Get(), nullptr, 0);
    deviceContext_->PSSetShader(skyBoxPixelShader.Get(), nullptr, 0);
    deviceContext_->IASetInputLayout(skyBoxInputLayout.Get());
    deviceContext_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 頂点・インデックスバッファ
    UINT vertexStride_ = sizeof(SkyBoxVertex);
    UINT vertexOffset_ = 0;
    deviceContext_->IASetVertexBuffers(0, 1, skyBoxVertexBuffer.GetAddressOf(), &vertexStride_, &vertexOffset_);
    deviceContext_->IASetIndexBuffer(skyBoxIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

    deviceContext_->DrawIndexed(6, 0, 0);

    // ---- 後始末 ----
    deviceContext_->VSSetShader(nullptr, nullptr, 0);
    deviceContext_->PSSetShader(nullptr, nullptr, 0);
    deviceContext_->IASetInputLayout(nullptr);

    ID3D11ShaderResourceView* nullShaderResource_ = nullptr;
    deviceContext_->PSSetShaderResources(10, 1, &nullShaderResource_);

    deviceContext_->OMSetDepthStencilState(renderState->GetDepthStencilState(DepthState::TestAndWrite), 0);
    deviceContext_->RSSetState(renderState->GetRasterizerState(RasterizerState::SolidCullBack));
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

void GraphicsDeviceManager::DrawPostEffectGuiContent()
{
    ImGui::Checkbox("VSync", &vsyncEnabled);
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Bloom (Kawase Blur)", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Bloom Enabled", &bloomParams.enabled);
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
        ImGui::SliderInt("Blur Passes", &bloomParams.blurPasses, 0, 8);
        //ImGui::SetItemTooltip("ぼかしを繰り返す回数です。多いほど滑らかに遠くまで光が届きます。");

        ImGui::Separator();

        // 5. トーンマッピング
        ImGui::Checkbox("Tone Mapping", &bloomParams.toneMappingEnabled);
        if (bloomParams.toneMappingEnabled)
        {
            ImGui::DragFloat("Exposure", &bloomParams.exposure, 0.01f, 0.0f, 10.0f, "%.2f");
        }

        ImGui::Separator();

        // 6. リセットボタン
        if (ImGui::Button("Reset Parameters")) {
            bloomParams.enabled = true;
            bloomParams.threshold = 1.0f;
            bloomParams.intensity = 1.0f;
            bloomParams.kawaseScale = 1.0f;
            bloomParams.blurPasses = 4;
            bloomParams.toneMappingEnabled = true;
            bloomParams.exposure = 1.0f;
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
    if (ImGui::CollapsingHeader("Motion Blur", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Motion Blur Enabled", &motionBlurParams.enabled);
        ImGui::SliderInt("Motion Blur Samples", &motionBlurParams.sampleCount, 2, 16);
        ImGui::DragFloat("Motion Blur Strength", &motionBlurParams.strength, 0.01f, 0.0f, 2.0f, "%.2f");
        ImGui::DragFloat("Motion Blur Max Velocity", &motionBlurParams.maxVelocity, 0.001f, 0.001f, 0.2f, "%.3f");
    }

    if (ImGui::CollapsingHeader("Vignette", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Vignette Enabled", &vignetteParams.enabled);
        ImGui::ColorEdit4("Vignette Color", &vignetteParams.color.x);
        ImGui::DragFloat2("Vignette Center", &vignetteParams.center.x, 0.01f, 0.0f, 1.0f, "%.2f");
        ImGui::DragFloat("Vignette Intensity", &vignetteParams.intensity, 0.01f, 0.0f, 4.0f, "%.2f");
        ImGui::DragFloat("Vignette Smoothness", &vignetteParams.smoothness, 0.01f, 0.01f, 4.0f, "%.2f");
        ImGui::SliderFloat("Vignette Rounded", &vignetteParams.rounded, 0.0f, 1.0f, "%.2f");
        ImGui::DragFloat("Vignette Roundness", &vignetteParams.roundness, 0.01f, 0.1f, 8.0f, "%.2f");
        ImGui::SliderFloat("Vignette Opacity", &vignetteParams.opacity, 0.0f, 1.0f, "%.2f");
    }
    if (ImGui::CollapsingHeader("XeGTAO", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("AO Enabled", &xeGtaoParams.enabled);
        ImGui::Checkbox("AO Only", &xeGtaoParams.showOnly);
        ImGui::DragFloat("AO Radius", &xeGtaoParams.radius, 0.01f, 0.01f, 20.0f, "%.2f");
        ImGui::DragFloat("AO Intensity", &xeGtaoParams.intensity, 0.01f, 0.0f, 3.0f, "%.2f");
        ImGui::DragFloat("AO Power", &xeGtaoParams.power, 0.01f, 0.1f, 5.0f, "%.2f");
        ImGui::DragFloat("AO Sample Radius", &xeGtaoParams.sampleRadius, 0.1f, 1.0f, 64.0f, "%.1f");
        ImGui::SliderInt("AO Samples", &xeGtaoParams.sampleCount, 1, 8);
        ImGui::SliderInt("AO Update Interval", &xeGtaoParams.updateInterval, 1, 8);

        if (ImGui::Button("Reset XeGTAO"))
        {
            xeGtaoParams = XeGtaoSystem::Parameters{};
        }
    }
    // --- 【追加】ラジアルブラー設定 ---
    if (ImGui::CollapsingHeader("Radial Blur", ImGuiTreeNodeFlags_DefaultOpen))
    {
        // 1. ブラー半径 (Blur Radius)
        ImGui::DragFloat("Blur Radius", &radialBlurParams.blurRadius, 0.001f, 0.0f, 0.5f, "%.3f");

        // 2. サンプリング数 (Sampling Count)
        ImGui::SliderInt("Sampling Count", &radialBlurParams.blurSamplingCount, 4, 32);

        // 3. ブラーの中心点 (Center)
        ImGui::DragFloat2("Blur Center", &radialBlurParams.blurCenter.x, 0.01f, 0.0f, 1.0f, "%.2f");

        // 4. マスク半径 (Mask Radius)
        ImGui::DragFloat("Mask Radius", &radialBlurParams.blurMaskRadius, 0.01f, 0.0f, 2.0f, "%.2f");

        ImGui::Separator();

        // 5. リセットボタン
        if (ImGui::Button("Reset Radial Blur")) {
            radialBlurParams.blurRadius = 0.05f;
            radialBlurParams.blurSamplingCount = 16;
            radialBlurParams.blurCenter = { 0.5f, 0.5f };
            radialBlurParams.blurMaskRadius = 0.5f;
        }
    }
}

void GraphicsDeviceManager::ShowPostEffectGui()
{
    ImGui::Begin("GraphicsSettings");
    DrawPostEffectGuiContent();
    ImGui::End();
}

/**
 * @brief Direct3D11デバイスとスワップチェーンを生成
 * @param hwnd_ ウィンドウハンドル
 * @return 成功時 true、失敗時 false
 */

bool GraphicsDeviceManager::CreateDeviceAndSwapChain(HWND hwnd_)
{
    HRESULT result_ = S_OK;

    UINT createDeviceFlags_ = 0;
#if defined(DEBUG) || defined(_DEBUG)
    createDeviceFlags_ |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    // =========================
    // 一番VRAMが大きいGPUを探す
    // =========================
    Microsoft::WRL::ComPtr<IDXGIFactory> dxgiFactory_;
    result_ = CreateDXGIFactory(__uuidof(IDXGIFactory), reinterpret_cast<void**>(dxgiFactory_.GetAddressOf()));
    if (FAILED(result_)) return false;

    Microsoft::WRL::ComPtr<IDXGIAdapter> bestAdapter_;
    SIZE_T bestVideoMemory_ = 0;

    for (UINT adapterIndex_ = 0;; ++adapterIndex_)
    {
        Microsoft::WRL::ComPtr<IDXGIAdapter> adapter_;
        if (dxgiFactory_->EnumAdapters(adapterIndex_, adapter_.GetAddressOf()) == DXGI_ERROR_NOT_FOUND)
        {
            break;
        }

        DXGI_ADAPTER_DESC adapterDesc_{};
        adapter_->GetDesc(&adapterDesc_);

        // Microsoft Basic Render Driver は除外
        if (adapterDesc_.VendorId == 0x1414)
        {
            continue;
        }

        if (adapterDesc_.DedicatedVideoMemory > bestVideoMemory_)
        {
            bestVideoMemory_ = adapterDesc_.DedicatedVideoMemory;
            bestAdapter_ = adapter_;
        }
    }

    if (!bestAdapter_)
    {
        return false;
    }

    // =========================
    // スワップチェーン設定
    // =========================
    swapChainDesc = {};
    swapChainDesc.BufferCount = 2;
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

    D3D_FEATURE_LEVEL featureLevels_[] =
    {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };

    D3D_FEATURE_LEVEL createdFeatureLevel_{};

    // =========================
    // 選んだGPUでDevice作成
    // 注意: adapter指定時は DRIVER_TYPE_UNKNOWN
    // =========================
    result_ = D3D11CreateDeviceAndSwapChain(
        bestAdapter_.Get(),
        D3D_DRIVER_TYPE_UNKNOWN,
        nullptr,
        createDeviceFlags_,
        featureLevels_,
        _countof(featureLevels_),
        D3D11_SDK_VERSION,
        &swapChainDesc,
        swapChain.GetAddressOf(),
        device.GetAddressOf(),
        &createdFeatureLevel_,
        immediateContext.GetAddressOf()
    );

    _ASSERT_EXPR(SUCCEEDED(result_), L"D3D11CreateDeviceAndSwapChain");
    if (FAILED(result_)) return false;

    Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice_;
    Microsoft::WRL::ComPtr<IDXGIAdapter> activeAdapter_;
    DXGI_ADAPTER_DESC activeAdapterDesc_{};
    if (SUCCEEDED(device.As(&dxgiDevice_)) &&
        SUCCEEDED(dxgiDevice_->GetAdapter(activeAdapter_.GetAddressOf())) &&
        SUCCEEDED(activeAdapter_->GetDesc(&activeAdapterDesc_)))
    {
        std::wostringstream message_;
        message_ << L"[Graphics] Active GPU: " << activeAdapterDesc_.Description
            << L" (Dedicated VRAM: "
            << activeAdapterDesc_.DedicatedVideoMemory / (1024ull * 1024ull)
            << L" MB)\n";
        OutputDebugStringW(message_.str().c_str());

        wchar_t currentTitle_[256]{};
        GetWindowTextW(hwnd_, currentTitle_, static_cast<int>(_countof(currentTitle_)));
        std::wostringstream title_;
        title_ << currentTitle_ << L" [GPU: " << activeAdapterDesc_.Description << L"]";
        SetWindowTextW(hwnd_, title_.str().c_str());
    }

    return true;
}

/**
 * @brief バックバッファからRenderTargetViewを作成
 * @return 成功時 true、失敗時 false
 */
bool GraphicsDeviceManager::CreateRenderTarget()
{
    HRESULT hr_ = S_OK;

    // swapChain_->GetBuffer() でバックバッファ取得
    Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer_;

    hr_ = swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(backBuffer_.GetAddressOf()));
    _ASSERT_EXPR(SUCCEEDED(hr_), (L"miss GetBuffer"));
    //device_->CreateRenderTargetView() でRTV作成
    hr_ = device->CreateRenderTargetView(backBuffer_.Get(), NULL, renderTargetView.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr_), L"miss CreateRenderTargetView");


    //成功時 true を返す
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
            copyDepthStencilSRV.ReleaseAndGetAddressOf());
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
    postRenderSystem.GetBloomSystem().Execute(
        rc,
        sceneSRV.Get(),
        renderTargetView.Get(),
        bloomParams
    );
}
void GraphicsDeviceManager::SetRenderTargets()
{
    immediateContext->RSSetViewports(1, &viewport);
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
    HRESULT result_ = device->CreateTexture2D(&desc, nullptr, texture.GetAddressOf());
    if (FAILED(result_)) return false;

    device->CreateRenderTargetView(texture.Get(), nullptr, sceneRTV.ReleaseAndGetAddressOf());
    device->CreateShaderResourceView(texture.Get(), nullptr, sceneSRV.ReleaseAndGetAddressOf());

    // --- ② 【追加】シーン法線バッファ ---
    D3D11_TEXTURE2D_DESC nDesc = desc; // カラーの設定を流用
    nDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT; // 法線用

    Microsoft::WRL::ComPtr<ID3D11Texture2D> nTexture;
    device->CreateTexture2D(&nDesc, nullptr, nTexture.GetAddressOf());
    device->CreateRenderTargetView(nTexture.Get(), nullptr, sceneNormalRTV.ReleaseAndGetAddressOf());
    device->CreateShaderResourceView(nTexture.Get(), nullptr, sceneNormalSRV.ReleaseAndGetAddressOf());

    D3D11_TEXTURE2D_DESC distortionDesc_ = desc;
    distortionDesc_.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> distortionTexture_;
    result_ = device->CreateTexture2D(&distortionDesc_, nullptr, distortionTexture_.GetAddressOf());
    if (FAILED(result_)) return false;
    result_ = device->CreateRenderTargetView(distortionTexture_.Get(), nullptr, sceneDistortionRTV.ReleaseAndGetAddressOf());
    if (FAILED(result_)) return false;
    result_ = device->CreateShaderResourceView(distortionTexture_.Get(), nullptr, sceneDistortionSRV.ReleaseAndGetAddressOf());
    if (FAILED(result_)) return false;

    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    result_ = device->CreateTexture2D(&desc, nullptr, copySceneColorTexture.ReleaseAndGetAddressOf());
    if (FAILED(result_)) return false;

    result_ = device->CreateShaderResourceView(copySceneColorTexture.Get(), nullptr, copySceneColorSRV.ReleaseAndGetAddressOf());
    if (FAILED(result_)) return false;

    // --- 法線のコピー用 ---
    result_ = device->CreateTexture2D(&desc, nullptr, copyNormalTexture.ReleaseAndGetAddressOf());
    if (FAILED(result_)) return false;

    result_ = device->CreateShaderResourceView(copyNormalTexture.Get(), nullptr, copyNormalSRV.ReleaseAndGetAddressOf());
    if (FAILED(result_)) return false;

    return true;
}

bool GraphicsDeviceManager::CreateGBufferTargets(int width_, int height_)
{
    struct Target_
    {
        DXGI_FORMAT format_;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView>* rtv_;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>* srv_;
    };

    Target_ targets_[] =
    {
        { DXGI_FORMAT_R16G16B16A16_FLOAT, &gBufferBaseColorRTV, &gBufferBaseColorSRV },
        { DXGI_FORMAT_R16G16B16A16_FLOAT, &gBufferNormalRTV, &gBufferNormalSRV },
        { DXGI_FORMAT_R8G8B8A8_UNORM, &gBufferMaterialRTV, &gBufferMaterialSRV },
        { DXGI_FORMAT_R16G16B16A16_FLOAT, &gBufferEmissiveRTV, &gBufferEmissiveSRV },
    };

    for (Target_& target_ : targets_)
    {
        D3D11_TEXTURE2D_DESC desc_{};
        desc_.Width = width_;
        desc_.Height = height_;
        desc_.MipLevels = 1;
        desc_.ArraySize = 1;
        desc_.Format = target_.format_;
        desc_.SampleDesc.Count = 1;
        desc_.Usage = D3D11_USAGE_DEFAULT;
        desc_.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture_;
        if (FAILED(device->CreateTexture2D(&desc_, nullptr, texture_.GetAddressOf())))
        {
            return false;
        }
        if (FAILED(device->CreateRenderTargetView(texture_.Get(), nullptr, target_.rtv_->ReleaseAndGetAddressOf())))
        {
            return false;
        }
        if (FAILED(device->CreateShaderResourceView(texture_.Get(), nullptr, target_.srv_->ReleaseAndGetAddressOf())))
        {
            return false;
        }
    }
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

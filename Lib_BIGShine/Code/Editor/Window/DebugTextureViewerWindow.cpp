#include "PrecompiledHeader.h"

#include "DebugTextureViewerWindow.h"

#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Graphics/RenderSystem/BloomSystem.h"
#include "Engine/Graphics/RenderSystem/CascadeShadowMap.h"
#include "Engine/Graphics/RenderSystem/MotionBlurSystem.h"
#include "Engine/Graphics/RenderSystem/PostRenderSystem.h"
#include "Engine/Graphics/RenderSystem/XeGtaoSystem.h"

#include <algorithm>
#include <d3dcompiler.h>
#include <utility>
#include <wrl/client.h>

namespace
{
    void SetChannelPreviewShader(const ImDrawList* parentList_, const ImDrawCmd* command_)
    {
        (void)parentList_;
        auto* shader_ = static_cast<ID3D11PixelShader*>(command_->UserCallbackData);
        GraphicsDeviceManager::Instance().GetContext()->PSSetShader(shader_, nullptr, 0);
        shader_->Release();
    }

    void ReleasePreviewTexture(const ImDrawList* parentList_, const ImDrawCmd* command_)
    {
        (void)parentList_;
        static_cast<ID3D11ShaderResourceView*>(command_->UserCallbackData)->Release();
    }
}

void DebugTextureViewerWindow::Draw()
{
    GraphicsDeviceManager::Instance().GetPostRenderSystem().SetDebugCaptureEnabled(false);
    if (!isVisible)
    {
        return;
    }

    BS_PROFILE_SCOPE("DebugTextureViewerWindow::Draw");

    // レンダリング途中のSRVをカテゴリ別タブで確認するデバッグ用ビューア。
    if (BeginEditorWindow("Debug Texture Viewer"))
    {
        DrawStylePopup();

        ImGui::SliderFloat("Thumbnail Size", &thumbnailSize, 96.0f, 512.0f, "%.0f");
        ImGui::Checkbox("Keep Aspect", &keepAspect);
        ImGui::SameLine();
        ImGui::Checkbox("Show Missing", &showMissingTextures);

        if (ImGui::BeginTabBar("DebugTextureViewerTabs"))
        {
            auto drawTab_ = [&](const char* label_, void (DebugTextureViewerWindow::*addTextures_)(std::vector<TextureEntry>&))
                {
                    // タブを開いた時だけ対象カテゴリのTextureEntryを集め、現在のRenderSystem状態を表示する。
                    if (ImGui::BeginTabItem(label_))
                    {
                        std::vector<TextureEntry> textures_;
                        (this->*addTextures_)(textures_);
                        DrawTextureGrid(textures_);
                        ImGui::EndTabItem();
                    }
                };

            drawTab_("Scene", &DebugTextureViewerWindow::AddSceneTextures);
            drawTab_("GBuffer", &DebugTextureViewerWindow::AddGBufferTextures);
            drawTab_("Post Effect", &DebugTextureViewerWindow::AddPostEffectTextures);
            drawTab_("Shadow Map", &DebugTextureViewerWindow::AddShadowTextures);

            ImGui::EndTabBar();
        }
    }

    EndEditorWindow();
}

void DebugTextureViewerWindow::DrawTextureGrid(const std::vector<TextureEntry>& textures_)
{
    BS_PROFILE_SCOPE("DebugTextureViewerWindow::DrawTextureGrid");

    // ウィンドウ幅とサムネイルサイズから列数を決め、TextureEntryをグリッド表示する。
    const float spacing_ = ImGui::GetStyle().ItemSpacing.x;
    const float availableWidth_ = ImGui::GetContentRegionAvail().x;
    const int columnCount_ = std::max(1, static_cast<int>((availableWidth_ + spacing_) / (thumbnailSize + spacing_)));

    if (ImGui::BeginTable("DebugTextureGrid", columnCount_, ImGuiTableFlags_SizingFixedFit))
    {
        int visibleIndex_ = 0;
        for (const TextureEntry& texture_ : textures_)
        {
            if (!showMissingTextures && texture_.srv == nullptr)
            {
                continue;
            }

            ImGui::TableNextColumn();
            ImGui::PushID(visibleIndex_++);
            DrawTexture(texture_);
            ImGui::PopID();
        }

        ImGui::EndTable();
    }
}

void DebugTextureViewerWindow::DrawTexture(const TextureEntry& texture_)
{
    // SRVが無い項目もShow Missingが有効なら枠だけ表示し、生成漏れを見つけやすくする。
    ImGui::TextUnformatted(texture_.name);

    const ImVec2 imageSize_ = GetTextureImageSize(texture_.srv);
    if (texture_.srv)
    {
        const bool singleChannel_ = texture_.channel >= 0 && texture_.channel < static_cast<int>(channelShaders.size());
        if (singleChannel_ && !EnsureChannelShaders())
        {
            ImGui::TextWrapped("Channel preview shader unavailable. See debug output.");
            return;
        }
        auto* drawList_ = ImGui::GetWindowDrawList();
        if (singleChannel_)
        {
            auto* shader_ = channelShaders[texture_.channel].Get();
            shader_->AddRef();
            drawList_->AddCallback(SetChannelPreviewShader, shader_);
        }
        // ImGuiの遅延描画が終わるまで、リサイズ前のSRVも保持する。
        texture_.srv->AddRef();
        ImGui::Image(reinterpret_cast<void*>(texture_.srv), imageSize_);
        if (singleChannel_ && ImGui::IsItemHovered())
        {
            constexpr const char* CHANNEL_DESCRIPTIONS[] =
            {
                "Black: Occluded / White: Unoccluded",
                "Black: Smooth / White: Rough",
                "Black: Non-metal / White: Metal",
                "Black: PBR / White: Toon"
            };
            ImGui::SetTooltip("%s", CHANNEL_DESCRIPTIONS[texture_.channel]);
        }
        drawList_->AddCallback(ReleasePreviewTexture, texture_.srv);
        if (singleChannel_)
        {
            drawList_->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
        }
    }
    else
    {
        ImGui::BeginDisabled();
        ImGui::Button("No Texture", imageSize_);
        ImGui::EndDisabled();
    }
}

bool DebugTextureViewerWindow::EnsureChannelShaders()
{
    if (channelShaderInitializationAttempted)
    {
        return channelShaders[0] != nullptr;
    }
    auto* device_ = GraphicsDeviceManager::Instance().GetDevice();
    if (!device_)
    {
        return false;
    }
    channelShaderInitializationAttempted = true;

    // ImGuiの頂点出力をそのまま受け取り、選択成分を不透明な白黒へ変換する。
    constexpr char SHADER_SOURCE[] = R"(
        Texture2D sourceTexture : register(t0);
        SamplerState sourceSampler : register(s0);
        struct PixelInput
        {
            float4 position : SV_POSITION;
            float4 color : COLOR0;
            float2 uv : TEXCOORD0;
        };
        float4 main(PixelInput input_) : SV_Target
        {
            float value_ = sourceTexture.Sample(sourceSampler, input_.uv).CHANNEL;
            return float4(value_, value_, value_, 1.0f) * input_.color;
        }
    )";
    constexpr const char* CHANNEL_NAMES[] = { "r", "g", "b", "a" };
    std::array<Microsoft::WRL::ComPtr<ID3D11PixelShader>, 4> shaders_;
    for (std::size_t index_ = 0; index_ < shaders_.size(); ++index_)
    {
        const D3D_SHADER_MACRO macros_[] = { { "CHANNEL", CHANNEL_NAMES[index_] }, { nullptr, nullptr } };
        Microsoft::WRL::ComPtr<ID3DBlob> bytecode_;
        Microsoft::WRL::ComPtr<ID3DBlob> errors_;
        if (FAILED(D3DCompile(SHADER_SOURCE, sizeof(SHADER_SOURCE) - 1, "DebugTextureChannel", macros_, nullptr,
            "main", "ps_4_0", 0, 0, bytecode_.GetAddressOf(), errors_.GetAddressOf())))
        {
            OutputDebugStringA("Debug Texture Viewer: channel shader compilation failed.\n");
            if (errors_)
            {
                OutputDebugStringA(static_cast<const char*>(errors_->GetBufferPointer()));
            }
            return false;
        }
        if (FAILED(device_->CreatePixelShader(bytecode_->GetBufferPointer(), bytecode_->GetBufferSize(),
            nullptr, shaders_[index_].GetAddressOf())))
        {
            OutputDebugStringA("Debug Texture Viewer: channel shader creation failed.\n");
            return false;
        }
    }
    channelShaders = std::move(shaders_);
    return true;
}

ImVec2 DebugTextureViewerWindow::GetTextureImageSize(ID3D11ShaderResourceView* srv_) const
{
    // Keep Aspectが有効な時はTexture2DのDescから比率を取り、正方形に引き伸ばさないようにする。
    if (!keepAspect || srv_ == nullptr)
    {
        return ImVec2(thumbnailSize, thumbnailSize);
    }

    Microsoft::WRL::ComPtr<ID3D11Resource> resource_;
    srv_->GetResource(resource_.GetAddressOf());
    if (!resource_)
    {
        return ImVec2(thumbnailSize, thumbnailSize);
    }

    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture_;
    if (FAILED(resource_.As(&texture_)) || !texture_)
    {
        return ImVec2(thumbnailSize, thumbnailSize);
    }

    D3D11_TEXTURE2D_DESC desc_{};
    texture_->GetDesc(&desc_);
    if (desc_.Width == 0 || desc_.Height == 0)
    {
        return ImVec2(thumbnailSize, thumbnailSize);
    }

    const float aspect_ = static_cast<float>(desc_.Width) / static_cast<float>(desc_.Height);
    if (aspect_ >= 1.0f)
    {
        return ImVec2(thumbnailSize, thumbnailSize / aspect_);
    }

    return ImVec2(thumbnailSize * aspect_, thumbnailSize);
}

void DebugTextureViewerWindow::AddSceneTextures(std::vector<TextureEntry>& textures_)
{
    // Sceneタブでは最終色、法線、深度、歪み、Dock表示用の基本RTを並べる。
    GraphicsDeviceManager& graphicsDeviceManager_ = GraphicsDeviceManager::Instance();

    textures_.push_back({ "Scene Color", graphicsDeviceManager_.GetSceneSRV() });
    textures_.push_back({ "Scene Normal", graphicsDeviceManager_.GetSceneNormalSRV() });
    textures_.push_back({ "Main Depth", graphicsDeviceManager_.GetMainDepthSRV() });
    textures_.push_back({ "Scene Distortion", graphicsDeviceManager_.GetSceneDistortionSRV() });
    textures_.push_back({ "Docked Scene View", graphicsDeviceManager_.GetSceneViewSRV() });
}

void DebugTextureViewerWindow::AddGBufferTextures(std::vector<TextureEntry>& textures_)
{
    // Deferred/ポスト処理の入力になるGBuffer群を確認する。
    GraphicsDeviceManager& graphicsDeviceManager_ = GraphicsDeviceManager::Instance();

    textures_.push_back({ "GBuffer Base Color", graphicsDeviceManager_.GetGBufferBaseColorSRV() });
    textures_.push_back({ "GBuffer Normal", graphicsDeviceManager_.GetGBufferNormalSRV() });
    auto* material_ = graphicsDeviceManager_.GetGBufferMaterialSRV();
    textures_.push_back({ "Ambient Occlusion (AO)", material_, 0 });
    textures_.push_back({ "Roughness", material_, 1 });
    textures_.push_back({ "Metalness", material_, 2 });
    textures_.push_back({ "Toon / PBR Blend", material_, 3 });
    textures_.push_back({ "GBuffer Material (Packed)", material_ });
    textures_.push_back({ "GBuffer Emissive", graphicsDeviceManager_.GetGBufferEmissiveSRV() });
}

void DebugTextureViewerWindow::AddPostEffectTextures(std::vector<TextureEntry>& textures_)
{
    // ポストエフェクトの作業バッファと各サブシステムの中間結果を一覧化する。
    GraphicsDeviceManager& graphicsDeviceManager_ = GraphicsDeviceManager::Instance();
    PostRenderSystem& postRenderSystem_ = graphicsDeviceManager_.GetPostRenderSystem();
    postRenderSystem_.SetDebugCaptureEnabled(true);
    BloomSystem& bloomSystem_ = postRenderSystem_.GetBloomSystem();
    XeGtaoSystem& xeGtaoSystem_ = postRenderSystem_.GetXeGtaoSystem();
    MotionBlurSystem& motionBlurSystem_ = postRenderSystem_.GetMotionBlurSystem();

    textures_.push_back({ "Post Work 0", postRenderSystem_.GetWorkBufferSRV(0) });
    textures_.push_back({ "Post Work 1", postRenderSystem_.GetWorkBufferSRV(1) });
    textures_.push_back({ "Bloom Composite", postRenderSystem_.GetBloomCompositeDebugSRV() });
    textures_.push_back({ "Fog Result", postRenderSystem_.GetFogDebugSRV() });
    textures_.push_back({ "Vignette Result", postRenderSystem_.GetVignetteDebugSRV() });
    textures_.push_back({ "FXAA Result", postRenderSystem_.GetFxaaDebugSRV() });
    textures_.push_back({ "Bloom Bright", bloomSystem_.GetBrightSRV() });

    static const char* bloomBlurNames_[] =
    {
        "Bloom Blur 0",
        "Bloom Blur 1",
        "Bloom Blur 2",
        "Bloom Blur 3",
        "Bloom Blur 4",
        "Bloom Blur 5",
        "Bloom Blur 6",
        "Bloom Blur 7"
    };

    const size_t bloomBlurCount_ = std::min(
        bloomSystem_.GetBlurTargetCount(),
        static_cast<size_t>(_countof(bloomBlurNames_)));
    for (size_t index_ = 0; index_ < bloomBlurCount_; ++index_)
    {
        textures_.push_back({ bloomBlurNames_[index_], bloomSystem_.GetBlurSRV(index_) });
    }

    textures_.push_back({ "GTAO AO", xeGtaoSystem_.GetAoSRV() });
    textures_.push_back({ "Motion Velocity", motionBlurSystem_.GetVelocitySRV() });
    textures_.push_back({ "Motion Tile Max", motionBlurSystem_.GetTileMaxSRV() });
    textures_.push_back({ "Motion Neighbor Max", motionBlurSystem_.GetNeighborMaxSRV() });
    textures_.push_back({ "Distortion", graphicsDeviceManager_.GetSceneDistortionSRV() });
}

void DebugTextureViewerWindow::AddShadowTextures(std::vector<TextureEntry>& textures_)
{
    // Cascade Shadowの各カスケードSRVを確認し、シャドウマップの範囲や欠落を見やすくする。
    CascadeShadowMap& cascadeShadowMap_ = GraphicsDeviceManager::Instance().GetCascadeShadowMap();

    textures_.push_back({ "Cascade Shadow 0", cascadeShadowMap_.GetSRV(0) });
    textures_.push_back({ "Cascade Shadow 1", cascadeShadowMap_.GetSRV(1) });
    textures_.push_back({ "Cascade Shadow 2", cascadeShadowMap_.GetSRV(2) });
    textures_.push_back({ "Cascade Shadow 3", cascadeShadowMap_.GetSRV(3) });
}

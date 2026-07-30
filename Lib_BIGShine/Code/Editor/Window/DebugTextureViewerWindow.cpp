#include "PrecompiledHeader.h"

#include "DebugTextureViewerWindow.h"

#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Graphics/RenderSystem/BloomSystem.h"
#include "Engine/Graphics/RenderSystem/CascadeShadowMap.h"
#include "Engine/Graphics/RenderSystem/MotionBlurSystem.h"
#include "Engine/Graphics/RenderSystem/PostRenderSystem.h"
#include "Engine/Graphics/RenderSystem/XeGtaoSystem.h"

#include <algorithm>
#include <wrl/client.h>

void DebugTextureViewerWindow::Draw()
{
    if (!isVisible) return;

    BS_PROFILE_SCOPE("DebugTextureViewerWindow::Draw");

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
    ImGui::TextUnformatted(texture_.name);

    const ImVec2 imageSize_ = GetTextureImageSize(texture_.srv);
    if (texture_.srv)
    {
        ImGui::Image(reinterpret_cast<void*>(texture_.srv), imageSize_);
    }
    else
    {
        ImGui::BeginDisabled();
        ImGui::Button("No Texture", imageSize_);
        ImGui::EndDisabled();
    }
}

ImVec2 DebugTextureViewerWindow::GetTextureImageSize(ID3D11ShaderResourceView* srv_) const
{
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
    GraphicsDeviceManager& graphicsDeviceManager_ = GraphicsDeviceManager::Instance();

    textures_.push_back({ "Scene Color", graphicsDeviceManager_.GetSceneSRV() });
    textures_.push_back({ "Scene Normal", graphicsDeviceManager_.GetSceneNormalSRV() });
    textures_.push_back({ "Main Depth", graphicsDeviceManager_.GetMainDepthSRV() });
    textures_.push_back({ "Scene Distortion", graphicsDeviceManager_.GetSceneDistortionSRV() });
    textures_.push_back({ "Docked Scene View", graphicsDeviceManager_.GetSceneViewSRV() });
}

void DebugTextureViewerWindow::AddGBufferTextures(std::vector<TextureEntry>& textures_)
{
    GraphicsDeviceManager& graphicsDeviceManager_ = GraphicsDeviceManager::Instance();

    textures_.push_back({ "GBuffer Base Color", graphicsDeviceManager_.GetGBufferBaseColorSRV() });
    textures_.push_back({ "GBuffer Normal", graphicsDeviceManager_.GetGBufferNormalSRV() });
    textures_.push_back({ "GBuffer Material", graphicsDeviceManager_.GetGBufferMaterialSRV() });
    textures_.push_back({ "GBuffer Emissive", graphicsDeviceManager_.GetGBufferEmissiveSRV() });
}

void DebugTextureViewerWindow::AddPostEffectTextures(std::vector<TextureEntry>& textures_)
{
    GraphicsDeviceManager& graphicsDeviceManager_ = GraphicsDeviceManager::Instance();
    PostRenderSystem& postRenderSystem_ = graphicsDeviceManager_.GetPostRenderSystem();
    BloomSystem& bloomSystem_ = postRenderSystem_.GetBloomSystem();
    XeGtaoSystem& xeGtaoSystem_ = postRenderSystem_.GetXeGtaoSystem();
    MotionBlurSystem& motionBlurSystem_ = postRenderSystem_.GetMotionBlurSystem();

    textures_.push_back({ "Post Work 0", postRenderSystem_.GetWorkBufferSRV(0) });
    textures_.push_back({ "Post Work 1", postRenderSystem_.GetWorkBufferSRV(1) });
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
    CascadeShadowMap& cascadeShadowMap_ = GraphicsDeviceManager::Instance().GetCascadeShadowMap();

    textures_.push_back({ "Cascade Shadow 0", cascadeShadowMap_.GetSRV(0) });
    textures_.push_back({ "Cascade Shadow 1", cascadeShadowMap_.GetSRV(1) });
    textures_.push_back({ "Cascade Shadow 2", cascadeShadowMap_.GetSRV(2) });
    textures_.push_back({ "Cascade Shadow 3", cascadeShadowMap_.GetSRV(3) });
}

#include "ParticleEditorWindow.h"

#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Graphics/Core/RenderContext.h"

#include <DirectXMath.h>
#include <filesystem>

ParticleEditorWindow::ParticleEditorWindow(GameObjectManager* manager_)
    : EditorWorkspaceWindow("Particle Editor", manager_)
{
    isVisible = false;
    style.backgroundColor = ImVec4(0.08f, 0.09f, 0.10f, 1.0f);
    style.titleBarColor = ImVec4(0.10f, 0.34f, 0.18f, 1.0f);
}

void ParticleEditorWindow::Draw()
{
    EnsurePreviewObjects();

    if (!BeginWorkspaceWindow(ImGuiWindowFlags_MenuBar))
    {
        EndEditorWindow();
        return;
    }

    if (ImGui::BeginMenuBar())
    {
        DrawToolbar();
        ImGui::EndMenuBar();
    }

    const float rightWidth_ = 360.0f;
    const float bottomHeight_ = 190.0f;
    ImVec2 available_ = ImGui::GetContentRegionAvail();
    float leftWidth_ = (std::max)(240.0f, available_.x - rightWidth_);
    float previewHeight_ = (std::max)(140.0f, available_.y - bottomHeight_);

    ImGui::BeginGroup();
    ImGui::BeginChild("ParticlePreviewPanel", ImVec2(leftWidth_, previewHeight_), true);
    DrawPreviewPanel();
    ImGui::EndChild();

    ImGui::BeginChild("ParticleAssetBrowserPanel", ImVec2(leftWidth_, 0.0f), true);
    DrawAssetBrowserPanel();
    ImGui::EndChild();
    ImGui::EndGroup();

    ImGui::SameLine();

    ImGui::BeginChild("ParticleInspectorPanel", ImVec2(0.0f, 0.0f), true);
    DrawInspectorPanel();
    ImGui::EndChild();

    EndEditorWindow();
}

void ParticleEditorWindow::EnsurePreviewObjects()
{
    if (isInitialized) return;

    auto& graphics_ = GraphicsDeviceManager::Instance();
    particleRenderSystem.Initialize(graphics_.GetDevice());

    previewObject = std::make_shared<GameObject>("ParticlePreviewObject");
    previewObject->transform->gameObject = previewObject;
    previewParticle = previewObject->AddComponent<ParticleComponent>();
    if (previewParticle)
    {
        previewParticle->RefreshTextureList();
    }

    previewCamera.SetLookAt({ 0.0f, 1.5f, -6.0f }, { 0.0f, 0.4f, 0.0f }, { 0.0f, 1.0f, 0.0f });
    previewCamera.SetPerspectiveFov(45.0f, 16.0f / 9.0f, 0.01f, 100.0f);

    RefreshParticleAssets();
    isInitialized = true;
}

void ParticleEditorWindow::DrawToolbar()
{
    if (ImGui::MenuItem("Refresh Assets"))
    {
        RefreshParticleAssets();
    }

    ImGui::Separator();

    if (ImGui::MenuItem("Play", nullptr, false, previewParticle != nullptr))
    {
        previewParticle->Play();
    }

    ImGui::MenuItem("Auto Play", nullptr, &autoPlay);
    ImGui::MenuItem("Grid", nullptr, &showGrid);
}

void ParticleEditorWindow::DrawPreviewPanel()
{
    ImGui::TextUnformatted("Preview");
    ImGui::Separator();

    ImVec2 previewSize_ = ImGui::GetContentRegionAvail();
    previewSize_.x = (std::max)(1.0f, previewSize_.x);
    previewSize_.y = (std::max)(1.0f, previewSize_.y);

    RenderPreview(previewSize_);

    if (previewTarget.GetShaderResourceView())
    {
        ImGui::Image(reinterpret_cast<void*>(previewTarget.GetShaderResourceView()), previewSize_);
    }
    else
    {
        ImGui::Dummy(previewSize_);
    }
}

void ParticleEditorWindow::DrawAssetBrowserPanel()
{
    ImGui::TextUnformatted("Data/Particle");
    ImGui::Separator();

    if (ImGui::Button("Refresh"))
    {
        RefreshParticleAssets();
    }
    ImGui::SameLine();
    if (ImGui::Button("Load Selected") &&
        previewParticle &&
        selectedAssetIndex >= 0 &&
        selectedAssetIndex < static_cast<int>(particleAssetPaths.size()))
    {
        previewParticle->LoadAsset(particleAssetPaths[selectedAssetIndex]);
        previewParticle->Play();
    }

    ImGui::Separator();

    if (particleAssetPaths.empty())
    {
        ImGui::TextDisabled("No particle json assets found.");
        return;
    }

    for (int i_ = 0; i_ < static_cast<int>(particleAssetPaths.size()); ++i_)
    {
        const std::filesystem::path path_(particleAssetPaths[i_]);
        if (ImGui::Selectable(path_.filename().string().c_str(), selectedAssetIndex == i_))
        {
            selectedAssetIndex = i_;
        }
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && previewParticle)
        {
            previewParticle->LoadAsset(particleAssetPaths[i_]);
            previewParticle->Play();
        }
    }
}

void ParticleEditorWindow::DrawInspectorPanel()
{
    ImGui::TextUnformatted("Particle Settings");
    ImGui::Separator();

    if (!previewParticle)
    {
        ImGui::TextDisabled("Preview particle is not ready.");
        return;
    }

    previewParticle->Gui();
}

void ParticleEditorWindow::RenderPreview(const ImVec2& previewSize_)
{
    auto& graphics_ = GraphicsDeviceManager::Instance();
    ID3D11Device* device_ = graphics_.GetDevice();
    ID3D11DeviceContext* context_ = graphics_.GetContext();
    if (!device_ || !context_ || !previewParticle) return;

    const int width_ = (std::max)(1, static_cast<int>(previewSize_.x));
    const int height_ = (std::max)(1, static_cast<int>(previewSize_.y));
    if (!previewTarget.Resize(device_, width_, height_)) return;

    previewCamera.SetPerspectiveFov(45.0f, static_cast<float>(width_) / static_cast<float>(height_), 0.01f, 100.0f);

    const float clearColor_[4] = { 0.025f, 0.028f, 0.032f, 1.0f };
    previewTarget.Begin(context_, clearColor_);

    const float deltaTime_ = ImGui::GetIO().DeltaTime;
    if (autoPlay)
    {
        previewParticle->Update(deltaTime_);
    }

    RenderContext renderContext_{};
    renderContext_.device = device_;
    renderContext_.deviceContext = context_;
    renderContext_.renderState = graphics_.GetRenderState();
    renderContext_.camera = &previewCamera;
    renderContext_.backBufferRTV = nullptr;
    renderContext_.depthSRV = previewTarget.GetDepthShaderResourceView();
    renderContext_.screenWidth = static_cast<float>(width_);
    renderContext_.screenHeight = static_cast<float>(height_);

    particleRenderSystem.ApplySceneConstants(renderContext_);
    graphics_.RenderSkyBox(renderContext_);

    particleRenderSystem.PushSubmit(previewParticle.get());
    particleRenderSystem.RenderAll(renderContext_);

    previewTarget.End(context_);
}

void ParticleEditorWindow::RefreshParticleAssets()
{
    particleAssetPaths.clear();
    const std::filesystem::path particleDir_("Data/Particle");
    if (!std::filesystem::exists(particleDir_)) return;

    for (const auto& entry_ : std::filesystem::directory_iterator(particleDir_))
    {
        if (!entry_.is_regular_file()) continue;
        if (entry_.path().extension() == ".json")
        {
            particleAssetPaths.push_back(entry_.path().generic_string());
        }
    }
}

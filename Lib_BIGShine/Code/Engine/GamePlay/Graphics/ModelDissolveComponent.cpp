#include "PrecompiledHeader.h"
#include "ModelDissolveComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/Graphics/Resource/GlTFModel.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include <algorithm>
#include <imgui_stdlib.h>

void ModelDissolveComponent::Start()
{
    noiseMaterial.SetTexture(ModelMaterial::TextureSlot::NORMAL, noiseTexturePath);
    Reset(startVisible);
    RegisterActions();
}

void ModelDissolveComponent::RegisterActions()
{
    const auto owner_ = GetGameObject();
    if (!owner_) return;
    const std::weak_ptr<GameObject> weakOwner_ = owner_;
    const auto register_ = [&](const std::string& name_, bool reveal_, bool reset_)
    {
        owner_->RegisterGlobalAction("ModelDissolve", name_, [weakOwner_, reveal_, reset_]()
        {
            const auto owner_ = weakOwner_.lock();
            const auto component_ = owner_ ? owner_->GetComponent<ModelDissolveComponent>() : nullptr;
            if (!component_) return;
            if (reset_) component_->Reset(reveal_);
            else component_->Play(reveal_);
        });
    };
    register_("DissolveOut", false, false);
    register_("DissolveIn", true, false);
    register_("ResetVisible", true, true);
    register_("ResetHidden", false, true);
}

void ModelDissolveComponent::Reset(bool visible_)
{
    playing = false;
    progress = visible_ ? 0.0f : 1.0f;
    const auto owner_ = GetGameObject();
    if (const auto model_ = owner_ ? owner_->GetComponent<GltfModelCommponennt>() : nullptr)
        model_->SetRuntimeVisible(visible_);
}

void ModelDissolveComponent::Play(bool reveal_)
{
    BS_PROFILE_SCOPE("ModelDissolveComponent::Play");
    startProgress = progress;
    targetProgress = reveal_ ? 0.0f : 1.0f;
    elapsed = 0.0f;
    duration = reveal_ ? revealDuration : disappearDuration;
    playing = true;
    // 形成中はモデルを描画キューへ戻し、Shaderのclipで輪郭を段階的に出す。
    const auto owner_ = GetGameObject();
    if (const auto model_ = owner_ ? owner_->GetComponent<GltfModelCommponennt>() : nullptr)
        model_->SetRuntimeVisible(true);
    if (duration <= 0.0f) Reset(reveal_);
}

void ModelDissolveComponent::UpdateLoadState()
{
    noiseMaterial.LoadPendingTextures(GraphicsDeviceManager::Instance().GetDevice());
}

ID3D11ShaderResourceView* ModelDissolveComponent::GetNoiseTexture() const
{
    return noiseMaterial.GetTexture(ModelMaterial::TextureSlot::NORMAL);
}

void ModelDissolveComponent::Update(float deltaTime_)
{
    BS_PROFILE_SCOPE("ModelDissolveComponent::Update");
    // 実行中のEditor差し替えも、再生やScene再読込を待たずに読み込みへ反映する。
    UpdateLoadState();
    if (!playing) return;
    const auto owner_ = GetGameObject();
    const auto model_ = owner_ ? owner_->GetComponent<GltfModelCommponennt>() : nullptr;
    if (!model_ || !model_->IsModelReady()) return;
    elapsed += (std::max)(deltaTime_, 0.0f);
    const float rate_ = duration <= 0.0f ? 1.0f : std::clamp(elapsed / duration, 0.0f, 1.0f);
    const float eased_ = rate_ * rate_ * (3.0f - 2.0f * rate_);
    progress = startProgress + (targetProgress - startProgress) * eased_;
    if (rate_ >= 1.0f) Reset(targetProgress <= 0.0f);
}

void ModelDissolveComponent::Gui()
{
    ImGui::Checkbox("Start Visible", &startVisible);
    ImGui::DragFloat("Disappear Duration", &disappearDuration, 0.05f, 0.0f, 60.0f);
    ImGui::DragFloat("Reveal Duration", &revealDuration, 0.05f, 0.0f, 60.0f);
    ImGui::ColorEdit4("Edge Color", &settings.edgeColor.x);
    ImGui::DragFloat("Edge Width", &settings.edgeWidth, 0.005f, 0.001f, 1.0f);
    ImGui::DragFloat("Edge Emission", &settings.edgeEmission, 0.1f, 0.0f, 50.0f);
    ImGui::DragFloat("Noise Scale", &settings.noiseScale, 0.1f, 0.01f, 100.0f);
    ImGui::DragFloat3("Dissolve Direction", &settings.direction.x, 0.05f);
    ImGui::SliderFloat("Directional Weight", &settings.directionalWeight, 0.0f, 1.0f);
    ImGui::DragFloat("Direction Extent", &settings.directionExtent, 0.1f, 0.01f, 200.0f);
    if (ImGui::InputText("Noise Texture", &noiseTexturePath))
        noiseMaterial.SetTexture(ModelMaterial::TextureSlot::NORMAL, noiseTexturePath);
    if (ImGui::Button("Dissolve Out")) Play(false);
    ImGui::SameLine();
    if (ImGui::Button("Dissolve In")) Play(true);
    if (ImGui::Button("Reset Visible")) Reset(true);
    ImGui::SameLine();
    if (ImGui::Button("Reset Hidden")) Reset(false);
    ImGui::Text("Progress: %.3f", progress);
}

void ModelDissolveComponent::Serialize(nlohmann::json& json_) const
{
    json_ = { {"StartVisible", startVisible}, {"DisappearDuration", disappearDuration},
        {"RevealDuration", revealDuration}, {"NoiseTexturePath", noiseTexturePath},
        {"NoiseScale", settings.noiseScale}, {"EdgeWidth", settings.edgeWidth},
        {"EdgeEmission", settings.edgeEmission}, {"DirectionalWeight", settings.directionalWeight},
        {"DirectionExtent", settings.directionExtent},
        {"Direction", {settings.direction.x, settings.direction.y, settings.direction.z}},
        {"EdgeColor", {settings.edgeColor.x, settings.edgeColor.y, settings.edgeColor.z, settings.edgeColor.w}} };
}

void ModelDissolveComponent::Deserialize(const nlohmann::json& json_)
{
    startVisible = json_.value("StartVisible", true);
    disappearDuration = (std::max)(json_.value("DisappearDuration", 4.0f), 0.0f);
    revealDuration = (std::max)(json_.value("RevealDuration", 3.0f), 0.0f);
    noiseTexturePath = json_.value("NoiseTexturePath", std::string{});
    settings.noiseScale = (std::max)(json_.value("NoiseScale", 3.0f), 0.01f);
    settings.edgeWidth = (std::max)(json_.value("EdgeWidth", 0.06f), 0.001f);
    settings.edgeEmission = (std::max)(json_.value("EdgeEmission", 2.0f), 0.0f);
    settings.directionalWeight = std::clamp(json_.value("DirectionalWeight", 0.4f), 0.0f, 1.0f);
    settings.directionExtent = (std::max)(json_.value("DirectionExtent", 10.0f), 0.01f);
    if (json_.contains("Direction") && json_["Direction"].is_array() && json_["Direction"].size() == 3)
        settings.direction = { json_["Direction"][0], json_["Direction"][1], json_["Direction"][2] };
    if (json_.contains("EdgeColor") && json_["EdgeColor"].is_array() && json_["EdgeColor"].size() == 4)
        settings.edgeColor = { json_["EdgeColor"][0], json_["EdgeColor"][1], json_["EdgeColor"][2], json_["EdgeColor"][3] };
    noiseMaterial.SetTexture(ModelMaterial::TextureSlot::NORMAL, noiseTexturePath);
}

REGISTER_COMPONENT(ModelDissolveComponent, "ModelDissolveComponent")

#include "PrecompiledHeader.h"

#include "ModelMaterialComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Graphics/Resource/GlTFModel.h"
#include "Editor/AssetSlotGui.h"

namespace
{
    constexpr const char* TEXTURE_LABELS[] =
    {
        "Base Color", "Normal", "Emissive", "Occlusion", "Metalness / Roughness"
    };
}

void ModelMaterialComponent::Start()
{
    UpdateLoadState();
}

void ModelMaterialComponent::UpdateLoadState()
{
    // モデルの非同期ロードやComponentの追加順に依存せず、描画前にも準備できる。
    ID3D11Device* device_ = GraphicsDeviceManager::Instance().GetDevice();
    for (auto& entry_ : materials)
    {
        entry_.second.LoadPendingTextures(device_);
    }
}

ModelMaterial& ModelMaterialComponent::GetMaterial(int materialIndex_)
{
    return materials[materialIndex_ < -1 ? -1 : materialIndex_];
}

void ModelMaterialComponent::RemoveMaterial(int materialIndex_)
{
    materials.erase(materialIndex_);
}

ID3D11ShaderResourceView* ModelMaterialComponent::ResolveTexture(
    int materialIndex_, ModelMaterial::TextureSlot slot_,
    ID3D11ShaderResourceView* originalTexture_) const
{
    if (!IsEnabled()) return originalTexture_;
    const auto material_ = materials.find(materialIndex_);
    if (material_ != materials.end())
    {
        if (ID3D11ShaderResourceView* texture_ = material_->second.GetTexture(slot_)) return texture_;
    }
    const auto fallback_ = materials.find(-1);
    if (fallback_ != materials.end())
    {
        if (ID3D11ShaderResourceView* texture_ = fallback_->second.GetTexture(slot_)) return texture_;
    }
    return originalTexture_;
}

void ModelMaterialComponent::Gui()
{
    UpdateLoadState();
    ImGui::TextUnformatted("Priority: target material > all materials > original model.");
    const auto owner_ = GetGameObject();
    const auto model_ = owner_ ? owner_->GetComponent<GltfModelCommponennt>() : nullptr;
    std::string preview_ = selectedMaterialIndex < 0 ? "All Materials" : std::to_string(selectedMaterialIndex);
    if (ImGui::BeginCombo("Target Material", preview_.c_str()))
    {
        if (ImGui::Selectable("All Materials", selectedMaterialIndex == -1)) selectedMaterialIndex = -1;
        if (model_)
        {
            const auto& modelMaterials_ = model_->GetMaterials();
            for (int index_ = 0; index_ < static_cast<int>(modelMaterials_.size()); ++index_)
            {
                const std::string label_ = std::to_string(index_) + ": " + modelMaterials_[index_].name;
                if (ImGui::Selectable(label_.c_str(), selectedMaterialIndex == index_)) selectedMaterialIndex = index_;
            }
        }
        // モデルが未ロードでも、保存済みの個別設定を編集できるようにする。
        for (const auto& entry_ : materials)
        {
            if (entry_.first < 0 ||
                (model_ && entry_.first < static_cast<int>(model_->GetMaterials().size()))) continue;
            const std::string label_ = std::to_string(entry_.first) + ": Saved Override";
            if (ImGui::Selectable(label_.c_str(), selectedMaterialIndex == entry_.first)) selectedMaterialIndex = entry_.first;
        }
        ImGui::EndCombo();
    }
    if (!model_ || !model_->IsModelReady()) ImGui::TextDisabled("Owner model is not ready.");

    ModelMaterial& material_ = GetMaterial(selectedMaterialIndex);
    for (std::size_t index_ = 0; index_ < ModelMaterial::TEXTURE_SLOT_COUNT; ++index_)
    {
        const auto slot_ = static_cast<ModelMaterial::TextureSlot>(index_);
        std::string path_ = material_.GetTexturePath(slot_);
        if (AssetSlotGui::Draw(TEXTURE_LABELS[index_], AssetSlotType::TEXTURE,
            path_, material_.GetTexture(slot_), "Data"))
        {
            material_.SetTexture(slot_, path_);
        }
        if (material_.HasTextureFailure(slot_))
        {
            ImGui::TextDisabled("Texture load failed. Using fallback texture.");
        }
    }
    if (ImGui::Button("Reload Textures")) material_.ReloadTextures();
    ImGui::SameLine();
    if (ImGui::Button("Clear Material Override")) RemoveMaterial(selectedMaterialIndex);
    UpdateLoadState();
}

void ModelMaterialComponent::Serialize(nlohmann::json& json_) const
{
    BS_PROFILE_SCOPE("ModelMaterialComponent::Serialize");
    json_["Materials"] = nlohmann::json::array();
    for (const auto& entry_ : materials)
    {
        nlohmann::json materialJson_;
        materialJson_["MaterialIndex"] = entry_.first;
        entry_.second.Serialize(materialJson_["Textures"]);
        json_["Materials"].push_back(materialJson_);
    }
}

void ModelMaterialComponent::Deserialize(const nlohmann::json& json_)
{
    BS_PROFILE_SCOPE("ModelMaterialComponent::Deserialize");
    materials.clear();
    selectedMaterialIndex = -1;
    if (!json_.contains("Materials") || !json_["Materials"].is_array()) return;
    for (const auto& materialJson_ : json_["Materials"])
    {
        if (!materialJson_.is_object()) continue;
        const int materialIndex_ = materialJson_.value("MaterialIndex", -1);
        if (materialIndex_ < -1 || !materialJson_.contains("Textures") ||
            !materialJson_["Textures"].is_object()) continue;
        GetMaterial(materialIndex_).Deserialize(materialJson_["Textures"]);
    }
}
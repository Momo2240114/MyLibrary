#include "PrecompiledHeader.h"

#include "ModelMaterialComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Graphics/Resource/GlTFModel.h"
#include "Engine/Debug/TracyProfiler.h"
#include "Editor/AssetSlotGui.h"

namespace
{
    constexpr const char* TEXTURE_LABELS[] =
    {
        "Base Color", "Normal", "Emissive", "Occlusion", "Metalness / Roughness",
        "Metallic", "Roughness"
    };
    static_assert(sizeof(TEXTURE_LABELS) / sizeof(TEXTURE_LABELS[0]) == ModelMaterial::TEXTURE_SLOT_COUNT);
}

void ModelMaterialComponent::Start()
{
    UpdateLoadState();
}

void ModelMaterialComponent::UpdateLoadState()
{
    BS_PROFILE_SCOPE("ModelMaterialComponent::UpdateLoadState");
    // モデルの非同期ロードやComponentの追加順に依存せず、描画前にも準備できる。
    ID3D11Device* device_ = GraphicsDeviceManager::Instance().GetDevice();
    for (auto& entry_ : materials)
    {
        entry_.second.LoadPendingTextures(device_);
    }
    for (auto& entry_ : meshMaterials)
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

ModelMaterial& ModelMaterialComponent::GetMeshMaterial(int meshIndex_)
{
    return meshMaterials[meshIndex_ < 0 ? 0 : meshIndex_];
}

void ModelMaterialComponent::RemoveMeshMaterial(int meshIndex_)
{
    meshMaterials.erase(meshIndex_);
}

ID3D11ShaderResourceView* ModelMaterialComponent::ResolveTexture(
    int materialIndex_, ModelMaterial::TextureSlot slot_,
    ID3D11ShaderResourceView* originalTexture_, int meshIndex_) const
{
    const ModelMaterial* material_ = FindTextureOverride(materialIndex_, slot_, meshIndex_);
    return material_ ? material_->GetTexture(slot_) : originalTexture_;
}

ModelMaterial::TextureChannel ModelMaterialComponent::ResolveChannel(
    int materialIndex_, ModelMaterial::MaterialValue value_, int meshIndex_) const
{
    const ModelMaterial* material_ = FindTextureOverride(
        materialIndex_, ModelMaterial::GetValueTextureSlot(value_), meshIndex_);
    return material_ ? material_->GetChannel(value_) : ModelMaterial::GetDefaultChannel(value_);
}

const ModelMaterial* ModelMaterialComponent::FindTextureOverride(
    int materialIndex_, ModelMaterial::TextureSlot slot_, int meshIndex_) const
{
    if (!IsEnabled()) return nullptr;
    // パーツで未設定またはロード失敗のスロットは、既存の共通設定へ引き継ぐ。
    const auto meshMaterial_ = meshMaterials.find(meshIndex_);
    if (meshMaterial_ != meshMaterials.end())
    {
        if (meshMaterial_->second.GetTexture(slot_)) return &meshMaterial_->second;
    }
    const auto material_ = materials.find(materialIndex_);
    if (material_ != materials.end())
    {
        if (material_->second.GetTexture(slot_)) return &material_->second;
    }
    const auto fallback_ = materials.find(-1);
    if (fallback_ != materials.end())
    {
        if (fallback_->second.GetTexture(slot_)) return &fallback_->second;
    }
    return nullptr;
}

void ModelMaterialComponent::Gui()
{
    BS_PROFILE_SCOPE("ModelMaterialComponent::Gui");
    UpdateLoadState();
    ImGui::TextUnformatted("Priority: part > target material > all materials > original model.");
    const auto owner_ = GetGameObject();
    const auto model_ = owner_ ? owner_->GetComponent<GltfModelCommponennt>() : nullptr;

    // ノード名とメッシュ番号を併記し、同じマテリアルを使うパーツを区別する。
    const auto getPartLabel_ = [&](int meshIndex_)
    {
        std::string label_ = "Mesh " + std::to_string(meshIndex_);
        if (!model_ || meshIndex_ < 0 || meshIndex_ >= static_cast<int>(model_->GetMeshes().size()))
            return label_ + ": Saved Override";
        const auto& mesh_ = model_->GetMeshes()[meshIndex_];
        const auto& nodes_ = model_->GetNodes();
        if (mesh_.nodeIndex >= 0 && mesh_.nodeIndex < static_cast<int>(nodes_.size()))
            label_ += ": " + nodes_[mesh_.nodeIndex].name;
        label_ += " (Material " + std::to_string(mesh_.materialIndex);
        const auto& modelMaterials_ = model_->GetMaterials();
        if (mesh_.materialIndex >= 0 && mesh_.materialIndex < static_cast<int>(modelMaterials_.size()))
            label_ += ": " + modelMaterials_[mesh_.materialIndex].name;
        return label_ + ")";
    };
    const std::string partPreview_ = selectedMeshIndex < 0 ? "Material / All Parts" : getPartLabel_(selectedMeshIndex);
    if (ImGui::BeginCombo("Target Part", partPreview_.c_str()))
    {
        if (ImGui::Selectable("Material / All Parts", selectedMeshIndex == -1)) selectedMeshIndex = -1;
        if (model_)
        {
            for (int index_ = 0; index_ < static_cast<int>(model_->GetMeshes().size()); ++index_)
            {
                const std::string label_ = getPartLabel_(index_);
                if (ImGui::Selectable(label_.c_str(), selectedMeshIndex == index_)) selectedMeshIndex = index_;
            }
        }
        // 非同期ロード前でも保存済みのパーツ設定を編集できるようにする。
        for (const auto& entry_ : meshMaterials)
        {
            if (model_ && entry_.first < static_cast<int>(model_->GetMeshes().size())) continue;
            const std::string label_ = getPartLabel_(entry_.first);
            if (ImGui::Selectable(label_.c_str(), selectedMeshIndex == entry_.first)) selectedMeshIndex = entry_.first;
        }
        ImGui::EndCombo();
    }
    ImGui::BeginDisabled(selectedMeshIndex >= 0);
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
    ImGui::EndDisabled();
    if (!model_ || !model_->IsModelReady()) ImGui::TextDisabled("Owner model is not ready.");

    ModelMaterial& material_ = selectedMeshIndex >= 0
        ? GetMeshMaterial(selectedMeshIndex) : GetMaterial(selectedMaterialIndex);
    // 単独マップは元モデルの係数に影響されず、各物性の値として直接使用する。
    ImGui::TextDisabled("Separate maps use the selected channel directly; unset maps use the packed map.");
    ImGui::TextDisabled("Base Color / Emissive: sRGB. Normal / AO / Metallic / Roughness: Linear.");
    ImGui::TextDisabled("Channel settings apply to the assigned override texture. A without alpha usually reads 1.");
    // 結合マップと単独マップの各物性に、参照する成分を割り当てる。
    const auto drawChannel_ = [&](const char* label_, ModelMaterial::MaterialValue value_)
    {
        int channel_ = static_cast<int>(material_.GetChannel(value_));
        if (ImGui::Combo(label_, &channel_, "R\0G\0B\0A\0"))
            material_.SetChannel(value_, static_cast<ModelMaterial::TextureChannel>(channel_));
    };
    for (std::size_t index_ = 0; index_ < ModelMaterial::TEXTURE_SLOT_COUNT; ++index_)
    {
        const auto slot_ = static_cast<ModelMaterial::TextureSlot>(index_);
        std::string path_ = material_.GetTexturePath(slot_);
        if (AssetSlotGui::Draw(TEXTURE_LABELS[index_], AssetSlotType::TEXTURE,
            path_, material_.GetTexture(slot_), "Data"))
        {
            material_.SetTexture(slot_, path_);
        }
        if (slot_ == ModelMaterial::TextureSlot::METALNESS_ROUGHNESS)
        {
            drawChannel_("Packed Metallic Channel", ModelMaterial::MaterialValue::PACKED_METALLIC);
            drawChannel_("Packed Roughness Channel", ModelMaterial::MaterialValue::PACKED_ROUGHNESS);
        }
        else if (slot_ == ModelMaterial::TextureSlot::METALLIC)
            drawChannel_("Metallic Channel", ModelMaterial::MaterialValue::METALLIC);
        else if (slot_ == ModelMaterial::TextureSlot::ROUGHNESS)
            drawChannel_("Roughness Channel", ModelMaterial::MaterialValue::ROUGHNESS);
        if (material_.HasTextureFailure(slot_))
        {
            ImGui::TextDisabled("Texture load failed. Using fallback texture.");
        }
    }
    if (ImGui::Button("Reload Textures")) material_.ReloadTextures();
    ImGui::SameLine();
    if (ImGui::Button("Clear Material Override"))
    {
        if (selectedMeshIndex >= 0) RemoveMeshMaterial(selectedMeshIndex);
        else RemoveMaterial(selectedMaterialIndex);
    }
    if (ImGui::Button("Reload All Overrides"))
    {
        for (auto& entry_ : materials) entry_.second.ReloadTextures();
        for (auto& entry_ : meshMaterials) entry_.second.ReloadTextures();
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear All Overrides"))
    {
        materials.clear();
        meshMaterials.clear();
    }
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
    json_["MeshMaterials"] = nlohmann::json::array();
    for (const auto& entry_ : meshMaterials)
    {
        nlohmann::json materialJson_;
        materialJson_["MeshIndex"] = entry_.first;
        entry_.second.Serialize(materialJson_["Textures"]);
        json_["MeshMaterials"].push_back(materialJson_);
    }
}

void ModelMaterialComponent::Deserialize(const nlohmann::json& json_)
{
    BS_PROFILE_SCOPE("ModelMaterialComponent::Deserialize");
    materials.clear();
    meshMaterials.clear();
    selectedMaterialIndex = -1;
    selectedMeshIndex = -1;
    if (json_.contains("Materials") && json_["Materials"].is_array())
    {
        for (const auto& materialJson_ : json_["Materials"])
        {
            if (!materialJson_.is_object()) continue;
            const int materialIndex_ = materialJson_.value("MaterialIndex", -1);
            if (materialIndex_ < -1 || !materialJson_.contains("Textures") ||
                !materialJson_["Textures"].is_object()) continue;
            GetMaterial(materialIndex_).Deserialize(materialJson_["Textures"]);
        }
    }
    // 旧JSONではパーツ設定を空にし、従来のマテリアル番号別の表示を維持する。
    if (json_.contains("MeshMaterials") && json_["MeshMaterials"].is_array())
    {
        for (const auto& materialJson_ : json_["MeshMaterials"])
        {
            if (!materialJson_.is_object()) continue;
            const int meshIndex_ = materialJson_.value("MeshIndex", -1);
            if (meshIndex_ < 0 || !materialJson_.contains("Textures") ||
                !materialJson_["Textures"].is_object()) continue;
            GetMeshMaterial(meshIndex_).Deserialize(materialJson_["Textures"]);
        }
    }
}

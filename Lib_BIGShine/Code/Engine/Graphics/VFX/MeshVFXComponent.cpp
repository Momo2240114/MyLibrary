#include "MeshVFXComponent.h"

#include "Editor/AssetBrowserPayload.h"
#include "Engine/Graphics/Core/CPUResourceLoader.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Graphics/Resource/ModelResource.h"
#include "Engine/Graphics/Resource/ModelResourceManager.h"
#include "Engine/Graphics/Resource/TextureResourceManager.h"
#include "Engine/Platform/WindowsDialogHelper.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <nlohmann/json.hpp>

namespace
{
    bool HasExtension(const std::filesystem::path& path_, const std::vector<std::string>& extensions_)
    {
        std::string extension_ = path_.extension().string();
        std::transform(extension_.begin(), extension_.end(), extension_.begin(),
            [](unsigned char character_) { return static_cast<char>(std::tolower(character_)); });
        return std::find(extensions_.begin(), extensions_.end(), extension_) != extensions_.end();
    }

    std::string GetAssetLabel(const std::string& path_)
    {
        const std::filesystem::path assetPath_ = WindowsDialogHelper::Utf8ToPath(path_);
        return WindowsDialogHelper::PathToUtf8(assetPath_.filename()) + "  [" + path_ + "]";
    }

    template<typename T>
    void ReadFloatArray(const nlohmann::json& json_, const char* key_, T& value_, size_t count_)
    {
        if (!json_.contains(key_) || !json_[key_].is_array()) return;
        float* values_ = reinterpret_cast<float*>(&value_);
        const auto& array_ = json_[key_];
        for (size_t index_ = 0; index_ < count_ && index_ < array_.size(); ++index_)
        {
            values_[index_] = array_[index_].get<float>();
        }
    }
}

void MeshVFXComponent::Start()
{
    RefreshAssetLists();
    LoadPartResources();
}

void MeshVFXComponent::Update(float deltaTime_)
{
    if (isPlaying) time += deltaTime_;
}


void MeshVFXComponent::Gui()
{
    ImGui::SeparatorText("Mesh VFX");
    if (ImGui::Button(isPlaying ? "Stop" : "Play")) isPlaying ? Stop() : Play();
    ImGui::SameLine();
    if (ImGui::Button("Reset")) Reset();
    ImGui::SameLine();
    if (ImGui::Button("Refresh Assets")) RefreshAssetLists();
    ImGui::Text("Time: %.2f", time);

    if (ImGui::Button("Add Part"))
    {
        asset.parts.emplace_back();
        runtimeParts.emplace_back();
    }

    for (int partIndex_ = 0; partIndex_ < static_cast<int>(asset.parts.size()); ++partIndex_)
    {
        ImGui::PushID(partIndex_);
        MeshVFXPart& part_ = asset.parts[partIndex_];
        bool removePart_ = false;

        if (ImGui::TreeNode("Part"))
        {
            ImGui::Checkbox("Render Part", &part_.renderEnabled);
            if (!part_.renderEnabled)
            {
                ImGui::SameLine();
                ImGui::TextDisabled("Model information only");
            }

         
            // Draw a separate native file dialog button.
            auto drawFileDialogButton_ = [&](
                const char* buttonLabel_,
                std::string& path_,
                bool isTexture_)
                {
                    bool changed_ = false;

                    if (ImGui::Button(buttonLabel_, ImVec2(-1, 0)))
                    {
                        namespace fs = std::filesystem;

                        fs::path initialPath_;

                        if (path_.empty())
                        {
                            initialPath_ = isTexture_ ? fs::path("Data/Texture") : fs::path("Data/Model");
                        }
                        else
                        {
                            initialPath_ = WindowsDialogHelper::Utf8ToPath(path_);

                            if (fs::is_regular_file(initialPath_))
                            {
                                initialPath_ = initialPath_.parent_path();
                            }
                        }

                        std::error_code error_;
                        initialPath_ = fs::absolute(initialPath_, error_);

                        if (error_ || !fs::exists(initialPath_))
                        {
                            initialPath_ = fs::current_path();
                        }

                        const std::string initialDirectory_ =
                            WindowsDialogHelper::PathToUtf8(initialPath_);

                        const HWND hwnd_ =
                            GraphicsDeviceManager::Instance().GetWindowHandle();

                        const std::string selectedPath_ = isTexture_
                            ? WindowsDialogHelper::OpenTextureDialog(
                                L"Select Mesh VFX Texture",
                                initialDirectory_,
                                hwnd_)
                            : WindowsDialogHelper::OpenGltfDialog(
                                L"Select Mesh VFX Model",
                                initialDirectory_,
                                hwnd_);

                        if (!selectedPath_.empty())
                        {
                            path_ = selectedPath_;
                            changed_ = true;
                            RefreshAssetLists();
                        }
                    }

                    return changed_;
                };
            auto drawTextureAssetSlot_ = [](const char* label_, std::string& path_)
                {
                    bool changed_ = false;
                    ImGui::PushID(label_);
                    ImGui::TextUnformatted(label_);
                    ID3D11ShaderResourceView* texture_ =
                        TextureResourceManager::Instance().GetTexture(path_);
                    if (texture_)
                    {
                        ImGui::Image(reinterpret_cast<void*>(texture_), ImVec2(64.0f, 64.0f));
                    }
                    else
                    {
                        ImGui::Button(path_.empty() ? "No Texture" : "Drop Texture", ImVec2(64.0f, 64.0f));
                    }
                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload* payload_ =
                            ImGui::AcceptDragDropPayload(AssetBrowserPayload::TEXTURE))
                        {
                            path_ = static_cast<const char*>(payload_->Data);
                            changed_ = true;
                        }
                        ImGui::EndDragDropTarget();
                    }
                    ImGui::SameLine();
                    ImGui::BeginGroup();
                    ImGui::TextWrapped("%s", path_.empty() ? "Not selected" : path_.c_str());
                    if (ImGui::Button("Clear"))
                    {
                        path_.clear();
                        changed_ = true;
                    }
                    ImGui::EndGroup();
                    ImGui::PopID();
                    return changed_;
                };

            // Model asset selection
            //bool modelChanged_ = drawAssetCombo_("Model Asset", part_.modelPath, modelAssetPaths, false);
            bool modelChanged_ = false;
            ImGui::Spacing();
            if (drawFileDialogButton_("Browse Model via Dialog...", part_.modelPath, false))
            {
                modelChanged_ = true;
            }

            if (modelChanged_)
            {
                LoadPartResource(static_cast<size_t>(partIndex_));
                const MeshVFXRuntimePart& loadedPart_ = runtimeParts[partIndex_];
                part_.meshIndex = loadedPart_.modelResource && !loadedPart_.modelResource->meshes.empty()
                    ? 0
                    : -1;
            }

            const MeshVFXRuntimePart* runtimePart_ =
                partIndex_ < static_cast<int>(runtimeParts.size()) ? &runtimeParts[partIndex_] : nullptr;
            const std::shared_ptr<ModelResource> modelResource_ =
                runtimePart_ ? runtimePart_->modelResource : nullptr;

            if (modelResource_)
            {
                ImGui::Text(
                    "Loaded: %s | Meshes: %zu | Nodes: %zu | Materials: %zu",
                    modelResource_->modelName.c_str(),
                    modelResource_->meshes.size(),
                    modelResource_->nodes.size(),
                    modelResource_->materials.size());

                std::string meshPreview_ = "All Meshes";
                if (part_.meshIndex >= 0 &&
                    part_.meshIndex < static_cast<int>(modelResource_->meshes.size()))
                {
                    const auto& mesh_ = modelResource_->meshes[part_.meshIndex];
                    const std::string nodeName_ = mesh_.node ? mesh_.node->name : "No Node";
                    const std::string materialName_ = mesh_.material ? mesh_.material->name : "No Material";
                    meshPreview_ = std::to_string(part_.meshIndex) + ": " + nodeName_ + " / " + materialName_;
                }

                if (ImGui::BeginCombo("Mesh", meshPreview_.c_str()))
                {
                    if (ImGui::Selectable("All Meshes", part_.meshIndex == -1)) part_.meshIndex = -1;
                    for (int meshIndex_ = 0; meshIndex_ < static_cast<int>(modelResource_->meshes.size()); ++meshIndex_)
                    {
                        const auto& mesh_ = modelResource_->meshes[meshIndex_];
                        const std::string nodeName_ = mesh_.node ? mesh_.node->name : "No Node";
                        const std::string materialName_ = mesh_.material ? mesh_.material->name : "No Material";
                        const std::string meshLabel_ =
                            std::to_string(meshIndex_) + ": " + nodeName_ + " / " + materialName_;
                        if (ImGui::Selectable(meshLabel_.c_str(), part_.meshIndex == meshIndex_))
                        {
                            part_.meshIndex = meshIndex_;
                        }
                    }
                    ImGui::EndCombo();
                }

                if (part_.meshIndex >= 0 &&
                    part_.meshIndex < static_cast<int>(modelResource_->meshes.size()))
                {
                    const auto& selectedMesh_ = modelResource_->meshes[part_.meshIndex];
                    ImGui::Text(
                        "Vertices: %zu | Indices: %zu | Bones: %zu",
                        selectedMesh_.vertices.size(),
                        selectedMesh_.indices.size(),
                        selectedMesh_.bones.size());
                }
            }
            else
            {
                ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.2f, 1.0f), "Model resource is not loaded.");
            }

            ImGui::SeparatorText("Textures");
            // Base texture selection
            bool baseTextureChanged_= false;
            if (drawFileDialogButton_("Browse Base Texture via Dialog...", part_.baseTexturePath, true))
            {
                baseTextureChanged_ = true;
            }
            baseTextureChanged_ |= drawTextureAssetSlot_("Base Texture Asset", part_.baseTexturePath);

            ImGui::Checkbox("Use Material Base Texture", &part_.useMaterialBaseTexture);

            // Normal texture selection
            bool normalTextureChanged_ = false;
            if (drawFileDialogButton_("Browse Normal Texture via Dialog...", part_.normalTexturePath, true))
            {
                normalTextureChanged_ = true;
            }
            normalTextureChanged_ |= drawTextureAssetSlot_("Normal Texture Asset", part_.normalTexturePath);

            ImGui::Checkbox("Use Material Normal Texture", &part_.useMaterialNormalTexture);

            bool layeredTextureChanged_ = false;
            if (ImGui::TreeNode("Mask Texture Layers"))
            {
                if (ImGui::Button("Add Mask Layer"))
                {
                    part_.maskLayers.emplace_back();
                    layeredTextureChanged_ = true;
                }
                int removeLayerIndex_ = -1;
                for (int layerIndex_ = 0; layerIndex_ < static_cast<int>(part_.maskLayers.size()); ++layerIndex_)
                {
                    ImGui::PushID(layerIndex_);
                    MeshVFXMaskLayer& layer_ = part_.maskLayers[layerIndex_];
                    const std::string buttonLabel_ = layer_.path.empty()
                        ? "Select Mask Texture..."
                        : std::filesystem::path(layer_.path).filename().string();
                    layeredTextureChanged_ |= drawFileDialogButton_(buttonLabel_.c_str(), layer_.path, true);
                    layeredTextureChanged_ |= drawTextureAssetSlot_("Mask Texture Asset", layer_.path);
                    const char* blendItems_[] = { "Multiply / Narrow", "Union / Expand" };
                    int blendMode_ = static_cast<int>(layer_.blendMode);
                    if (ImGui::Combo("Blend", &blendMode_, blendItems_, IM_ARRAYSIZE(blendItems_)))
                    {
                        layer_.blendMode = static_cast<MeshVFXMaskBlendMode>(blendMode_);
                    }
                    ImGui::Checkbox("Invert", &layer_.invert);
                    if (ImGui::Button("Remove Mask Layer")) removeLayerIndex_ = layerIndex_;
                    ImGui::Separator();
                    ImGui::PopID();
                }
                if (removeLayerIndex_ >= 0)
                {
                    part_.maskLayers.erase(part_.maskLayers.begin() + removeLayerIndex_);
                    layeredTextureChanged_ = true;
                }
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Noise Texture Layers"))
            {
                if (ImGui::Button("Add Noise Layer"))
                {
                    part_.noiseTexturePaths.emplace_back();
                    layeredTextureChanged_ = true;
                }
                int removeLayerIndex_ = -1;
                for (int layerIndex_ = 0; layerIndex_ < static_cast<int>(part_.noiseTexturePaths.size()); ++layerIndex_)
                {
                    ImGui::PushID(layerIndex_);
                    std::string& path_ = part_.noiseTexturePaths[layerIndex_];
                    const std::string buttonLabel_ = path_.empty()
                        ? "Select Noise Texture..."
                        : std::filesystem::path(path_).filename().string();
                    layeredTextureChanged_ |= drawFileDialogButton_(buttonLabel_.c_str(), path_, true);
                    layeredTextureChanged_ |= drawTextureAssetSlot_("Noise Texture Asset", path_);
                    if (ImGui::Button("Remove Noise Layer")) removeLayerIndex_ = layerIndex_;
                    ImGui::Separator();
                    ImGui::PopID();
                }
                if (removeLayerIndex_ >= 0)
                {
                    part_.noiseTexturePaths.erase(part_.noiseTexturePaths.begin() + removeLayerIndex_);
                    layeredTextureChanged_ = true;
                }
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Distortion Noise Texture"))
            {
                const std::string buttonLabel_ = part_.distortionNoiseTexturePath.empty()
                    ? "Select Distortion Noise Texture..."
                    : std::filesystem::path(part_.distortionNoiseTexturePath).filename().string();
                layeredTextureChanged_ |= drawFileDialogButton_(
                    buttonLabel_.c_str(),
                    part_.distortionNoiseTexturePath,
                    true);
                layeredTextureChanged_ |= drawTextureAssetSlot_(
                    "Distortion Noise Texture Asset",
                    part_.distortionNoiseTexturePath);
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Vertex Deformation Noise Texture"))
            {
                const std::string buttonLabel_ = part_.vertexNoiseTexturePath.empty()
                    ? "Select Vertex Noise Texture..."
                    : std::filesystem::path(part_.vertexNoiseTexturePath).filename().string();
                layeredTextureChanged_ |= drawFileDialogButton_(
                    buttonLabel_.c_str(),
                    part_.vertexNoiseTexturePath,
                    true);
                layeredTextureChanged_ |= drawTextureAssetSlot_(
                    "Vertex Noise Texture Asset",
                    part_.vertexNoiseTexturePath);
                ImGui::TextDisabled("Used only for vertex displacement. Empty uses procedural noise.");
                ImGui::TreePop();
            }

            if (baseTextureChanged_ || normalTextureChanged_ || layeredTextureChanged_)
            {
                LoadPartResource(static_cast<size_t>(partIndex_));
            }

            ImGui::DragFloat3("Position", &part_.position.x, 0.01f);
            ImGui::DragFloat3("Rotation", &part_.rotation.x, 0.5f);
            ImGui::DragFloat3("Scale", &part_.scale.x, 0.01f);

            if (ImGui::TreeNode("Animation"))
            {
                ImGui::DragFloat("Duration", &part_.duration, 0.05f, 0.0f, 600.0f);
                ImGui::Checkbox("Loop", &part_.loop);
                ImGui::DragFloat3("Mesh Rotation Speed", &part_.meshRotationSpeed.x, 1.0f);

                ImGui::Checkbox("Animate Scale", &part_.animateScale);
                if (part_.animateScale)
                {
                    ImGui::DragFloat3("Scale Start", &part_.scaleStart.x, 0.01f);
                    ImGui::DragFloat3("Scale End", &part_.scaleEnd.x, 0.01f);
                }

                ImGui::Checkbox("Animate Alpha", &part_.animateAlpha);
                if (part_.animateAlpha)
                {
                    ImGui::DragFloat("Alpha Start", &part_.alphaStart, 0.01f, 0.0f, 1.0f);
                    ImGui::DragFloat("Alpha End", &part_.alphaEnd, 0.01f, 0.0f, 1.0f);
                }

                ImGui::DragFloat2("UV Rotation Center", &part_.uvRotationCenter.x, 0.01f);
                ImGui::DragFloat("Base UV Rotation Speed", &part_.baseUVRotationSpeed, 1.0f);
                ImGui::DragFloat("Mask UV Rotation Speed", &part_.maskUVRotationSpeed, 1.0f);
                ImGui::DragFloat("Noise UV Rotation Speed", &part_.noiseUVRotationSpeed, 1.0f);
                ImGui::SeparatorText("Radial UV Animation");
                ImGui::TextDisabled("Positive speed expands the Base / Mask texture from the UV center.");
                ImGui::DragFloat("UV Radial Scale", &part_.uvRadialScale, 0.01f, 0.001f, 100.0f);
                ImGui::DragFloat("UV Radial Scale Speed", &part_.uvRadialScaleSpeed, 0.01f, -10.0f, 10.0f);
                ImGui::TreePop();
            }

            ImGui::ColorEdit4("Color", &part_.color.x);
            ImGui::DragFloat2("Base UV Speed", &part_.baseUVSpeed.x, 0.01f);
            ImGui::DragFloat2("Mask UV Speed", &part_.maskUVSpeed.x, 0.01f);
            ImGui::DragFloat2("Noise UV Speed", &part_.noiseUVSpeed.x, 0.01f);
            ImGui::DragFloat("Alpha", &part_.alpha, 0.01f, 0.0f, 1.0f);
            ImGui::DragFloat("Normal Strength", &part_.normalStrength, 0.01f, 0.0f, 10.0f);
            ImGui::DragFloat("Noise Strength", &part_.noiseStrength, 0.001f, 0.0f, 1.0f);
            if (ImGui::TreeNode("Vertex Noise Deformation"))
            {
                ImGui::Checkbox("Use Vertex Noise", &part_.useVertexNoise);
                ImGui::TextDisabled("Displaces vertices along their normals. Dense meshes produce smoother results.");
                ImGui::DragFloat(
                    "Vertex Noise Horizontal XZ Strength",
                    &part_.vertexNoiseHorizontalStrength,
                    0.001f,
                    0.0f,
                    100.0f);
                ImGui::DragFloat(
                    "Vertex Noise Vertical Y Strength",
                    &part_.vertexNoiseVerticalStrength,
                    0.001f,
                    0.0f,
                    100.0f);
                ImGui::DragFloat(
                    "Vertex Noise Horizontal XZ Scale",
                    &part_.vertexNoiseHorizontalScale,
                    0.01f,
                    0.001f,
                    100.0f);
                ImGui::DragFloat(
                    "Vertex Noise Vertical Y Scale",
                    &part_.vertexNoiseVerticalScale,
                    0.01f,
                    0.001f,
                    100.0f);
                ImGui::DragFloat3("Vertex Noise Speed", &part_.vertexNoiseSpeed.x, 0.01f);
                ImGui::TreePop();
            }
            ImGui::DragFloat("Fresnel Power", &part_.fresnelPower, 0.01f, 0.1f, 10.0f);
            ImGui::DragFloat("Fresnel Strength", &part_.fresnelStrength, 0.01f, 0.0f, 20.0f);
            ImGui::DragFloat("Dissolve Threshold", &part_.dissolveThreshold, 0.01f, 0.0f, 1.0f);
            ImGui::Checkbox("Use Mask", &part_.useMask);
            ImGui::Checkbox("Use Noise", &part_.useNoise);
            ImGui::Checkbox("Use Normal", &part_.useNormal);
            ImGui::Checkbox("Use Fresnel", &part_.useFresnel);

            if (ImGui::TreeNode("Masked Area Distortion"))
            {
                ImGui::Checkbox("Use Mask Distortion", &part_.useMaskDistortion);
                ImGui::TextDisabled("Distorts the background where the mask becomes transparent. Uses Distortion Noise Resource.");
                ImGui::DragFloat(
                    "Mask Distortion Strength",
                    &part_.maskDistortionStrength,
                    0.1f,
                    0.0f,
                    100.0f,
                    "%.1f px");
                ImGui::DragFloat(
                    "Mask Distortion Opacity",
                    &part_.maskDistortionOpacity,
                    0.01f,
                    0.0f,
                    1.0f);
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Edge Highlight"))
            {
                ImGui::Checkbox("Use Edge Highlight", &part_.useEdgeHighlight);
                ImGui::ColorEdit4("Edge Color", &part_.edgeColor.x);
                ImGui::DragFloat(
                    "Silhouette Edge Strength",
                    &part_.silhouetteEdgeStrength,
                    0.01f,
                    0.0f,
                    10.0f);
                ImGui::DragFloat(
                    "Silhouette Edge Power",
                    &part_.silhouetteEdgePower,
                    0.05f,
                    0.1f,
                    20.0f);
                ImGui::DragFloat(
                    "Intersection Edge Strength",
                    &part_.intersectionEdgeStrength,
                    0.01f,
                    0.0f,
                    10.0f);
                ImGui::DragFloat(
                    "Intersection Edge Width",
                    &part_.intersectionEdgeWidth,
                    0.0001f,
                    0.0001f,
                    0.1f,
                    "%.4f");
                ImGui::TreePop();
            }

            const char* blendItems_[] = { "Alpha", "Additive" };
            int blendMode_ = static_cast<int>(part_.blendMode);
            if (ImGui::Combo("Blend Mode", &blendMode_, blendItems_, IM_ARRAYSIZE(blendItems_)))
            {
                part_.blendMode = static_cast<MeshVFXBlendMode>(blendMode_);
            }

            if (ImGui::Button("Reload Part Resource"))
            {
                LoadPartResource(static_cast<size_t>(partIndex_));
            }
            ImGui::SameLine();
            removePart_ = ImGui::Button("Remove Part");
            ImGui::TreePop();
        }
        ImGui::PopID();

        if (removePart_)
        {
            asset.parts.erase(asset.parts.begin() + partIndex_);
            runtimeParts.erase(runtimeParts.begin() + partIndex_);
            break;
        }
    }

    if (ImGui::Button("Reload All Resources")) LoadPartResources();
}

void MeshVFXComponent::Play() { isPlaying = true; }
void MeshVFXComponent::Stop() { isPlaying = false; }
void MeshVFXComponent::Reset() { time = 0.0f; }

void MeshVFXComponent::RefreshAssetLists()
{
    modelAssetPaths.clear();
    textureAssetPaths.clear();

    auto collectAssets_ = [](const std::filesystem::path& rootPath_,
        const std::vector<std::string>& extensions_,
        std::vector<std::string>& assetPaths_)
        {
            std::error_code error_;
            if (!std::filesystem::is_directory(rootPath_, error_)) return;

            for (const auto& entry_ : std::filesystem::recursive_directory_iterator(rootPath_, error_))
            {
                if (error_) break;
                if (!entry_.is_regular_file(error_) || !HasExtension(entry_.path(), extensions_)) continue;
                assetPaths_.push_back(WindowsDialogHelper::PathToUtf8(entry_.path().lexically_normal()));
            }
            std::sort(assetPaths_.begin(), assetPaths_.end());
        };

    collectAssets_("Data/Model", { ".gltf", ".glb" }, modelAssetPaths);
    collectAssets_("Data/Texture", { ".png", ".jpg", ".jpeg", ".bmp", ".tga", ".dds", ".hdr" }, textureAssetPaths);
}

void MeshVFXComponent::Serialize(nlohmann::json& json_) const
{
    json_["IsPlaying"] = isPlaying;
    json_["Parts"] = nlohmann::json::array();
    for (const MeshVFXPart& part_ : asset.parts)
    {
        nlohmann::json partJson_;
        partJson_["ModelPath"] = part_.modelPath;
        partJson_["MeshIndex"] = part_.meshIndex;
        partJson_["RenderEnabled"] = part_.renderEnabled;
        partJson_["UseMaterialBaseTexture"] = part_.useMaterialBaseTexture;
        partJson_["UseMaterialNormalTexture"] = part_.useMaterialNormalTexture;
        partJson_["Position"] = { part_.position.x, part_.position.y, part_.position.z };
        partJson_["Rotation"] = { part_.rotation.x, part_.rotation.y, part_.rotation.z };
        partJson_["Scale"] = { part_.scale.x, part_.scale.y, part_.scale.z };
        partJson_["MeshRotationSpeed"] =
        {
            part_.meshRotationSpeed.x,
            part_.meshRotationSpeed.y,
            part_.meshRotationSpeed.z,
        };
        partJson_["Duration"] = part_.duration;
        partJson_["Loop"] = part_.loop;
        partJson_["AnimateScale"] = part_.animateScale;
        partJson_["ScaleStart"] = { part_.scaleStart.x, part_.scaleStart.y, part_.scaleStart.z };
        partJson_["ScaleEnd"] = { part_.scaleEnd.x, part_.scaleEnd.y, part_.scaleEnd.z };
        partJson_["AnimateAlpha"] = part_.animateAlpha;
        partJson_["AlphaStart"] = part_.alphaStart;
        partJson_["AlphaEnd"] = part_.alphaEnd;
        partJson_["BaseTexturePath"] = part_.baseTexturePath;
        partJson_["NormalTexturePath"] = part_.normalTexturePath;
        partJson_["MaskTexturePath"] = part_.maskTexturePath;
        partJson_["NoiseTexturePath"] = part_.noiseTexturePath;
        partJson_["DistortionNoiseTexturePath"] = part_.distortionNoiseTexturePath;
        partJson_["VertexNoiseTexturePath"] = part_.vertexNoiseTexturePath;
        partJson_["MaskLayers"] = nlohmann::json::array();
        for (const MeshVFXMaskLayer& layer_ : part_.maskLayers)
        {
            partJson_["MaskLayers"].push_back(
                {
                    { "Path", layer_.path },
                    { "BlendMode", static_cast<int>(layer_.blendMode) },
                    { "Invert", layer_.invert },
                });
        }
        partJson_["NoiseTexturePaths"] = part_.noiseTexturePaths;
        partJson_["Color"] = { part_.color.x, part_.color.y, part_.color.z, part_.color.w };
        partJson_["BaseUVSpeed"] = { part_.baseUVSpeed.x, part_.baseUVSpeed.y };
        partJson_["MaskUVSpeed"] = { part_.maskUVSpeed.x, part_.maskUVSpeed.y };
        partJson_["NoiseUVSpeed"] = { part_.noiseUVSpeed.x, part_.noiseUVSpeed.y };
        partJson_["UVRotationCenter"] = { part_.uvRotationCenter.x, part_.uvRotationCenter.y };
        partJson_["BaseUVRotationSpeed"] = part_.baseUVRotationSpeed;
        partJson_["MaskUVRotationSpeed"] = part_.maskUVRotationSpeed;
        partJson_["NoiseUVRotationSpeed"] = part_.noiseUVRotationSpeed;
        partJson_["UVRadialScale"] = part_.uvRadialScale;
        partJson_["UVRadialScaleSpeed"] = part_.uvRadialScaleSpeed;
        partJson_["Alpha"] = part_.alpha;
        partJson_["NormalStrength"] = part_.normalStrength;
        partJson_["NoiseStrength"] = part_.noiseStrength;
        partJson_["VertexNoiseHorizontalStrength"] = part_.vertexNoiseHorizontalStrength;
        partJson_["VertexNoiseVerticalStrength"] = part_.vertexNoiseVerticalStrength;
        partJson_["VertexNoiseHorizontalScale"] = part_.vertexNoiseHorizontalScale;
        partJson_["VertexNoiseVerticalScale"] = part_.vertexNoiseVerticalScale;
        partJson_["VertexNoiseSpeed"] =
        {
            part_.vertexNoiseSpeed.x,
            part_.vertexNoiseSpeed.y,
            part_.vertexNoiseSpeed.z,
        };
        partJson_["MaskDistortionStrength"] = part_.maskDistortionStrength;
        partJson_["MaskDistortionOpacity"] = part_.maskDistortionOpacity;
        partJson_["EdgeColor"] =
        {
            part_.edgeColor.x,
            part_.edgeColor.y,
            part_.edgeColor.z,
            part_.edgeColor.w,
        };
        partJson_["SilhouetteEdgeStrength"] = part_.silhouetteEdgeStrength;
        partJson_["SilhouetteEdgePower"] = part_.silhouetteEdgePower;
        partJson_["IntersectionEdgeStrength"] = part_.intersectionEdgeStrength;
        partJson_["IntersectionEdgeWidth"] = part_.intersectionEdgeWidth;
        partJson_["FresnelPower"] = part_.fresnelPower;
        partJson_["FresnelStrength"] = part_.fresnelStrength;
        partJson_["DissolveThreshold"] = part_.dissolveThreshold;
        partJson_["UseMask"] = part_.useMask;
        partJson_["UseNoise"] = part_.useNoise;
        partJson_["UseVertexNoise"] = part_.useVertexNoise;
        partJson_["UseMaskDistortion"] = part_.useMaskDistortion;
        partJson_["UseEdgeHighlight"] = part_.useEdgeHighlight;
        partJson_["UseNormal"] = part_.useNormal;
        partJson_["UseFresnel"] = part_.useFresnel;
        partJson_["BlendMode"] = static_cast<int>(part_.blendMode);
        json_["Parts"].push_back(partJson_);
    }
}

void MeshVFXComponent::Deserialize(const nlohmann::json& json_)
{
    asset.parts.clear();
    runtimeParts.clear();
    isPlaying = json_.value("IsPlaying", true);
    time = 0.0f;
    if (!json_.contains("Parts") || !json_["Parts"].is_array()) return;

    for (const auto& partJson_ : json_["Parts"])
    {
        MeshVFXPart part_;
        part_.modelPath = partJson_.value("ModelPath", "");
        part_.meshIndex = partJson_.value("MeshIndex", -1);
        part_.renderEnabled = partJson_.value("RenderEnabled", false);
        part_.useMaterialBaseTexture = partJson_.value("UseMaterialBaseTexture", true);
        part_.useMaterialNormalTexture = partJson_.value("UseMaterialNormalTexture", true);
        ReadFloatArray(partJson_, "Position", part_.position, 3);
        ReadFloatArray(partJson_, "Rotation", part_.rotation, 3);
        ReadFloatArray(partJson_, "Scale", part_.scale, 3);
        ReadFloatArray(partJson_, "MeshRotationSpeed", part_.meshRotationSpeed, 3);
        part_.duration = (std::max)(partJson_.value("Duration", 0.0f), 0.0f);
        part_.loop = partJson_.value("Loop", true);
        part_.animateScale = partJson_.value("AnimateScale", false);
        ReadFloatArray(partJson_, "ScaleStart", part_.scaleStart, 3);
        ReadFloatArray(partJson_, "ScaleEnd", part_.scaleEnd, 3);
        part_.animateAlpha = partJson_.value("AnimateAlpha", false);
        part_.alphaStart = partJson_.value("AlphaStart", 1.0f);
        part_.alphaEnd = partJson_.value("AlphaEnd", 1.0f);
        part_.baseTexturePath = partJson_.value("BaseTexturePath", "");
        part_.normalTexturePath = partJson_.value("NormalTexturePath", "");
        part_.maskTexturePath = partJson_.value("MaskTexturePath", "");
        part_.noiseTexturePath = partJson_.value("NoiseTexturePath", "");
        part_.distortionNoiseTexturePath = partJson_.value("DistortionNoiseTexturePath", "");
        part_.vertexNoiseTexturePath = partJson_.value("VertexNoiseTexturePath", "");
        if (partJson_.contains("MaskLayers") && partJson_["MaskLayers"].is_array())
        {
            for (const auto& layerJson_ : partJson_["MaskLayers"])
            {
                MeshVFXMaskLayer layer_;
                layer_.path = layerJson_.value("Path", "");
                layer_.blendMode = static_cast<MeshVFXMaskBlendMode>(
                    std::clamp(layerJson_.value("BlendMode", 0), 0, 1));
                layer_.invert = layerJson_.value("Invert", false);
                part_.maskLayers.push_back(layer_);
            }
        }
        if (partJson_.contains("NoiseTexturePaths") && partJson_["NoiseTexturePaths"].is_array())
        {
            for (const auto& pathJson_ : partJson_["NoiseTexturePaths"])
            {
                part_.noiseTexturePaths.push_back(pathJson_.get<std::string>());
            }
        }
        if (part_.maskLayers.empty() && !part_.maskTexturePath.empty())
        {
            part_.maskLayers.push_back({ part_.maskTexturePath, MeshVFXMaskBlendMode::MULTIPLY, false });
        }
        if (part_.noiseTexturePaths.empty() && !part_.noiseTexturePath.empty())
        {
            part_.noiseTexturePaths.push_back(part_.noiseTexturePath);
        }
        ReadFloatArray(partJson_, "Color", part_.color, 4);
        ReadFloatArray(partJson_, "BaseUVSpeed", part_.baseUVSpeed, 2);
        ReadFloatArray(partJson_, "MaskUVSpeed", part_.maskUVSpeed, 2);
        ReadFloatArray(partJson_, "NoiseUVSpeed", part_.noiseUVSpeed, 2);
        ReadFloatArray(partJson_, "UVRotationCenter", part_.uvRotationCenter, 2);
        part_.baseUVRotationSpeed = partJson_.value("BaseUVRotationSpeed", 0.0f);
        part_.maskUVRotationSpeed = partJson_.value("MaskUVRotationSpeed", 0.0f);
        part_.noiseUVRotationSpeed = partJson_.value("NoiseUVRotationSpeed", 0.0f);
        part_.uvRadialScale = (std::max)(partJson_.value("UVRadialScale", 1.0f), 0.001f);
        part_.uvRadialScaleSpeed = partJson_.value("UVRadialScaleSpeed", 0.0f);
        part_.alpha = partJson_.value("Alpha", 1.0f);
        part_.normalStrength = partJson_.value("NormalStrength", 1.0f);
        part_.noiseStrength = partJson_.value("NoiseStrength", 0.05f);
        const float legacyVertexNoiseStrength_ =
            (std::max)(partJson_.value("VertexNoiseStrength", 0.1f), 0.0f);
        part_.vertexNoiseHorizontalStrength = (std::max)(
            partJson_.value("VertexNoiseHorizontalStrength", legacyVertexNoiseStrength_),
            0.0f);
        part_.vertexNoiseVerticalStrength = (std::max)(
            partJson_.value("VertexNoiseVerticalStrength", legacyVertexNoiseStrength_),
            0.0f);
        const float legacyVertexNoiseScale_ =
            (std::max)(partJson_.value("VertexNoiseScale", 1.0f), 0.001f);
        part_.vertexNoiseHorizontalScale = (std::max)(
            partJson_.value("VertexNoiseHorizontalScale", legacyVertexNoiseScale_),
            0.001f);
        part_.vertexNoiseVerticalScale = (std::max)(
            partJson_.value("VertexNoiseVerticalScale", legacyVertexNoiseScale_),
            0.001f);
        ReadFloatArray(partJson_, "VertexNoiseSpeed", part_.vertexNoiseSpeed, 3);
        part_.maskDistortionStrength = (std::max)(partJson_.value("MaskDistortionStrength", 8.0f), 0.0f);
        part_.maskDistortionOpacity = std::clamp(partJson_.value("MaskDistortionOpacity", 0.5f), 0.0f, 1.0f);
        ReadFloatArray(partJson_, "EdgeColor", part_.edgeColor, 4);
        part_.silhouetteEdgeStrength = (std::max)(partJson_.value("SilhouetteEdgeStrength", 0.0f), 0.0f);
        part_.silhouetteEdgePower = (std::max)(partJson_.value("SilhouetteEdgePower", 3.0f), 0.1f);
        part_.intersectionEdgeStrength = (std::max)(partJson_.value("IntersectionEdgeStrength", 0.0f), 0.0f);
        part_.intersectionEdgeWidth = (std::max)(partJson_.value("IntersectionEdgeWidth", 0.002f), 0.0001f);
        part_.fresnelPower = partJson_.value("FresnelPower", 2.0f);
        part_.fresnelStrength = partJson_.value("FresnelStrength", 1.0f);
        part_.dissolveThreshold = partJson_.value("DissolveThreshold", 0.0f);
        part_.useMask = partJson_.value("UseMask", false);
        part_.useNoise = partJson_.value("UseNoise", false);
        part_.useVertexNoise = partJson_.value("UseVertexNoise", false);
        part_.useMaskDistortion = partJson_.value("UseMaskDistortion", false);
        part_.useEdgeHighlight = partJson_.value("UseEdgeHighlight", false);
        part_.useNormal = partJson_.value("UseNormal", false);
        part_.useFresnel = partJson_.value("UseFresnel", false);
        part_.blendMode = static_cast<MeshVFXBlendMode>(std::clamp(partJson_.value("BlendMode", 0), 0, 1));
        asset.parts.push_back(part_);
    }
    LoadPartResources();
}

bool MeshVFXComponent::SaveAsset(const std::string& path_) const
{
    if (path_.empty()) return false;
    nlohmann::json json_;
    json_["AssetType"] = "MeshVFX";
    json_["AssetVersion"] = 1;
    Serialize(json_["MeshVFX"]);

    const std::filesystem::path assetPath_ = WindowsDialogHelper::Utf8ToPath(path_);
    std::error_code error_;
    if (assetPath_.has_parent_path())
    {
        std::filesystem::create_directories(assetPath_.parent_path(), error_);
        if (error_) return false;
    }
    std::ofstream file_(assetPath_, std::ios::out | std::ios::trunc);
    if (!file_.is_open()) return false;
    file_ << json_.dump(4);
    return file_.good();
}

bool MeshVFXComponent::LoadAsset(const std::string& path_)
{
    if (path_.empty()) return false;
    const std::filesystem::path assetPath_ = WindowsDialogHelper::Utf8ToPath(path_);
    std::ifstream file_(assetPath_);
    if (!file_.is_open()) return false;

    nlohmann::json json_;
    try
    {
        file_ >> json_;
        const nlohmann::json& meshVFXJson_ =
            json_.contains("MeshVFX") ? json_["MeshVFX"] : json_;
        if (!meshVFXJson_.contains("Parts") || !meshVFXJson_["Parts"].is_array()) return false;
        Deserialize(meshVFXJson_);
    }
    catch (const nlohmann::json::exception&)
    {
        return false;
    }
    return true;
}

void MeshVFXComponent::SetAsset(const MeshVFXAsset& asset_)
{
    asset = asset_;
    time = 0.0f;
    RebuildRuntimeResources();
}

void MeshVFXComponent::RebuildRuntimeResources()
{
    LoadPartResources();
}

void MeshVFXComponent::LoadPartResources()
{
    runtimeParts.resize(asset.parts.size());
    for (size_t partIndex_ = 0; partIndex_ < asset.parts.size(); ++partIndex_)
    {
        LoadPartResource(partIndex_);
    }
}

void MeshVFXComponent::LoadPartResource(size_t partIndex_)
{
    if (partIndex_ >= asset.parts.size()) return;
    if (runtimeParts.size() < asset.parts.size()) runtimeParts.resize(asset.parts.size());

    ID3D11Device* device_ = GraphicsDeviceManager::Instance().GetDevice();
    if (!device_) return;

    MeshVFXPart& part_ = asset.parts[partIndex_];
    MeshVFXRuntimePart& runtimePart_ = runtimeParts[partIndex_];
    runtimePart_.modelResource.reset();
    if (!part_.modelPath.empty())
    {
        runtimePart_.modelResource = ModelResourceManager::Instance().GetOrCreateResource(device_, part_.modelPath);
    }

    LoadTexture(device_, part_.baseTexturePath, 0xFFFFFFFF, runtimePart_.baseTexture);
    LoadTexture(device_, part_.normalTexturePath, 0xFFFF7F7F, runtimePart_.normalTexture);
    runtimePart_.maskTextures.clear();
    runtimePart_.noiseTextures.clear();
    for (const MeshVFXMaskLayer& layer_ : part_.maskLayers)
    {
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture_;
        LoadTexture(device_, layer_.path, 0xFFFFFFFF, texture_);
        runtimePart_.maskTextures.push_back(texture_);
    }
    for (const std::string& path_ : part_.noiseTexturePaths)
    {
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture_;
        LoadTexture(device_, path_, 0xFF808080, texture_);
        runtimePart_.noiseTextures.push_back(texture_);
    }
    if (runtimePart_.maskTextures.empty())
    {
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture_;
        LoadTexture(device_, part_.maskTexturePath, 0xFFFFFFFF, texture_);
        runtimePart_.maskTextures.push_back(texture_);
    }
    if (runtimePart_.noiseTextures.empty())
    {
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture_;
        LoadTexture(device_, part_.noiseTexturePath, 0xFF808080, texture_);
        runtimePart_.noiseTextures.push_back(texture_);
    }
    runtimePart_.distortionNoiseTexture.Reset();
    if (!part_.distortionNoiseTexturePath.empty())
    {
        LoadTexture(
            device_,
            part_.distortionNoiseTexturePath,
            0xFF808080,
            runtimePart_.distortionNoiseTexture);
    }
    runtimePart_.vertexNoiseTexture.Reset();
    if (!part_.vertexNoiseTexturePath.empty())
    {
        LoadTexture(
            device_,
            part_.vertexNoiseTexturePath,
            0xFF808080,
            runtimePart_.vertexNoiseTexture);
    }
}

void MeshVFXComponent::LoadTexture(
    ID3D11Device* device_,
    const std::string& path_,
    UINT fallbackColor_,
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& texture_)
{
    texture_.Reset();
    if (!path_.empty() && std::filesystem::exists(WindowsDialogHelper::Utf8ToPath(path_)))
    {
        ID3D11ShaderResourceView* loadedTexture_ =
            TextureResourceManager::Instance().LoadTexture(device_, path_);
        if (loadedTexture_)
        {
            texture_ = loadedTexture_;
            return;
        }
    }
    CPUResourceLoader::CreateDummyTexture(device_, fallbackColor_, texture_.GetAddressOf());
}

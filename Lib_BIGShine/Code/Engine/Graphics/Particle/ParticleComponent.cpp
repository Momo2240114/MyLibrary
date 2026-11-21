#include "PrecompiledHeader.h"

#include "ParticleComponent.h"

#include "Editor/AssetSlotGui.h"


#include "Engine/Graphics/Core/CPUResourceLoader.h"
#include "Engine/Platform/WindowsDialogHelper.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Graphics/Resource/GlTFModel.h"
#include "Engine/Graphics/VFXMaterial/VFXMaterialColorUtility.h"
#include "Engine/Graphics/VFXMaterial/VFXMaterialConversionUtility.h"
#include "Engine/Graphics/VFXMaterial/VFXMaterialResourceManager.h"
#include "Engine/Graphics/VFXMaterial/VFXMaterialSerializer.h"
#include "Engine/GamePlay/Serialize/ActionRegistry.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Graphics/ModelDissolveComponent.h"
#include <imgui_stdlib.h>
#include "Engine/Debug/TracyProfiler.h"
#include "Engine/GamePlay/Combat/HealthComponent.h"
#include "Engine/UI/ImGuiColorEditor.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <functional>
#include <fstream>
#include <imgui_stdlib.h>
#include <nlohmann/json.hpp>

namespace
{
    void ApplyParticleMaterialColor(
        DirectX::XMFLOAT4& color_,
        const VFXMaterialData& materialData_,
        const DirectX::XMFLOAT4& rampColor_,
        const DirectX::XMFLOAT4& randomColor_)
    {
        BS_PROFILE_SCOPE("ParticleComponent::ApplyParticleMaterialColor");

        const float alpha_ = std::max(0.0f, materialData_.alpha);
        const float emissiveMultiplier_ = std::max(1.0f, materialData_.emissiveIntensity);

        color_.x *= materialData_.baseColor.x * materialData_.emissiveColor.x * emissiveMultiplier_;
        color_.y *= materialData_.baseColor.y * materialData_.emissiveColor.y * emissiveMultiplier_;
        color_.z *= materialData_.baseColor.z * materialData_.emissiveColor.z * emissiveMultiplier_;
        color_.w *= materialData_.baseColor.w * alpha_;
        color_ = VFXMaterialColorUtility::ApplyColorRamp(
            color_,
            materialData_.colorRamp,
            rampColor_);
        color_ = VFXMaterialColorUtility::ApplyRandomColor(
            color_,
            materialData_.randomColor,
            randomColor_);
    }

    uint32_t BuildParticleMaterialRandomSeed(
        const std::string& materialPath_,
        const std::string& emitterName_)
    {
        BS_PROFILE_SCOPE("ParticleComponent::BuildParticleMaterialRandomSeed");

        const std::string key_ = materialPath_ + ":" + emitterName_;
        return static_cast<uint32_t>(std::hash<std::string>{}(key_));
    }
}

void ParticleComponent::Start()
{
    // Editor候補の更新と、保存済みAssetが指定されている場合の遅延読み込みを行う。
    RefreshTextureList();
    RefreshAssetList();

    if (!assetPath.empty() && tracks.empty())
    {
        LoadAsset(assetPath);
    }

    RebuildEmitterResources();
}

void ParticleComponent::RegisterActions()
{
    // ActionSequencerなどからParticleを操作できるよう、GameObjectローカルActionを登録する。
    auto gameObject_ = GetGameObject();
    if (!gameObject_)
    {
        return;
    }

    auto& registry_ = gameObject_->GetLocalRegistry();
    for (const std::string& actionName_ : registeredActionNames)
    {
        registry_.RemoveAction("Particle", actionName_);
    }
    registeredActionNames.clear();

    const std::string objectName_ = gameObject_->GetName();
    auto registerAction_ = [&](const std::string& actionName_, ActionRegistry::ActionFunc action_)
        {
            registry_.RegisterAction("Particle", actionName_, std::move(action_));
            registeredActionNames.push_back(actionName_);
        };

    registerAction_(objectName_ + "_Particle_Play", [this]()
    {
        Play();
    });
    registerAction_(objectName_ + "_Particle_Stop", [this]()
    {
        Stop();
    });

    for (const EmitterTrack& track_ : tracks)
    {
        const std::string emitterName_ = track_.name;
        const std::string actionPrefix_ = objectName_ + "_Particle_" + emitterName_;
        registerAction_(actionPrefix_ + "_Play", [this, emitterName_]()
        {
            PlayEmitter(emitterName_);
        });
        registerAction_(actionPrefix_ + "_Stop", [this, emitterName_]()
        {
            StopEmitter(emitterName_);
        });
        registerAction_(actionPrefix_ + "_Trigger", [this, emitterName_]()
        {
            TriggerEmitter(emitterName_);
        });
    }
}

void ParticleComponent::Gui()
{
    // Particle Asset全体、Emitter単体、各EmitterパラメータをこのGUIで編集する。
    RefreshAssetList();

    ImGui::Checkbox("Scale Emission By Health", &scaleEmissionByHealth);
    if (scaleEmissionByHealth)
    {
        ImGui::InputText("Health Owner (empty = self)", &healthOwnerName);
        ImGui::DragFloat("Full HP Emission Multiplier", &healthyEmissionMultiplier, 0.05f, 0.0f, 10.0f);
        ImGui::DragFloat("Low HP Emission Multiplier", &lowHealthEmissionMultiplier, 0.05f, 0.0f, 10.0f);
    }
    ImGui::InputText("Mesh Source Object", &meshSourceObjectName);
    ImGui::InputText("Attraction Target Object", &attractionTargetName);
    ImGui::Checkbox("Emit Near Dissolve Direction Boundary", &emitDissolveBoundary);
    ImGui::DragFloat("Boundary Band", &dissolveBoundaryBand, 0.01f, 0.01f, 1.0f);
    ImGui::SeparatorText("Particle Asset");
    ImGui::InputText("Asset Name", assetNameBuffer, IM_ARRAYSIZE(assetNameBuffer));
    ImGui::Text("Current: %s", assetPath.empty() ? "Unsaved" : assetPath.c_str());

    std::string selectedAssetPath_ = assetPath;
    if (AssetSlotGui::Draw(
        "Particle JSON Asset",
        AssetSlotType::JSON,
        selectedAssetPath_,
        nullptr,
        "Data/Particle"))
    {
        if (!selectedAssetPath_.empty())
        {
            LoadAsset(selectedAssetPath_);
        }
    }

    if (ImGui::Button("Save Particle JSON..."))
    {
        const std::string initialPath_ = assetPath.empty()
            ? BuildAssetPathFromName()
            : assetPath;
        const std::string selectedPath_ = WindowsDialogHelper::SaveJsonDialog(
            L"Save Particle Asset",
            initialPath_,
            GraphicsDeviceManager::Instance().GetWindowHandle());
        if (!selectedPath_.empty() && SaveAsset(selectedPath_))
        {
            RefreshAssetList();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload Asset") && !assetPath.empty())
    {
        LoadAsset(assetPath);
    }

    ImGui::SeparatorText("Playback");

    if (ImGui::Button("Play Effect"))
    {
        Play();
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop"))
    {
        Stop();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset All"))
    {
        for (auto& track : tracks)
        {
            track.Reset();
        }
    }

    ImGui::Separator();

    if (ImGui::Button("Add Emitter"))
    {
        // 新規Emitterは、勝手に暴れすぎない安全な初期値から作る。
        EmitterTrack newTrack;
        newTrack.name = "Emitter_" + std::to_string(tracks.size());
        newTrack.nameId = StringIdUtility::Hash(newTrack.name);
        newTrack.settings = ParticleSystemConstants();
        newTrack.settings.maxParticleCount = 1000;
        newTrack.settings.burstCount = 100;


        newTrack.settings.emissionSpeed = { 0.0f, 0.0f };
        newTrack.settings.gravity = -9.8f;
        newTrack.settings.emissionAngularSpeed = { 0.0f, 0.0f };
        newTrack.settings.noiseScale = 0.0f;
        newTrack.settings.noiseStrength = 0.0f;
        newTrack.settings.drag = 0.0f;
        newTrack.settings.windVelocity = { 0.0f, 0.0f, 0.0f, 0.0f };
        newTrack.settings.shapeSize = { 1, 1, 1, 0 };
        newTrack.emitter = std::make_unique<ParticleEmitter>(
            GraphicsDeviceManager::Instance().GetDevice(),
            newTrack.settings,
            newTrack.settings.maxParticleCount);
        tracks.push_back(std::move(newTrack));
        RefreshEmitterNameIndex();
        RegisterActions();
    }

    if (tracks.empty())
    {
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "No Emitters. Click 'Add Emitter' or import one.");
        if (AssetSlotGui::Draw(
            "Emitter JSON Asset",
            AssetSlotType::JSON,
            emitterAssetPath,
            nullptr,
            "Data/Particle/Emitter"))
        {
            if (!emitterAssetPath.empty())
            {
                LoadEmitterAsset(emitterAssetPath);
            }
        }
        return;
    }

    if (selectedEmitterIndex >= static_cast<int>(tracks.size()))
    {
        selectedEmitterIndex = static_cast<int>(tracks.size()) - 1;
    }
    if (selectedEmitterIndex < 0)
    {
        selectedEmitterIndex = 0;
    }

    ImGui::Text("Emitter List");
    ImGui::BeginChild("EmitterList", ImVec2(160, 96), true);
    for (int i = 0; i < static_cast<int>(tracks.size()); ++i)
    {
        ImGui::PushID(i);
        const std::string label_ = std::to_string(i) + ": " + tracks[i].name;
        if (ImGui::Selectable(label_.c_str(), selectedEmitterIndex == i))
        {
            selectedEmitterIndex = i;
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::SameLine();

    ImGui::BeginGroup();
    bool shouldRemove = ImGui::Button("Remove Selected");
    if (ImGui::Button("Save Emitter JSON..."))
    {
        const std::string initialPath_ = emitterAssetPath.empty()
            ? BuildEmitterAssetPathFromName(static_cast<size_t>(selectedEmitterIndex))
            : emitterAssetPath;
        const std::string selectedPath_ = WindowsDialogHelper::SaveJsonDialog(
            L"Save Particle Emitter",
            initialPath_,
            GraphicsDeviceManager::Instance().GetWindowHandle());
        if (!selectedPath_.empty() &&
            SaveEmitterAsset(static_cast<size_t>(selectedEmitterIndex), selectedPath_))
        {
            emitterAssetPath = selectedPath_;
        }
    }
    if (AssetSlotGui::Draw(
        "Emitter JSON Asset",
        AssetSlotType::JSON,
        emitterAssetPath,
        nullptr,
        "Data/Particle/Emitter"))
    {
        if (!emitterAssetPath.empty() && LoadEmitterAsset(emitterAssetPath))
        {
            selectedEmitterIndex = static_cast<int>(tracks.size()) - 1;
        }
    }
    if (ImGui::Button("Refresh Textures"))
    {
        RefreshTextureList();
    }
    ImGui::EndGroup();

    if (shouldRemove)
    {
        tracks.erase(tracks.begin() + selectedEmitterIndex);
        if (selectedEmitterIndex > 0)
        {
            --selectedEmitterIndex;
        }
        RefreshEmitterNameIndex();
        RegisterActions();
        return;
    }

    ImGui::Separator();

    ImGui::PushID(selectedEmitterIndex);
    auto& track = tracks[selectedEmitterIndex];
    auto& s = track.settings;

    if (ImGui::InputText("Emitter Name", &track.name))
    {
        track.nameId = StringIdUtility::Hash(track.name);
        RefreshEmitterNameIndex();
        RegisterActions();
    }
    ImGui::SameLine();
    if (ImGui::Button("Play Emitter"))
    {
        PlayEmitter(track.name);
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop Emitter"))
    {
        StopEmitter(track.name);
    }
    ImGui::SameLine();
    if (ImGui::Button("Trigger Emitter"))
    {
        TriggerEmitter(track.name);
    }

    const bool hasDuplicateName_ = std::count_if(tracks.begin(), tracks.end(),
        [&](const EmitterTrack& track_)
    {
        return track_.name == track.name;
    }) > 1;
    if (hasDuplicateName_)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.25f, 1.0f),
            "Emitter names must be unique for individual Actions.");
    }
    ImGui::TextDisabled("Individual Actions: <Object>_Particle_%s_Play / Stop / Trigger", track.name.c_str());

    if (ImGui::BeginTabBar("EmitterParams"))
    {
        if (ImGui::BeginTabItem("Emission"))
        {
            // 発生数・寿命・初速など、Particleがいつ何個生まれるかを調整する。
            int maxCount = static_cast<int>(s.maxParticleCount);
            if (ImGui::DragInt("Max Count", &maxCount, 1, 2000000))
            {
                s.maxParticleCount = static_cast<uint32_t>(maxCount);
            }
            ImGui::DragFloat("Delay", &track.delay, 0.01f, 0.0f, 60.0f);
            ImGui::DragFloat2("Life Min/Max", &s.lifespan.x, 0.05f, 0.01f, 60.0f);
            ImGui::DragFloat2("Initial Speed Min/Max", &s.emissionSpeed.x, 0.05f, 0.0f, 100.0f);

            const char* modes[] = { "Continuous", "Burst" };
            ImGui::Combo("Mode", reinterpret_cast<int*>(&s.emissionMode), modes, IM_ARRAYSIZE(modes));
            if (s.emissionMode == 0)
            {
                ImGui::DragInt("Spawn Rate", reinterpret_cast<int*>(&s.burstCount), 1, 0, 20000);
            }
            else
            {
                ImGui::DragInt("Burst Count", reinterpret_cast<int*>(&s.burstCount), 1, 0, static_cast<int>(s.maxParticleCount));
                if (ImGui::Button("Fire Burst"))
                {
                    s.burstTrigger = 1.0f;
                }
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Physics"))
        {
            // 生成後の移動、Noise、中心方向の速度など、軌道の性格を調整する。
            ImGui::DragFloat("Gravity / Acceleration Y", &s.gravity, 0.01f, -100.0f, 100.0f);
            ImGui::DragFloat3("Additional Acceleration XYZ", &s.windVelocity.x, 0.01f, -100.0f, 100.0f);
            ImGui::DragFloat("Drag", &s.drag, 0.01f, 0.0f, 20.0f);
            ImGui::DragFloat2("Direction Cone Angle", &s.emissionConeAngle.x, 0.01f, 0.0f, 6.28f);
            ImGui::DragFloat2("Angular Velocity", &s.emissionAngularSpeed.x, 0.01f, -50.0f, 50.0f);

            const char* spawnDirectionModes_[] =
            {
                "Cone / Up",
                "From Center Outward",
                "To Center Inward",
                "Axis Direction"
            };
            int spawnDirectionMode_ = static_cast<int>(s.spawnDirectionMode);
            if (ImGui::Combo("Initial Direction", &spawnDirectionMode_, spawnDirectionModes_, IM_ARRAYSIZE(spawnDirectionModes_)))
            {
                s.spawnDirectionMode = static_cast<uint32_t>(
                    std::clamp(spawnDirectionMode_, 0, IM_ARRAYSIZE(spawnDirectionModes_) - 1));
            }
            ImGui::BeginDisabled(
                s.spawnDirectionMode != static_cast<uint32_t>(PARTICLE_SPAWN_DIRECTION_MODE::AXIS));
            ImGui::DragFloat3("Direction Axis", &s.emissionAxis.x, 0.01f, -1.0f, 1.0f);
            ImGui::EndDisabled();
            ImGui::TextDisabled("Axis uses Initial Speed and Cone Angle around Direction Axis.");

            ImGui::SeparatorText("Target Pull");
            bool useTargetAttraction_ = s.useTargetAttraction != 0u;
            if (ImGui::Checkbox("Use Target Pull", &useTargetAttraction_))
            {
                s.useTargetAttraction = useTargetAttraction_ ? 1u : 0u;
            }
            ImGui::BeginDisabled(s.useTargetAttraction == 0);
            ImGui::DragFloat3("Target Position", &s.targetPosition.x, 0.01f);
            ImGui::DragFloat("Target Strength", &s.targetAttractionStrength, 0.01f, -100.0f, 100.0f);
            ImGui::DragFloat("Target Range", &s.targetAttractionRange, 0.01f, 0.0f, 100.0f);
            ImGui::DragFloat("Target Falloff", &s.targetAttractionFalloff, 0.01f, 0.001f, 8.0f);
            ImGui::EndDisabled();

            ImGui::SeparatorText("Turbulence / Noise");
            ImGui::DragFloat("Noise Strength", &s.noiseStrength, 0.01f, 0.0f, 100.0f);
            ImGui::DragFloat("Noise Frequency", &s.noiseFrequency, 0.01f, 0.01f, 20.0f);
            ImGui::DragFloat("Noise Speed", &s.noiseSpeed, 0.01f, -20.0f, 20.0f);
            s.noiseScale = s.noiseStrength;

            ImGui::SeparatorText("Velocity Modules");
            ImGui::DragFloat("Radial Velocity", &s.radialVelocity, 0.01f, -100.0f, 100.0f);
            ImGui::DragFloat("Orbit Velocity", &s.orbitVelocity, 0.01f, -100.0f, 100.0f);
            ImGui::TextDisabled("Default: Initial Speed 0, Gravity -9.8, Angular Velocity 0, Noise 0.");
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Shape"))
        {
            // 発生位置の形状を調整する。見た目の広がりはここ、飛ぶ方向はPhysics側で分けて扱う。
            const char* shapes[] = { "Point", "Box", "Circle", "Sphere", "Line", "Mesh Vertices" };
            ImGui::Combo("Spawn Shape", reinterpret_cast<int*>(&s.spawnShape), shapes, IM_ARRAYSIZE(shapes));
            const char* spawnSpaces_[] = { "Owner Relative", "World" };
            int spawnSpace_ = static_cast<int>(track.spawnSpace);
            if (ImGui::Combo("Position Space", &spawnSpace_, spawnSpaces_, IM_ARRAYSIZE(spawnSpaces_)))
            {
                track.spawnSpace = static_cast<ParticleSpawnSpace>(spawnSpace_);
            }
            ImGui::DragFloat3(
                track.spawnSpace == ParticleSpawnSpace::WORLD ? "World Position" : "Emitter Offset",
                &s.emissionPosition.x, 0.05f);
            ImGui::DragFloat3("Shape Size", &s.shapeSize.x, 0.05f, 0.0f, 100.0f);
            ImGui::DragFloat("Vertical Spawn Range", &s.verticalSpawnRange, 0.05f, 0.0f, 100.0f);
            ImGui::TextDisabled("Adds random Y spread for Point, Circle and Mesh Vertices.");
            if (s.spawnShape == 4)
            {
                ImGui::DragFloat3("Line Start", &s.shapeStart.x, 0.05f);
                ImGui::DragFloat3("Line End", &s.shapeEnd.x, 0.05f);
            }
            else if (s.spawnShape == 5)
            {
                int meshSampleCount_ = static_cast<int>(track.meshVertexSampleCount);
                if (ImGui::SliderInt("Mesh Vertex Samples", &meshSampleCount_, 1, 20000))
                {
                    track.meshVertexSampleCount = static_cast<uint32_t>(meshSampleCount_);
                }
                auto owner_ = GetGameObject();
                const bool hasSourceModel_ = owner_ && owner_->GetComponent<GltfModelCommponennt>() != nullptr;
                ImGui::TextColored(
                    hasSourceModel_ ? ImVec4(0.25f, 1.0f, 0.40f, 1.0f) : ImVec4(1.0f, 0.45f, 0.25f, 1.0f),
                    hasSourceModel_ ? "Source: GltfModel on this GameObject" : "Add GltfModel to this GameObject");
            }
            ImGui::DragFloat2("Legacy Radius Min/Max", &s.emissionOffset.x, 0.05f, 0.0f, 100.0f);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Visual"))
        {
            // 色、サイズ、フェードなど、Particle 1枚ごとの見た目を調整する。
            bool useColorOverLife_ = s.useColorOverLife != 0;
            if (ImGui::Checkbox("Color Over Life", &useColorOverLife_))
            {
                s.useColorOverLife = useColorOverLife_ ? 1u : 0u;
                if (s.useColorOverLife != 0)
                {
                    s.colorStart = s.manualColor;
                    s.colorEnd = s.manualColor;
                }
            }

            bool useRandomColor_ = s.useRandomColor != 0;
            if (ImGui::Checkbox("Random Color", &useRandomColor_))
            {
                s.useRandomColor = useRandomColor_ ? 1u : 0u;
            }
            if (s.useColorOverLife != 0)
            {
                ImGuiColorEditor::DrawColorEdit4(useRandomColor_ ? "Start Color A" : "Start Color", &s.colorStart.x);
                if (useRandomColor_)
                {
                    ImGuiColorEditor::DrawColorEdit4("Start Color B", &s.randomColorStart.x);
                }
                ImGuiColorEditor::DrawColorEdit4(useRandomColor_ ? "End Color A" : "End Color", &s.colorEnd.x);
                if (useRandomColor_)
                {
                    ImGuiColorEditor::DrawColorEdit4("End Color B", &s.randomColorEnd.x);
                }
            }
            else
            {
                ImGuiColorEditor::DrawColorEdit4(useRandomColor_ ? "Color A" : "Color", &s.manualColor.x);
                if (useRandomColor_)
                {
                    ImGuiColorEditor::DrawColorEdit4("Color B", &s.randomColorStart.x);
                }
            }
            if (useRandomColor_ && s.useGradientTexture != 0)
            {
                ImGui::TextDisabled("Gradient Texture supplies RGB. Random Color still varies alpha.");
            }

            ImGui::DragFloat2("Size Start/End", &s.emissionSize.x, 0.02f, 0.01f, 100.0f);
            bool useRandomSize_ = s.useRandomSize != 0;
            if (ImGui::Checkbox("Random Spawn Size", &useRandomSize_))
            {
                s.useRandomSize = useRandomSize_ ? 1u : 0u;
            }
            if (s.useRandomSize != 0)
            {
                ImGui::DragFloat2("Start Size Random", &s.sizeRandomStart.x, 0.01f, 0.01f, 20.0f, "%.2f");
                ImGui::DragFloat2("End Size Random", &s.sizeRandomEnd.x, 0.01f, 0.01f, 20.0f, "%.2f");
                if (s.sizeRandomStart.x > s.sizeRandomStart.y)
                {
                    std::swap(s.sizeRandomStart.x, s.sizeRandomStart.y);
                }
                if (s.sizeRandomEnd.x > s.sizeRandomEnd.y)
                {
                    std::swap(s.sizeRandomEnd.x, s.sizeRandomEnd.y);
                }
            }
            ImGui::DragFloat2("Fade In/Out", &s.fadeDuration.x, 0.01f, 0.0f, 10.0f);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Texture"))
        {
            // 色Texture、Mask、Gradient、SpriteSheetを差し替える。
            if (track.materialPaths.empty())
            {
                track.materialPaths.push_back(track.materialPath);
            }
            int removeMaterialIndex_ = -1;
            for (int materialIndex_ = 0; materialIndex_ < static_cast<int>(track.materialPaths.size()); ++materialIndex_)
            {
                ImGui::PushID(materialIndex_);
                ID3D11ShaderResourceView* materialPreviewTexture_ = nullptr;
                if (materialIndex_ < static_cast<int>(track.materialResources.size()) &&
                    track.materialResources[materialIndex_])
                {
                    materialPreviewTexture_ = track.materialResources[materialIndex_]->GetBaseTexture();
                }
                if (AssetSlotGui::Draw(
                    "VFX Material Slot",
                    AssetSlotType::MATERIAL,
                    track.materialPaths[materialIndex_],
                    materialPreviewTexture_,
                    "Data/VFXMaterials"))
                {
                    track.materialPath = track.materialPaths.empty() ? std::string{} : track.materialPaths.front();
                    LoadEmitterMaterial(track, track.materialPath);
                }
                if (track.materialPaths.size() > 1u)
                {
                    ImGui::SameLine();
                    if (ImGui::Button("Remove"))
                    {
                        removeMaterialIndex_ = materialIndex_;
                    }
                }
                ImGui::PopID();
            }
            if (removeMaterialIndex_ >= 0)
            {
                track.materialPaths.erase(track.materialPaths.begin() + removeMaterialIndex_);
                track.materialPath = track.materialPaths.empty() ? std::string{} : track.materialPaths.front();
                LoadEmitterMaterial(track, track.materialPath);
            }
            if (ImGui::Button("Add VFX Material Slot"))
            {
                track.materialPaths.emplace_back();
            }
            ImGui::SameLine();
            if (ImGui::Button("Convert To VFX Material"))
            {
                ConvertEmitterToMaterial(track);
            }

            auto drawTextureSlot_ = [&](const char* label_, std::string& fileName_, std::string& path_, int slot_)
                {
                    ID3D11ShaderResourceView* texture_ = nullptr;
                    if (track.emitter)
                    {
                        if (slot_ == 0)
                    {
                        texture_ = track.emitter->GetTextureSRV();
                    }
                    else if (slot_ == 1)
                    {
                        texture_ = track.emitter->GetMaskTextureSRV();
                    }
                    else if (slot_ == 2)
                    {
                        texture_ = track.emitter->GetGradientTextureSRV();
                    }
                    else
                    {
                        texture_ = track.emitter->GetShadeGradientTextureSRV();
                    }
                }
                    if (!AssetSlotGui::Draw(
                        label_, AssetSlotType::TEXTURE, path_, texture_, "Data/Texture/Particle"))
                {
                    return;
                }

                fileName_ = path_.empty()
                        ? std::string()
                        : WindowsDialogHelper::PathToUtf8(
                            WindowsDialogHelper::Utf8ToPath(path_).filename());
                    if (!path_.empty() && LoadEmitterTextureSlot(track, path_, slot_))
                    {
                        if (slot_ == 1)
                    {
                        s.useMaskTexture = 1;
                    }
                    if (slot_ == 2)
                    {
                        s.useGradientTexture = 1;
                    }
                    if (slot_ == 3)
                    {
                        s.useShadeGradientTexture = 1;
                    }
                    return;
                    }

                    auto device_ = GraphicsDeviceManager::Instance().GetDevice();
                    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> dummyTexture_;
                    CPUResourceLoader::CreateDummyTexture(device_, 0xFFFFFFFFu, dummyTexture_.GetAddressOf());
                    if (track.emitter)
                    {
                        if (slot_ == 0)
                    {
                        track.emitter->SetTexture(dummyTexture_);
                    }
                    else if (slot_ == 1)
                    {
                        track.emitter->SetMaskTexture(dummyTexture_);
                    }
                    else if (slot_ == 2)
                    {
                        track.emitter->SetGradientTexture(dummyTexture_);
                    }
                    else
                    {
                        track.emitter->SetShadeGradientTexture(dummyTexture_);
                    }
                }
                    if (slot_ == 1)
                {
                    s.useMaskTexture = 0;
                }
                if (slot_ == 2)
                {
                    s.useGradientTexture = 0;
                }
                if (slot_ == 3)
                {
                    s.useShadeGradientTexture = 0;
                }
            };

            drawTextureSlot_("Base Texture", track.textureFileName, track.texturePath, 0);
            drawTextureSlot_("Mask Texture", track.maskTextureFileName, track.maskTexturePath, 1);
            drawTextureSlot_("Gradient Texture", track.gradientTextureFileName, track.gradientTexturePath, 2);
            drawTextureSlot_("Shade Gradient Texture", track.shadeGradientTextureFileName, track.shadeGradientTexturePath, 3);
            bool useMask = s.useMaskTexture != 0;
            bool useGradient = s.useGradientTexture != 0;
            bool useShadeGradient_ = s.useShadeGradientTexture != 0;
            if (ImGui::Checkbox("Use Mask", &useMask))
            {
                s.useMaskTexture = useMask ? 1u : 0u;
            }
            if (ImGui::Checkbox("Use Gradient Texture", &useGradient))
            {
                s.useGradientTexture = useGradient ? 1u : 0u;
            }
            if (s.useGradientTexture != 0)
            {
                ImGui::TextDisabled("GradientTexture enabled: Particle RGB color is ignored. Alpha is still used.");
                int sampleMode_ = static_cast<int>(s.gradientSampleMode);
                const char* sampleModeNames_[] = { "Horizontal", "Vertical" };
                if (ImGui::Combo("Gradient Sample Mode", &sampleMode_, sampleModeNames_, IM_ARRAYSIZE(sampleModeNames_)))
                {
                    s.gradientSampleMode = static_cast<uint32_t>(sampleMode_);
                }
                int direction_ = static_cast<int>(s.gradientDirection);
                const char* directionNames_[] = { "Forward", "Reverse" };
                if (ImGui::Combo("Gradient Direction", &direction_, directionNames_, IM_ARRAYSIZE(directionNames_)))
                {
                    s.gradientDirection = static_cast<uint32_t>(direction_);
                }
                ImGui::SliderFloat("Gradient Fixed UV", &s.gradientFixedUv, 0.0f, 1.0f);
                ImGui::SliderFloat("Gradient Start UV", &s.gradientStartUv, 0.0f, 1.0f);
                ImGui::SliderFloat("Gradient End UV", &s.gradientEndUv, 0.0f, 1.0f);
            }
            if (ImGui::Checkbox("Use Shade Gradient Texture", &useShadeGradient_))
            {
                s.useShadeGradientTexture = useShadeGradient_ ? 1u : 0u;
            }
            if (s.useShadeGradientTexture != 0)
            {
                int sampleMode_ = static_cast<int>(s.shadeGradientSampleMode);
                const char* sampleModeNames_[] = { "Horizontal", "Vertical" };
                if (ImGui::Combo("Shade Gradient Sample Mode", &sampleMode_, sampleModeNames_, IM_ARRAYSIZE(sampleModeNames_)))
                {
                    s.shadeGradientSampleMode = static_cast<uint32_t>(sampleMode_);
                }
                int direction_ = static_cast<int>(s.shadeGradientDirection);
                const char* directionNames_[] = { "Forward", "Reverse" };
                if (ImGui::Combo("Shade Gradient Direction", &direction_, directionNames_, IM_ARRAYSIZE(directionNames_)))
                {
                    s.shadeGradientDirection = static_cast<uint32_t>(direction_);
                }
                ImGui::SliderFloat("Shade Gradient Fixed UV", &s.shadeGradientFixedUv, 0.0f, 1.0f);
                ImGui::SliderFloat("Shade Gradient Start UV", &s.shadeGradientStartUv, 0.0f, 1.0f);
                ImGui::SliderFloat("Shade Gradient End UV", &s.shadeGradientEndUv, 0.0f, 1.0f);
            }

            ImGui::SeparatorText("Sprite Sheet");
            int splitCount_[2] = { static_cast<int>(s.textureSplitCount.x), static_cast<int>(s.textureSplitCount.y) };
            if (ImGui::InputInt2("Columns / Rows", splitCount_))
            {
                s.textureSplitCount.x = static_cast<uint32_t>((std::max)(1, splitCount_[0]));
                s.textureSplitCount.y = static_cast<uint32_t>((std::max)(1, splitCount_[1]));
            }

            int spriteIndex_[2] = { static_cast<int>(s.spriteSheetIndex.x), static_cast<int>(s.spriteSheetIndex.y) };
            if (ImGui::InputInt2("Cell X / Y", spriteIndex_))
            {
                s.spriteSheetIndex.x = static_cast<uint32_t>((std::clamp)(spriteIndex_[0], 0, static_cast<int>(s.textureSplitCount.x) - 1));
                s.spriteSheetIndex.y = static_cast<uint32_t>((std::clamp)(spriteIndex_[1], 0, static_cast<int>(s.textureSplitCount.y) - 1));
            }

            const uint32_t totalCells_ = s.textureSplitCount.x * s.textureSplitCount.y;
            bool useSpriteAnimation_ = s.useSpriteAnimation != 0;
            if (ImGui::Checkbox("Animate Sprite Index", &useSpriteAnimation_))
            {
                s.useSpriteAnimation = useSpriteAnimation_ ? 1u : 0u;
            }
            if (s.useSpriteAnimation != 0)
            {
                int animationRange_[2] = { static_cast<int>(s.spriteAnimationStartIndex), static_cast<int>(s.spriteAnimationEndIndex) };
                if (ImGui::InputInt2("Start / End Index", animationRange_))
                {
                    s.spriteAnimationStartIndex = static_cast<uint32_t>((std::clamp)(animationRange_[0], 0, static_cast<int>(totalCells_) - 1));
                    s.spriteAnimationEndIndex = static_cast<uint32_t>((std::clamp)(animationRange_[1], 0, static_cast<int>(totalCells_) - 1));
                }
                ImGui::DragFloat("Frames Per Second", &s.spriteAnimationFps, 0.1f, 0.0f, 120.0f, "%.1f");
                bool spriteAnimationLoop_ = s.spriteAnimationLoop != 0;
                if (ImGui::Checkbox("Loop Sprite Animation", &spriteAnimationLoop_))
                {
                    s.spriteAnimationLoop = spriteAnimationLoop_ ? 1u : 0u;
                }
            }
            ImGui::Text("Selected Linear Index: %u", s.spriteSheetIndex.y * s.textureSplitCount.x + s.spriteSheetIndex.x);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Render"))
        {
            // Billboardの種類や合成方法など、描画時の扱いを決める。
            const char* renderTypes[] = { "Billboard", "Stretched" };
            ImGui::Combo("Render Type", reinterpret_cast<int*>(&s.type), renderTypes, IM_ARRAYSIZE(renderTypes));
            if (s.type == 1)
            {
                ImGui::DragFloat("Stretch Scale", &s.stretchScale, 0.001f, 0.0f, 2.0f, "%.3f");
                ImGui::DragFloat("Stretch Max", &s.stretchMax, 0.1f, 0.01f, 1000.0f);
            }

            const char* blendModes_[] = { "Alpha", "Additive" };
            int blendMode_ = static_cast<int>(track.blendMode);
            if (ImGui::Combo("Blend Mode", &blendMode_, blendModes_, IM_ARRAYSIZE(blendModes_)))
            {
                track.blendMode = static_cast<ParticleBlendMode>(blendMode_);
            }

            bool useFresnel_ = s.useFresnel != 0;
            if (ImGui::Checkbox("Use Fresnel", &useFresnel_))
            {
                s.useFresnel = useFresnel_ ? 1u : 0u;
            }
            if (s.useFresnel != 0)
            {
                ImGui::DragFloat("Fresnel Power", &s.fresnelPower, 0.01f, 0.1f, 10.0f);
                ImGui::DragFloat("Fresnel Strength", &s.fresnelStrength, 0.01f, 0.0f, 10.0f);
            }
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }


    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f, 0.55f, 0.22f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.14f, 0.72f, 0.29f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.08f, 0.42f, 0.17f, 1.0f));
    if (ImGui::Button("APPLY CHANGES", ImVec2(-1.0f, 38.0f)))
    {
        const bool wasPlaying_ = isPlaying;
        RebuildEmitterResources();
        if (wasPlaying_)
        {
            Play();
        }
        applyFeedbackUntil = ImGui::GetTime() + 2.5;
    }
    ImGui::PopStyleColor(3);

    if (ImGui::GetTime() < applyFeedbackUntil)
    {
        ImGui::TextColored(ImVec4(0.25f, 1.0f, 0.40f, 1.0f), "APPLIED: Emitters rebuilt and preview restarted.");
    }
    else
    {
        ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.18f, 1.0f), "Press APPLY CHANGES to confirm edited settings.");
    }

    ImGui::PopID();
}

bool ParticleComponent::LoadEmitterTexture(EmitterTrack& track, const std::string& fullPath)
{
    return LoadEmitterTextureSlot(track, fullPath, 0);
}

bool ParticleComponent::LoadEmitterTextureSlot(EmitterTrack& track, const std::string& fullPath, int slot)
{
    if (!track.emitter || fullPath.empty())
    {
        return false;
    }

    // slot番号でColor/Mask/Gradientを切り替え、Emitter内部のSRVへ反映する。
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    const std::wstring wpath = WindowsDialogHelper::Utf8ToPath(fullPath).wstring();

    if (SUCCEEDED(CPUResourceLoader::LoadTextureFromFile(
        GraphicsDeviceManager::Instance().GetDevice(),
        wpath.c_str(), srv.GetAddressOf(), nullptr)))
    {
        if (slot == 1)
        {
            track.emitter->SetMaskTexture(srv);
            track.maskTexturePath = fullPath;
            track.settings.useMaskTexture = 1;
        }
        else if (slot == 2)
        {
            track.emitter->SetGradientTexture(srv);
            track.gradientTexturePath = fullPath;
            track.settings.useGradientTexture = 1;
        }
        else if (slot == 3)
        {
            track.emitter->SetShadeGradientTexture(srv);
            track.shadeGradientTexturePath = fullPath;
            track.settings.useShadeGradientTexture = 1;
        }
        else
        {
            track.emitter->SetTexture(srv);
            track.texturePath = fullPath;
        }
        return true;
    }
    return false;
}

bool ParticleComponent::LoadEmitterMaterial(
    EmitterTrack& track_,
    const std::string& materialPath_)
{
    BS_PROFILE_SCOPE("ParticleComponent::LoadEmitterMaterial");

    if (!LoadEmitterMaterialResource(track_, materialPath_))
    {
        return false;
    }

    ApplyEmitterMaterialTextures(track_);

    if (track_.emitter)
    {
        ParticleSystemConstants evaluatedSettings_ = track_.settings;
        ApplyEmitterMaterialToSettings(track_, evaluatedSettings_, timer);
        track_.emitter->ApplySettings(evaluatedSettings_);
        track_.emitter->SetDepthSortEnabled(track_.runtimeBlendMode == ParticleBlendMode::ALPHA);
    }

    return true;
}

bool ParticleComponent::LoadEmitterMaterialResource(
    EmitterTrack& track_,
    const std::string& materialPath_)
{
    BS_PROFILE_SCOPE("ParticleComponent::LoadEmitterMaterialResource");

    track_.materialPath = materialPath_;
    track_.materialResource.reset();
    track_.materialResources.clear();
    track_.materialRandomColors.clear();
    track_.runtimeBlendMode = track_.blendMode;

    if (track_.materialPaths.empty() && !track_.materialPath.empty())
    {
        track_.materialPaths.push_back(track_.materialPath);
    }
    if (!track_.materialPaths.empty())
    {
        track_.materialPath = track_.materialPaths.front();
    }
    if (track_.materialPaths.empty() && track_.materialPath.empty())
    {
        return false;
    }

    for (const std::string& materialPath_ : track_.materialPaths)
    {
        if (materialPath_.empty())
        {
            track_.materialResources.push_back(nullptr);
            track_.materialRandomColors.push_back({ 1.0f, 1.0f, 1.0f, 1.0f });
            continue;
        }
        std::shared_ptr<VFXMaterialResource> materialResource_ =
            VFXMaterialResourceManager::GetInstance().Load(materialPath_);
        if (!materialResource_)
        {
            track_.materialResources.push_back(nullptr);
            track_.materialRandomColors.push_back({ 1.0f, 1.0f, 1.0f, 1.0f });
            continue;
        }
        const VFXMaterialData& materialData_ = materialResource_->GetMaterialData();
        track_.materialResources.push_back(materialResource_);
        track_.materialRandomColors.push_back(VFXMaterialColorUtility::SampleRandomColor(
            materialData_.randomColor,
            BuildParticleMaterialRandomSeed(materialPath_, track_.name)));
        if (!track_.materialResource)
        {
            track_.materialResource = materialResource_;
        }
        if (materialData_.renderState.blendMode == VFX_MATERIAL_BLEND_MODE::ADDITIVE)
        {
            track_.runtimeBlendMode = ParticleBlendMode::ADDITIVE;
        }
    }
    if (!track_.materialResource)
    {
        return false;
    }

    const VFXMaterialData& materialData_ = track_.materialResource->GetMaterialData();
    track_.materialRandomSeed = BuildParticleMaterialRandomSeed(track_.materialPath, track_.name);
    track_.materialRandomColor = VFXMaterialColorUtility::SampleRandomColor(
        materialData_.randomColor,
        track_.materialRandomSeed);
    if (materialData_.renderState.blendMode == VFX_MATERIAL_BLEND_MODE::ADDITIVE)
    {
        track_.runtimeBlendMode = ParticleBlendMode::ADDITIVE;
    }

    return true;
}

bool ParticleComponent::ConvertEmitterToMaterial(EmitterTrack& track_)
{
    BS_PROFILE_SCOPE("ParticleComponent::ConvertEmitterToMaterial");

    const std::string defaultPath_ =
        "Data/VFXMaterials/" + (track_.name.empty() ? std::string{ "ParticleEmitter" } : track_.name) + ".vfxmat";
    const std::string selectedPath_ = WindowsDialogHelper::SaveVFXMaterialDialog(
        L"Convert Particle Emitter To VFX Material",
        defaultPath_,
        GraphicsDeviceManager::Instance().GetWindowHandle());
    if (selectedPath_.empty())
    {
        return false;
    }

    VFXMaterialData materialData_ = VFXMaterialConversionUtility::FromParticleEmitter(
        track_.settings,
        track_.name,
        track_.texturePath,
        track_.maskTexturePath,
        track_.blendMode);
    if (!VFXMaterialSerializer::Save(selectedPath_, materialData_))
    {
        return false;
    }

    if (track_.materialPaths.empty())
    {
        track_.materialPaths.push_back(selectedPath_);
    }
    else
    {
        track_.materialPaths.front() = selectedPath_;
    }
    track_.materialPath = selectedPath_;
    return LoadEmitterMaterial(track_, selectedPath_);
}

void ParticleComponent::ApplyEmitterMaterialToSettings(
    const EmitterTrack& track_,
    ParticleSystemConstants& settings_,
    float materialTime_) const
{
    BS_PROFILE_SCOPE("ParticleComponent::ApplyEmitterMaterialToSettings");

    if (track_.materialResources.empty() && !track_.materialResource)
    {
        return;
    }
    const size_t materialCount_ = track_.materialResources.empty() ? 1u : track_.materialResources.size();
    for (size_t materialIndex_ = 0; materialIndex_ < materialCount_; ++materialIndex_)
    {
        const std::shared_ptr<VFXMaterialResource>& materialResource_ =
            track_.materialResources.empty() ? track_.materialResource : track_.materialResources[materialIndex_];
        if (!materialResource_)
        {
            continue;
        }
        const VFXMaterialData& materialData_ = materialResource_->GetMaterialData();
        const DirectX::XMFLOAT4 randomColor_ =
            materialIndex_ < track_.materialRandomColors.size()
                ? track_.materialRandomColors[materialIndex_]
                : track_.materialRandomColor;
        const bool useRampLife_ =
            materialData_.colorRamp.enabled &&
            materialData_.colorRamp.sampleSource == VFX_MATERIAL_COLOR_RAMP_SAMPLE_SOURCE::LIFE;
        const DirectX::XMFLOAT4 manualRampColor_ = VFXMaterialColorUtility::SampleColorRamp(
            materialData_.colorRamp,
            0.5f,
            materialTime_);
        const DirectX::XMFLOAT4 startRampColor_ = VFXMaterialColorUtility::SampleColorRamp(
            materialData_.colorRamp,
            useRampLife_ ? 0.0f : 0.5f,
            materialTime_);
        const DirectX::XMFLOAT4 endRampColor_ = VFXMaterialColorUtility::SampleColorRamp(
            materialData_.colorRamp,
            useRampLife_ ? 1.0f : 0.5f,
            materialTime_);

        ApplyParticleMaterialColor(settings_.manualColor, materialData_, manualRampColor_, randomColor_);
        ApplyParticleMaterialColor(settings_.colorStart, materialData_, startRampColor_, randomColor_);
        ApplyParticleMaterialColor(settings_.colorEnd, materialData_, endRampColor_, randomColor_);
        if (settings_.useRandomColor != 0)
        {
            ApplyParticleMaterialColor(settings_.randomColorStart, materialData_, startRampColor_, randomColor_);
            ApplyParticleMaterialColor(settings_.randomColorEnd, materialData_, endRampColor_, randomColor_);
        }
        if (materialData_.colorRamp.enabled && useRampLife_)
        {
            settings_.useColorOverLife = 1u;
        }

        if (!materialData_.maskTexturePath.empty() && materialResource_->GetMaskTexture())
        {
            settings_.useMaskTexture = 1u;
        }
    }
}

void ParticleComponent::ApplyEmitterMaterialTextures(EmitterTrack& track_)
{
    BS_PROFILE_SCOPE("ParticleComponent::ApplyEmitterMaterialTextures");

    if (track_.materialResources.empty() && !track_.materialResource)
    {
        return;
    }
    const size_t materialCount_ = track_.materialResources.empty() ? 1u : track_.materialResources.size();
    for (size_t materialIndex_ = 0; materialIndex_ < materialCount_; ++materialIndex_)
    {
        const std::shared_ptr<VFXMaterialResource>& materialResource_ =
            track_.materialResources.empty() ? track_.materialResource : track_.materialResources[materialIndex_];
        if (!materialResource_)
        {
            continue;
        }
        const VFXMaterialData& materialData_ = materialResource_->GetMaterialData();

        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> baseTexture_;
        baseTexture_ = materialResource_->GetBaseTexture();
        if (baseTexture_)
        {
            if (track_.emitter)
            {
                track_.emitter->SetTexture(baseTexture_);
            }
            if (!materialData_.baseTexturePath.empty())
            {
                track_.texturePath = materialData_.baseTexturePath;
                track_.textureFileName = WindowsDialogHelper::PathToUtf8(
                    WindowsDialogHelper::Utf8ToPath(track_.texturePath).filename());
            }
        }

        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> maskTexture_;
        maskTexture_ = materialResource_->GetMaskTexture();
        if (maskTexture_)
        {
            if (track_.emitter)
            {
                track_.emitter->SetMaskTexture(maskTexture_);
            }
            if (!materialData_.maskTexturePath.empty())
            {
                track_.maskTexturePath = materialData_.maskTexturePath;
                track_.maskTextureFileName = WindowsDialogHelper::PathToUtf8(
                    WindowsDialogHelper::Utf8ToPath(track_.maskTexturePath).filename());
            }
        }
    }
}

void ParticleComponent::Stop()
{
    isPlaying = false;
    for (EmitterTrack& track_ : tracks)
    {
        track_.isPlaybackEnabled = false;
        if (track_.emitter)
        {
            track_.emitter->SetEmissionEnabled(false);
        }
    }
}

void ParticleComponent::ResetRuntime()
{
    BS_PROFILE_SCOPE("ParticleComponent::ResetRuntime");
    runtimeVisualScale = 1.0f;
    runtimeSizeScale = 1.0f;
    Stop();
    timer = 0.0f;
    for (EmitterTrack& track_ : tracks)
    {
        if (track_.hasBeamVisualOverride)
        {
            track_.settings = track_.beamVisualSettingsBackup;
            track_.spawnSpace = track_.beamVisualSpawnSpaceBackup;
            track_.delay = track_.beamVisualDelayBackup;
            track_.hasBeamVisualOverride = false;
        }
        track_.isStarted = false;
        if (!track_.emitter)
        {
            continue;
        }
        track_.emitter->ResetRuntime();
        track_.emitter->SetEmissionEnabled(false);
    }
}

bool ParticleComponent::HasRuntimeResources() const
{
    if (tracks.empty())
    {
        return false;
    }
    return std::all_of(
        tracks.begin(),
        tracks.end(),
        [](const EmitterTrack& track_)
        {
            return track_.emitter && track_.emitter->HasGPUResources();
        });
}

void ParticleComponent::RefreshTextureList()
{
    textureFiles.clear();
    std::string path = "Data/Texture/Particle/";
    if (!std::filesystem::exists(path))
    {
        return;
    }

    for (const auto& entry : std::filesystem::directory_iterator(path))
    {
        if (entry.is_regular_file())
        {
            textureFiles.push_back(entry.path().filename().string());
        }
    }
}

void ParticleComponent::RefreshAssetList()
{
    assetFiles.clear();
    std::string path_ = "Data/Particle/";
    if (!std::filesystem::exists(path_))
    {
        return;
    }

    for (const auto& entry_ : std::filesystem::directory_iterator(path_))
    {
        if (!entry_.is_regular_file())
        {
            continue;
        }
        if (entry_.path().extension() == ".json")
        {
            assetFiles.push_back(entry_.path().filename().string());
        }
    }
}


ParticleEffectAsset ParticleComponent::GetAsset() const
{
    // Component内部状態を外部保存用Asset形式へ詰め直す。
    ParticleEffectAsset asset_{};
    asset_.isPlaying = isPlaying;

    for (const EmitterTrack& track_ : tracks)
    {
        ParticleEmitterAsset emitterAsset_{};
        emitterAsset_.settings = track_.settings;
        emitterAsset_.name = track_.name;
        emitterAsset_.materialPath = track_.materialPath;
        emitterAsset_.materialPaths = track_.materialPaths;
        emitterAsset_.texturePath = track_.texturePath;
        emitterAsset_.textureFileName = track_.textureFileName;
        emitterAsset_.maskTexturePath = track_.maskTexturePath;
        emitterAsset_.maskTextureFileName = track_.maskTextureFileName;
        emitterAsset_.gradientTexturePath = track_.gradientTexturePath;
        emitterAsset_.gradientTextureFileName = track_.gradientTextureFileName;
        emitterAsset_.shadeGradientTexturePath = track_.shadeGradientTexturePath;
        emitterAsset_.shadeGradientTextureFileName = track_.shadeGradientTextureFileName;
        emitterAsset_.delay = track_.delay;
        emitterAsset_.meshVertexSampleCount = track_.meshVertexSampleCount;
        emitterAsset_.blendMode = track_.blendMode;
        emitterAsset_.spawnSpace = track_.spawnSpace;

        asset_.emitters.push_back(emitterAsset_);
    }

    return asset_;
}

void ParticleComponent::SetAsset(const ParticleEffectAsset& asset_)
{
    // AssetデータをComponent内部のEmitterTrackへ展開する。Runtimeリソースは後でRebuildする。
    Stop();

    tracks.clear();
    timer = 0.0f;
    isPlaying = false;

    for (const ParticleEmitterAsset& emitterAsset_ : asset_.emitters)
    {
        EmitterTrack track_{};
        track_.settings = emitterAsset_.settings;
        track_.name = emitterAsset_.name;
        track_.nameId = StringIdUtility::Hash(track_.name);
        track_.materialPath = emitterAsset_.materialPath;
        track_.materialPaths = emitterAsset_.materialPaths;
        if (track_.materialPaths.empty() && !track_.materialPath.empty())
        {
            track_.materialPaths.push_back(track_.materialPath);
        }
        if (track_.materialPath.empty() && !track_.materialPaths.empty())
        {
            track_.materialPath = track_.materialPaths.front();
        }
        track_.texturePath = emitterAsset_.texturePath;
        track_.textureFileName = emitterAsset_.textureFileName;
        track_.maskTexturePath = emitterAsset_.maskTexturePath;
        track_.maskTextureFileName = emitterAsset_.maskTextureFileName;
        track_.gradientTexturePath = emitterAsset_.gradientTexturePath;
        track_.gradientTextureFileName = emitterAsset_.gradientTextureFileName;
        track_.shadeGradientTexturePath = emitterAsset_.shadeGradientTexturePath;
        track_.shadeGradientTextureFileName = emitterAsset_.shadeGradientTextureFileName;
        track_.delay = emitterAsset_.delay;
        track_.meshVertexSampleCount = emitterAsset_.meshVertexSampleCount;
        track_.blendMode = emitterAsset_.blendMode;
        track_.spawnSpace = emitterAsset_.spawnSpace;
        track_.isPlaybackEnabled = false;
        track_.isStarted = false;

        tracks.push_back(std::move(track_));
    }

    RebuildEmitterResources();
    RegisterActions();
}

bool ParticleComponent::LoadAssetDataOnly(
    const std::string& path_,
    ParticleEffectAsset& asset_)
{
    // VFXManagerなどが一覧読み込みするため、Componentとして起動せずAsset情報だけ読む。
    if (path_.empty())
    {
        return false;
    }

    const std::filesystem::path filePath_ =
        WindowsDialogHelper::Utf8ToPath(path_);

    if (!std::filesystem::exists(filePath_))
    {
        return false;
    }

    std::ifstream file_(filePath_);
    if (!file_.is_open())
    {
        return false;
    }

    nlohmann::json json_;
    try
    {
        file_ >> json_;
    }
    catch (const nlohmann::json::exception&)
    {
        return false;
    }

    if (json_.contains("AssetType") &&
        json_.value("AssetType", std::string{}) == "ParticleEmitter")
    {
        return false;
    }

    ParticleComponent component_;
    component_.Deserialize(json_);
    if (component_.tracks.empty())
    {
        return false;
    }

    asset_ = component_.GetAsset();
    return true;
}

bool ParticleComponent::SaveAsset(const std::string& path_)
{
    // 編集中の全Emitterを1つのParticleEffect JSONとして保存する。
    if (path_.empty())
    {
        return false;
    }

    std::filesystem::path assetPath_ = path_;
    std::filesystem::create_directories(assetPath_.parent_path());

    std::string previousAssetPath_ = assetPath;
    assetPath = path_;

    nlohmann::json json_;
    Serialize(json_);
    json_["AssetType"] = "ParticleEffect";
    json_["AssetVersion"] = 1;

    std::ofstream file_(path_, std::ios::out | std::ios::trunc);
    if (!file_.is_open())
    {
        assetPath = previousAssetPath_;
        return false;
    }

    file_ << json_.dump(4);
    return true;
}

bool ParticleComponent::LoadAsset(const std::string& path_)
{
    // ParticleEffect JSONを読み込み、TextureやEmitterのGPUリソースを再構築する。
    if (path_.empty() || !std::filesystem::exists(WindowsDialogHelper::Utf8ToPath(path_)))
    {
        return false;
    }

    std::ifstream file_(WindowsDialogHelper::Utf8ToPath(path_));
    if (!file_.is_open())
    {
        return false;
    }

    nlohmann::json json_;
    file_ >> json_;
    Deserialize(json_);

    assetPath = path_;
    std::string assetName_ = std::filesystem::path(path_).stem().string();
    strncpy_s(assetNameBuffer, assetName_.c_str(), sizeof(assetNameBuffer) - 1);
    assetNameBuffer[sizeof(assetNameBuffer) - 1] = '\0';

    RebuildEmitterResources();
    return true;
}

bool ParticleComponent::SaveEmitterAsset(size_t emitterIndex_, const std::string& path_)
{
    // 選択中Emitterだけを単体JSONとして保存し、別Particleへ再利用できるようにする。
    if (path_.empty() || emitterIndex_ >= tracks.size())
    {
        return false;
    }

    nlohmann::json componentJson_;
    Serialize(componentJson_);
    if (!componentJson_.contains("Emitters") ||
        emitterIndex_ >= componentJson_["Emitters"].size())
    {
        return false;
    }

    const std::filesystem::path emitterPath_ = WindowsDialogHelper::Utf8ToPath(path_);
    std::filesystem::create_directories(emitterPath_.parent_path());

    nlohmann::json emitterJson_;
    emitterJson_["AssetType"] = "ParticleEmitter";
    emitterJson_["AssetVersion"] = 1;
    emitterJson_["Emitter"] = componentJson_["Emitters"][emitterIndex_];

    std::ofstream file_(emitterPath_, std::ios::out | std::ios::trunc);
    if (!file_.is_open())
    {
        return false;
    }

    file_ << emitterJson_.dump(4);
    return true;
}

bool ParticleComponent::LoadEmitterAsset(const std::string& path_)
{
    // 単体Emitter JSONを読み込み、現在のParticleComponentへ追加する。
    const std::filesystem::path emitterPath_ = WindowsDialogHelper::Utf8ToPath(path_);
    if (path_.empty() || !std::filesystem::exists(emitterPath_))
    {
        return false;
    }

    std::ifstream file_(emitterPath_);
    if (!file_.is_open())
    {
        return false;
    }

    nlohmann::json emitterAssetJson_;
    file_ >> emitterAssetJson_;
    if (emitterAssetJson_.contains("AssetType") &&
        emitterAssetJson_.value("AssetType", "") != "ParticleEmitter")
    {
        return false;
    }

    const nlohmann::json& emitterJson_ = emitterAssetJson_.contains("Emitter")
        ? emitterAssetJson_["Emitter"]
        : emitterAssetJson_;
    if (!emitterJson_.is_object())
    {
        return false;
    }

    nlohmann::json componentJson_;
    componentJson_["Emitters"] = nlohmann::json::array({ emitterJson_ });
    componentJson_["IsPlaying"] = false;

    ParticleComponent loadedComponent_;
    loadedComponent_.Deserialize(componentJson_);
    if (loadedComponent_.tracks.empty())
    {
        return false;
    }

    tracks.push_back(std::move(loadedComponent_.tracks.front()));
    RefreshEmitterNameIndex();
    RegisterActions();
    return true;
}

void ParticleComponent::Play()
{
    // 全Emitterを先頭から再生する。delayがないEmitterは即座に発生を開始する。
    isPlaying = true;
    timer = 0.0f;
    for (EmitterTrack& track_ : tracks)
    {
        track_.isPlaybackEnabled = true;
        track_.isStarted = track_.delay <= 0.0f;
        if (track_.emitter)
        {
            track_.emitter->SetEmissionEnabled(track_.isStarted);
        }
        if (track_.isStarted && track_.settings.emissionMode == 1)
        {
            track_.settings.burstTrigger = 1.0f;
        }
    }
}

bool ParticleComponent::PlayEmitter(const std::string& emitterName_)
{
    EmitterTrack* track_ = FindEmitterTrack(emitterName_);
    if (!track_)
    {
        return false;
    }

    isPlaying = true;
    track_->isPlaybackEnabled = true;
    track_->isStarted = true;
    if (track_->emitter)
    {
        track_->emitter->SetEmissionEnabled(true);
    }
    if (track_->settings.emissionMode == 1)
    {
        track_->settings.burstTrigger = 1.0f;
    }
    return true;
}

bool ParticleComponent::StopEmitter(const std::string& emitterName_)
{
    EmitterTrack* track_ = FindEmitterTrack(emitterName_);
    if (!track_)
    {
        return false;
    }

    track_->isPlaybackEnabled = false;
    if (track_->emitter)
    {
        track_->emitter->SetEmissionEnabled(false);
    }
    return true;
}

bool ParticleComponent::TriggerEmitter(const std::string& emitterName_)
{
    // 指定Emitterだけを即時再生する。Burstの場合はtriggerを立てて1回だけ発生させる。
    EmitterTrack* track_ = FindEmitterTrack(emitterName_);
    if (!track_)
    {
        return false;
    }

    isPlaying = true;
    track_->isPlaybackEnabled = true;
    track_->isStarted = true;
    if (track_->emitter)
    {
        track_->emitter->SetEmissionEnabled(true);
    }
    track_->settings.burstTrigger = 1.0f;
    return true;
}

bool ParticleComponent::SetEmitterPosition(
    const std::string& emitterName_,
    const DirectX::XMFLOAT3& position_,
    ParticleSpawnSpace spawnSpace_)
{
    // AnimationEventやVFXManagerから、Emitterの発生位置を直接差し替えるための入口。
    EmitterTrack* track_ = FindEmitterTrack(emitterName_);
    if (!track_)
    {
        return false;
    }

    track_->settings.emissionPosition = { position_.x, position_.y, position_.z, 1.0f };
    track_->spawnSpace = spawnSpace_;
    return true;
}

bool ParticleComponent::SetEmitterLine(
    const std::string& emitterName_,
    const DirectX::XMFLOAT3& startPosition_,
    const DirectX::XMFLOAT3& endPosition_)
{
    // BoneSegmentなどから線分上にParticleを出すため、対象EmitterをLine形状へ切り替える。
    auto applyLine_ = [&](EmitterTrack& track_)
    {
        track_.settings.spawnShape = 4;
        track_.settings.emissionPosition = { 0.0f, 0.0f, 0.0f, 1.0f };
        track_.settings.shapeStart = {
            startPosition_.x, startPosition_.y, startPosition_.z, 1.0f };
        track_.settings.shapeEnd = {
            endPosition_.x, endPosition_.y, endPosition_.z, 1.0f };
        track_.spawnSpace = ParticleSpawnSpace::WORLD;
    };

    if (!emitterName_.empty())
    {
        EmitterTrack* track_ = FindEmitterTrack(emitterName_);
        if (!track_)
        {
            return false;
        }

        applyLine_(*track_);
        return true;
    }

    bool changed_ = false;
    for (EmitterTrack& track_ : tracks)
    {
        applyLine_(track_);
        changed_ = true;
    }
    return changed_;
}

bool ParticleComponent::TriggerEmitterAtPosition(
    const std::string& emitterName_,
    const DirectX::XMFLOAT3& worldPosition_)
{
    return SetEmitterPosition(emitterName_, worldPosition_, ParticleSpawnSpace::WORLD) &&
        TriggerEmitter(emitterName_);
}

void ParticleComponent::SetEmissionScale(float scale_)
{
    emissionScale = std::clamp(scale_, 0.0f, 1.0f);
}

void ParticleComponent::SetRuntimeVisualScale(float scale_)
{
    runtimeVisualScale = std::clamp(scale_, 0.01f, 20.0f);
}

void ParticleComponent::SetRuntimeSizeScale(float scale_)
{
    runtimeSizeScale = std::clamp(scale_, 0.01f, 20.0f);
}

void ParticleComponent::SetRuntimeRingAppearance(float radius_, const DirectX::XMFLOAT4& color_)
{
    runtimeRingEnabled = true;
    runtimeRingRadius = (std::max)(radius_, 0.0f);
    runtimeRingColor = color_;
}

void ParticleComponent::Update(float deltaTime)
{
    BS_PROFILE_SCOPE("ParticleComponent::Update");
    // Component再生時間、Emitterのdelay、Owner相対座標、Mesh頂点発生をまとめて処理する。
    if (isPlaying)
    {
        timer += deltaTime;
    }

    auto context_ = GraphicsDeviceManager::Instance().GetContext();
    auto owner_ = GetGameObject();
    if (!owner_)
    {
        return;
    }

    const Vector3 worldPosition_ = owner_->transform->GetWorldPosition();
    float healthEmissionMultiplier_ = 1.0f;
    bool healthAllowsEmission_ = true;
    if (scaleEmissionByHealth)
    {
        const auto healthOwner_ = healthOwnerName.empty() ? owner_ : owner_->manager ? owner_->GetManager().Find(healthOwnerName) : nullptr;
        const auto health_ = healthOwner_ ? healthOwner_->GetComponent<HealthComponent>() : nullptr;
        if (health_ && health_->GetMaxHealth() > 0.0f)
        {
            const float missingHealthRate_ = 1.0f - std::clamp(health_->GetCurrentHealth() / health_->GetMaxHealth(), 0.0f, 1.0f);
            healthEmissionMultiplier_ = std::clamp(healthyEmissionMultiplier +
                (lowHealthEmissionMultiplier - healthyEmissionMultiplier) * missingHealthRate_, 0.0f, 10.0f);
            // 撃破後は新規発生だけを止め、既存の煙は寿命まで自然に消す。
            healthAllowsEmission_ = !health_->IsDead();
        }
    }


    for (EmitterTrack& track_ : tracks)
    {
        if (isPlaying && track_.isPlaybackEnabled && !track_.isStarted && timer >= track_.delay)
        {
            // delayが終わったEmitterだけを有効化し、Burstならこの瞬間に発火させる
            track_.isStarted = true;
            if (track_.emitter)
            {
                track_.emitter->SetEmissionEnabled(true);
            }
            if (track_.settings.emissionMode == 1)
            {
                track_.settings.burstTrigger = 1.0f;
            }
        }

        if (!track_.isStarted || !track_.emitter)
        {
            continue;
        }

        if (track_.settings.spawnShape == 5)
        {
            // Mesh Vertices発生はモデル姿勢が毎フレーム変わる可能性があるため、描画前に座標を更新する
            UpdateMeshSpawnPositions(track_);
        }

        ParticleSystemConstants tempSettings_ = track_.settings;
        // GPUへ渡す一時設定だけを拡大し、Pool再利用で倍率が累積することを防ぐ。
        tempSettings_.emissionSize.x *= runtimeVisualScale * runtimeSizeScale;
        tempSettings_.emissionSize.y *= runtimeVisualScale * runtimeSizeScale;
        tempSettings_.emissionSpeed.x *= runtimeVisualScale;
        tempSettings_.emissionSpeed.y *= runtimeVisualScale;
        tempSettings_.shapeSize.x *= runtimeVisualScale;
        tempSettings_.shapeSize.y *= runtimeVisualScale;
        tempSettings_.shapeSize.z *= runtimeVisualScale;
        if (!attractionTargetName.empty() && owner_->manager)
        {
            const auto target_ = owner_->GetManager().Find(attractionTargetName);
            if (target_ && target_->transform)
            {
                const auto position_ = target_->transform->GetWorldPosition();
                tempSettings_.targetPosition = {position_.x,position_.y,position_.z,1.0f};
            }
        }
        if (track_.spawnSpace == ParticleSpawnSpace::OWNER_RELATIVE)
        {
            // Owner Relativeは保存値をローカルオフセットとして扱い、実行時だけワールド位置へ変換する
            tempSettings_.emissionPosition.x = worldPosition_.x + track_.settings.emissionPosition.x;
            tempSettings_.emissionPosition.y = worldPosition_.y + track_.settings.emissionPosition.y;
            tempSettings_.emissionPosition.z = worldPosition_.z + track_.settings.emissionPosition.z;
        }

        ApplyEmitterMaterialToSettings(track_, tempSettings_, timer);
        if (runtimeRingEnabled)
        {
            tempSettings_.emissionOffset = { (std::max)(runtimeRingRadius - 0.5f, 0.0f), runtimeRingRadius };
            tempSettings_.manualColor = runtimeRingColor;
        }
        // 既存粒子は消さず、新しく発生する粒子数だけを環境遷移に合わせる。
        if (tempSettings_.emissionMode == 0)
        {
            tempSettings_.burstCount = static_cast<uint32_t>(tempSettings_.burstCount * emissionScale);
        }
        if (scaleEmissionByHealth)
        {
            // 環境の発生量補正とHP補正を合成し、補正後も発生要求の上限を守る。
            tempSettings_.burstCount = static_cast<uint32_t>((std::min)(static_cast<double>(tempSettings_.burstCount) * healthEmissionMultiplier_, 20000.0));
            track_.emitter->SetEmissionEnabled(isPlaying && track_.isPlaybackEnabled && healthAllowsEmission_);
        }
        track_.emitter->ApplySettings(tempSettings_);
        track_.emitter->SetDepthSortEnabled(track_.runtimeBlendMode == ParticleBlendMode::ALPHA);
        track_.emitter->SetDepthSortInterval(2);
        track_.emitter->Update(context_, deltaTime);

        if (track_.settings.burstTrigger > 0.0f)
        {
            track_.settings.burstTrigger = 0.0f;
        }
    }
}

void ParticleComponent::Serialize(nlohmann::json& j) const
{
    // Editorで調整した値を、Textureパスも含めて復元可能なJSONへ保存する
    nlohmann::json emitterList = nlohmann::json::array();

    for (const auto& track : tracks)
    {
        nlohmann::json ej;
        const auto& s = track.settings;

        ej["Name"] = track.name;
        ej["SpawnSpace"] = static_cast<int>(track.spawnSpace);
        ej["MaterialPath"] = track.materialPath;
        ej["MaterialPaths"] = track.materialPaths;
        ej["TexturePath"] = track.texturePath;
        ej["texturePath"] = track.texturePath;
        ej["MaskTexturePath"] = track.maskTexturePath;
        ej["maskTexturePath"] = track.maskTexturePath;
        ej["GradientTexturePath"] = track.gradientTexturePath;
        ej["gradientTexturePath"] = track.gradientTexturePath;
        ej["shadeGradientTexturePath"] = track.shadeGradientTexturePath;
        ej["Delay"] = track.delay;
        ej["MeshVertexSampleCount"] = track.meshVertexSampleCount;
        ej["MaxCount"] = s.maxParticleCount;
        ej["EmissionMode"] = s.emissionMode;
        ej["BurstCount"] = s.burstCount;
        ej["SpawnShape"] = s.spawnShape;
        ej["UseMask"] = s.useMaskTexture;
        ej["UseGradient"] = s.useGradientTexture;
        ej["useGradientTexture"] = s.useGradientTexture;
        ej["gradientSampleMode"] = s.gradientSampleMode;
        ej["gradientDirection"] = s.gradientDirection;
        ej["gradientFixedUv"] = s.gradientFixedUv;
        ej["gradientStartUv"] = s.gradientStartUv;
        ej["gradientEndUv"] = s.gradientEndUv;
        ej["useShadeGradientTexture"] = s.useShadeGradientTexture;
        ej["shadeGradientSampleMode"] = s.shadeGradientSampleMode;
        ej["shadeGradientDirection"] = s.shadeGradientDirection;
        ej["shadeGradientFixedUv"] = s.shadeGradientFixedUv;
        ej["shadeGradientStartUv"] = s.shadeGradientStartUv;
        ej["shadeGradientEndUv"] = s.shadeGradientEndUv;
        ej["PosOffset"] = { s.emissionPosition.x, s.emissionPosition.y, s.emissionPosition.z };
        ej["ShapeStart"] = { s.shapeStart.x, s.shapeStart.y, s.shapeStart.z };
        ej["ShapeEnd"] = { s.shapeEnd.x, s.shapeEnd.y, s.shapeEnd.z };
        ej["ShapeSize"] = { s.shapeSize.x, s.shapeSize.y, s.shapeSize.z };
        ej["LegacyRadius"] = { s.emissionOffset.x, s.emissionOffset.y };
        ej["Lifespan"] = { s.lifespan.x, s.lifespan.y };
        ej["Speed"] = { s.emissionSpeed.x, s.emissionSpeed.y };
        ej["ConeAngle"] = { s.emissionConeAngle.x, s.emissionConeAngle.y };
        ej["RotationSpeed"] = { s.emissionAngularSpeed.x, s.emissionAngularSpeed.y };
        ej["Gravity"] = s.gravity;
        ej["Wind"] = { s.windVelocity.x, s.windVelocity.y, s.windVelocity.z };
        ej["Acceleration"] = { s.windVelocity.x, s.windVelocity.y, s.windVelocity.z };
        ej["Drag"] = s.drag;
        ej["NoiseStrength"] = s.noiseStrength;
        ej["NoiseFrequency"] = s.noiseFrequency;
        ej["NoiseSpeed"] = s.noiseSpeed;
        ej["RadialVelocity"] = s.radialVelocity;
        ej["OrbitVelocity"] = s.orbitVelocity;
        ej["VerticalSpawnRange"] = s.verticalSpawnRange;
        ej["SpawnDirectionMode"] = s.spawnDirectionMode;
        ej["EmissionAxis"] = { s.emissionAxis.x, s.emissionAxis.y, s.emissionAxis.z };
        ej["TargetPosition"] = { s.targetPosition.x, s.targetPosition.y, s.targetPosition.z };
        ej["UseTargetAttraction"] = s.useTargetAttraction;
        ej["TargetAttractionStrength"] = s.targetAttractionStrength;
        ej["TargetAttractionRange"] = s.targetAttractionRange;
        ej["TargetAttractionFalloff"] = s.targetAttractionFalloff;
        ej["StretchScale"] = s.stretchScale;
        ej["StretchMax"] = s.stretchMax;
        ej["Color"] = { s.manualColor.x, s.manualColor.y, s.manualColor.z, s.manualColor.w };
        ej["ColorStart"] = { s.colorStart.x, s.colorStart.y, s.colorStart.z, s.colorStart.w };
        ej["ColorEnd"] = { s.colorEnd.x, s.colorEnd.y, s.colorEnd.z, s.colorEnd.w };
        ej["UseColorOverLife"] = s.useColorOverLife;
        ej["UseRandomColor"] = s.useRandomColor;
        ej["RandomColorStart"] = { s.randomColorStart.x, s.randomColorStart.y, s.randomColorStart.z, s.randomColorStart.w };
        ej["RandomColorEnd"] = { s.randomColorEnd.x, s.randomColorEnd.y, s.randomColorEnd.z, s.randomColorEnd.w };
        ej["Size"] = { s.emissionSize.x, s.emissionSize.y };
        ej["SizeRandomStart"] = { s.sizeRandomStart.x, s.sizeRandomStart.y };
        ej["SizeRandomEnd"] = { s.sizeRandomEnd.x, s.sizeRandomEnd.y };
        ej["UseRandomSize"] = s.useRandomSize;
        ej["Fade"] = { s.fadeDuration.x, s.fadeDuration.y };
        ej["TextureSplitCount"] = { s.textureSplitCount.x, s.textureSplitCount.y };
        ej["SpriteSheetIndex"] = { s.spriteSheetIndex.x, s.spriteSheetIndex.y };
        ej["UseSpriteAnimation"] = s.useSpriteAnimation;
        ej["SpriteAnimationLoop"] = s.spriteAnimationLoop;
        ej["SpriteAnimationRange"] = { s.spriteAnimationStartIndex, s.spriteAnimationEndIndex };
        ej["SpriteAnimationFps"] = s.spriteAnimationFps;
        ej["RenderType"] = s.type;
        ej["BlendMode"] = static_cast<int>(track.blendMode);
        ej["UseFresnel"] = s.useFresnel;
        ej["FresnelPower"] = s.fresnelPower;
        ej["FresnelStrength"] = s.fresnelStrength;

        emitterList.push_back(ej);
    }

    j["Emitters"] = emitterList;
    j["IsPlaying"] = isPlaying;
    j["ScaleEmissionByHealth"] = scaleEmissionByHealth;
    j["HealthOwnerName"] = healthOwnerName;
    j["HealthyEmissionMultiplier"] = healthyEmissionMultiplier;
    j["LowHealthEmissionMultiplier"] = lowHealthEmissionMultiplier;
    j["MeshSourceObjectName"] = meshSourceObjectName;
    j["AttractionTargetName"] = attractionTargetName;
    j["EmitDissolveBoundary"] = emitDissolveBoundary;
    j["DissolveBoundaryBand"] = dissolveBoundaryBand;
    j["AssetPath"] = assetPath;
}

void ParticleComponent::Deserialize(const nlohmann::json& j)
{
    // JSONからEmitterTrackを復元する古いキーも読めるように一部互換処理を残す
    tracks.clear();
    assetPath = j.value("AssetPath", assetPath);
    scaleEmissionByHealth = j.value("ScaleEmissionByHealth", false);
    healthOwnerName = j.value("HealthOwnerName", std::string{});
    healthyEmissionMultiplier = std::clamp(j.value("HealthyEmissionMultiplier", 1.0f), 0.0f, 10.0f);
    lowHealthEmissionMultiplier = std::clamp(j.value("LowHealthEmissionMultiplier", 3.0f), 0.0f, 10.0f);
    meshSourceObjectName = j.value("MeshSourceObjectName", std::string{});
    attractionTargetName = j.value("AttractionTargetName", std::string{});
    emitDissolveBoundary = j.value("EmitDissolveBoundary", false);
    dissolveBoundaryBand = (std::max)(j.value("DissolveBoundaryBand", 0.15f),0.01f);

    if (j.contains("Emitters"))
    {
        for (const auto& ej : j["Emitters"])
        {
            EmitterTrack track;
            track.name = ej.value("Name", "Emitter_" + std::to_string(tracks.size()));
            track.nameId = StringIdUtility::Hash(track.name);
            track.spawnSpace = static_cast<ParticleSpawnSpace>(
                std::clamp(ej.value("SpawnSpace", 0), 0, 1));
            auto& s = track.settings;

            track.texturePath = ej.value(
                "texturePath", ej.value("TexturePath", ej.value("texture_path", std::string{})));
            track.materialPath = ej.value("MaterialPath", std::string{});
            if (ej.contains("MaterialPaths") && ej["MaterialPaths"].is_array())
            {
                for (const auto& materialPathJson_ : ej["MaterialPaths"])
                {
                    track.materialPaths.push_back(materialPathJson_.get<std::string>());
                }
            }
            if (track.materialPaths.empty() && !track.materialPath.empty())
            {
                track.materialPaths.push_back(track.materialPath);
            }
            if (track.materialPath.empty() && !track.materialPaths.empty())
            {
                track.materialPath = track.materialPaths.front();
            }
            track.maskTexturePath = ej.value(
                "maskTexturePath", ej.value("MaskTexturePath", std::string{}));
            track.gradientTexturePath = ej.value(
                "gradientTexturePath",
                ej.value("GradientTexturePath", ej.value("color_temper_path", std::string{})));
            track.shadeGradientTexturePath = ej.value("shadeGradientTexturePath", std::string{});
            if (!track.texturePath.empty())
            {
                track.textureFileName = std::filesystem::path(track.texturePath).filename().string();
            }
            if (!track.maskTexturePath.empty())
            {
                track.maskTextureFileName = std::filesystem::path(track.maskTexturePath).filename().string();
            }
            if (!track.gradientTexturePath.empty())
            {
                track.gradientTextureFileName = std::filesystem::path(track.gradientTexturePath).filename().string();
            }
            if (!track.shadeGradientTexturePath.empty())
            {
                track.shadeGradientTextureFileName = std::filesystem::path(track.shadeGradientTexturePath).filename().string();
            }

            track.delay = ej.value("Delay", 0.0f);
            track.meshVertexSampleCount = ej.value("MeshVertexSampleCount", 2048u);
            s.maxParticleCount = ej.value("MaxCount", 1000u);
            s.emissionMode = ej.value("EmissionMode", 0u);
            s.burstCount = ej.value("BurstCount", 100u);
            s.spawnShape = ej.value("SpawnShape", 0u);
            s.useMaskTexture = ej.value("UseMask", 0u);
            s.useGradientTexture = ej.value("useGradientTexture", ej.value("UseGradient", 0u));
            s.gradientSampleMode = ej.value("gradientSampleMode", static_cast<uint32_t>(PARTICLE_GRADIENT_SAMPLE_MODE_HORIZONTAL));
            s.gradientDirection = ej.value("gradientDirection", static_cast<uint32_t>(PARTICLE_GRADIENT_DIRECTION_FORWARD));
            s.gradientFixedUv = ej.value("gradientFixedUv", 0.5f);
            s.gradientStartUv = ej.value("gradientStartUv", 0.0f);
            s.gradientEndUv = ej.value("gradientEndUv", 1.0f);
            s.useShadeGradientTexture = ej.value("useShadeGradientTexture", 0u);
            s.shadeGradientSampleMode = ej.value("shadeGradientSampleMode", static_cast<uint32_t>(PARTICLE_GRADIENT_SAMPLE_MODE_HORIZONTAL));
            s.shadeGradientDirection = ej.value("shadeGradientDirection", static_cast<uint32_t>(PARTICLE_GRADIENT_DIRECTION_FORWARD));
            s.shadeGradientFixedUv = ej.value("shadeGradientFixedUv", 0.5f);
            s.shadeGradientStartUv = ej.value("shadeGradientStartUv", 0.0f);
            s.shadeGradientEndUv = ej.value("shadeGradientEndUv", 1.0f);
            s.gravity = ej.value("Gravity", -9.8f);
            if (ej.contains("Acceleration"))
            {
                s.windVelocity = { ej["Acceleration"][0], ej["Acceleration"][1], ej["Acceleration"][2], 0.0f };
            }
            else if (ej.contains("Wind"))
            {
                s.windVelocity = { ej["Wind"][0], ej["Wind"][1], ej["Wind"][2], 0.0f };
            }
            s.drag = ej.value("Drag", 0.0f);
            s.noiseStrength = ej.value("NoiseStrength", ej.value("NoiseScale", 0.0f));
            s.noiseScale = s.noiseStrength;
            s.noiseFrequency = ej.value("NoiseFrequency", 1.0f);
            s.noiseSpeed = ej.value("NoiseSpeed", 1.0f);
            s.radialVelocity = ej.value("RadialVelocity", 0.0f);
            s.orbitVelocity = ej.value("OrbitVelocity", 0.0f);
            s.verticalSpawnRange = ej.value("VerticalSpawnRange", 0.0f);
            s.spawnDirectionMode = static_cast<uint32_t>(
                std::clamp(ej.value("SpawnDirectionMode", 0), 0, 3));
            if (ej.contains("EmissionAxis"))
            {
                s.emissionAxis = { ej["EmissionAxis"][0], ej["EmissionAxis"][1], ej["EmissionAxis"][2], 0.0f };
            }
            if (ej.contains("TargetPosition"))
            {
                s.targetPosition = { ej["TargetPosition"][0], ej["TargetPosition"][1], ej["TargetPosition"][2], 1.0f };
            }
            s.useTargetAttraction = ej.value("UseTargetAttraction", 0u);
            s.targetAttractionStrength = ej.value("TargetAttractionStrength", 0.0f);
            s.targetAttractionRange = ej.value("TargetAttractionRange", 0.0f);
            s.targetAttractionFalloff = (std::max)(ej.value("TargetAttractionFalloff", 1.0f), 0.001f);
            s.stretchScale = ej.value("StretchScale", 0.04f);
            s.stretchMax = ej.value("StretchMax", 100.0f);
            s.type = ej.value("RenderType", 0u);
            track.blendMode = static_cast<ParticleBlendMode>(
                std::clamp(ej.value("BlendMode", 0), 0, 1));
            s.useFresnel = ej.value("UseFresnel", 0u);
            s.fresnelPower = ej.value("FresnelPower", 2.0f);
            s.fresnelStrength = ej.value("FresnelStrength", 1.0f);

            if (ej.contains("PosOffset"))
            {
                s.emissionPosition = { ej["PosOffset"][0], ej["PosOffset"][1], ej["PosOffset"][2], 1.0f };
            }
            if (ej.contains("ShapeStart"))
            {
                s.shapeStart = { ej["ShapeStart"][0], ej["ShapeStart"][1], ej["ShapeStart"][2], 1.0f };
            }
            if (ej.contains("ShapeEnd"))
            {
                s.shapeEnd = { ej["ShapeEnd"][0], ej["ShapeEnd"][1], ej["ShapeEnd"][2], 1.0f };
            }
            if (ej.contains("ShapeSize"))
            {
                s.shapeSize = { ej["ShapeSize"][0], ej["ShapeSize"][1], ej["ShapeSize"][2], 0.0f };
            }
            if (ej.contains("LegacyRadius"))
            {
                s.emissionOffset = { ej["LegacyRadius"][0], ej["LegacyRadius"][1] };
            }
            if (ej.contains("Lifespan"))
            {
                s.lifespan = { ej["Lifespan"][0], ej["Lifespan"][1] };
            }
            if (ej.contains("Speed"))
            {
                s.emissionSpeed = { ej["Speed"][0], ej["Speed"][1] };
            }
            if (ej.contains("ConeAngle"))
            {
                s.emissionConeAngle = { ej["ConeAngle"][0], ej["ConeAngle"][1] };
            }
            if (ej.contains("RotationSpeed"))
            {
                s.emissionAngularSpeed = { ej["RotationSpeed"][0], ej["RotationSpeed"][1] };
            }
            if (ej.contains("Color"))
            {
                s.manualColor = { ej["Color"][0], ej["Color"][1], ej["Color"][2], ej["Color"][3] };
            }
            if (ej.contains("ColorStart"))
            {
                s.colorStart = { ej["ColorStart"][0], ej["ColorStart"][1], ej["ColorStart"][2], ej["ColorStart"][3] };
            }
            else
            {
                s.colorStart = s.manualColor;
            }
            if (ej.contains("ColorEnd"))
            {
                s.colorEnd = { ej["ColorEnd"][0], ej["ColorEnd"][1], ej["ColorEnd"][2], ej["ColorEnd"][3] };
            }
            else
            {
                s.colorEnd = s.manualColor;
            }
            s.useColorOverLife = ej.value("UseColorOverLife", 0u);
            s.useRandomColor = ej.value("UseRandomColor", 0u);
            if (ej.contains("RandomColorStart"))
            {
                s.randomColorStart = { ej["RandomColorStart"][0], ej["RandomColorStart"][1],
                    ej["RandomColorStart"][2], ej["RandomColorStart"][3] };
            }
            if (ej.contains("RandomColorEnd"))
            {
                s.randomColorEnd = { ej["RandomColorEnd"][0], ej["RandomColorEnd"][1],
                    ej["RandomColorEnd"][2], ej["RandomColorEnd"][3] };
            }
            if (ej.contains("Size"))
            {
                s.emissionSize = { ej["Size"][0], ej["Size"][1] };
            }
            if (ej.contains("SizeRandomStart"))
            {
                s.sizeRandomStart = { ej["SizeRandomStart"][0], ej["SizeRandomStart"][1] };
            }
            if (ej.contains("SizeRandomEnd"))
            {
                s.sizeRandomEnd = { ej["SizeRandomEnd"][0], ej["SizeRandomEnd"][1] };
            }
            s.useRandomSize = ej.value("UseRandomSize", 0u);
            if (ej.contains("Fade"))
            {
                s.fadeDuration = { ej["Fade"][0], ej["Fade"][1] };
            }
            if (ej.contains("TextureSplitCount"))
            {
                s.textureSplitCount = { ej["TextureSplitCount"][0], ej["TextureSplitCount"][1] };
            }
            if (ej.contains("SpriteSheetIndex"))
            {
                s.spriteSheetIndex = { ej["SpriteSheetIndex"][0], ej["SpriteSheetIndex"][1] };
            }
            s.useSpriteAnimation = ej.value("UseSpriteAnimation", 0u);
            s.spriteAnimationLoop = ej.value("SpriteAnimationLoop", 1u);
            if (ej.contains("SpriteAnimationRange"))
            {
                s.spriteAnimationStartIndex = ej["SpriteAnimationRange"][0];
                s.spriteAnimationEndIndex = ej["SpriteAnimationRange"][1];
            }
            s.spriteAnimationFps = ej.value("SpriteAnimationFps", 12.0f);

            tracks.push_back(std::move(track));
        }
    }

    RebuildEmitterResources();

    if (j.value("IsPlaying", false))
    {
        Play();
    }
}

void ParticleComponent::UpdateMeshSpawnPositions(EmitterTrack& track_)
{
    BS_PROFILE_SCOPE("ParticleComponent::UpdateMeshSpawnPositions");
    // 現在のGltfModel頂点をワールド座標へ変換し、Particleの発生候補点としてEmitterへ渡す
    if (!track_.emitter)
    {
        return;
    }

    auto owner_ = GetGameObject();
    if (owner_ && owner_->manager && !meshSourceObjectName.empty())
    {
        const auto source_ = owner_->GetManager().Find(meshSourceObjectName);
        const auto model_ = source_ ? source_->GetComponent<GltfModelCommponennt>() : nullptr;
        auto& points_ = track_.emitter->GetMeshSpawnPositions();
        if (!model_)
        {
            points_.clear();
            return;
        }
        model_->CollectWorldVertexSamples(points_, track_.meshVertexSampleCount);
        if (emitDissolveBoundary)
        {
            if (const auto dissolve_ = source_->GetComponent<ModelDissolveComponent>())
            {
                const auto& settings_ = dissolve_->GetSettings();
                const auto origin_ = source_->transform->GetWorldPosition();
                const float length_ = std::sqrt(settings_.direction.x*settings_.direction.x+settings_.direction.y*settings_.direction.y+settings_.direction.z*settings_.direction.z);
                const float extent_ = (std::max)(settings_.directionExtent*length_,0.001f);
                // GPUのNoiseを読み戻さず、方向分布の近似境界から崩壊粒子を発生させる。
                points_.erase(std::remove_if(points_.begin(),points_.end(),[&](const DirectX::XMFLOAT3& point_)
                {
                    const float height_ = ((point_.x-origin_.x)*settings_.direction.x+(point_.y-origin_.y)*settings_.direction.y+(point_.z-origin_.z)*settings_.direction.z)/extent_;
                    return std::abs(height_-dissolve_->GetProgress()) > dissolveBoundaryBand;
                }),points_.end());
            }
        }
        return;
    }
    auto model_ = owner_ ? owner_->GetComponent<GltfModelCommponennt>() : nullptr;
    if (!owner_ || !owner_->transform || !model_)
    {
        track_.emitter->GetMeshSpawnPositions().clear();
        return;
    }

    size_t totalVertexCount_ = 0;
    for (const auto& mesh_ : model_->GetMeshes())
    {
        totalVertexCount_ += mesh_.vertices.size();
    }

    if (totalVertexCount_ == 0)
    {
        track_.emitter->GetMeshSpawnPositions().clear();
        return;
    }

    const DirectX::XMMATRIX ownerWorld_ = owner_->transform->GetWorldMatrix();

    const DirectX::XMVECTOR emitterOffset_ = DirectX::XMVectorSet(
        track_.settings.emissionPosition.x,
        track_.settings.emissionPosition.y,
        track_.settings.emissionPosition.z,
        0.0f);
    const DirectX::XMVECTOR rotatedOffset_ =
        DirectX::XMVector3TransformNormal(emitterOffset_, ownerWorld_);

    const size_t sampleLimit_ = (std::max)(static_cast<size_t>(1), static_cast<size_t>(track_.meshVertexSampleCount));
    const size_t sampleStride_ = (std::max)(static_cast<size_t>(1), totalVertexCount_ / sampleLimit_);

    std::vector<DirectX::XMFLOAT3>& positions_ = track_.emitter->GetMeshSpawnPositions();
    positions_.clear();
    positions_.reserve((std::min)(totalVertexCount_, sampleLimit_));

    size_t vertexIndex_ = 0;
    for (const auto& mesh_ : model_->GetMeshes())
    {
        const DirectX::XMMATRIX meshLocal_ = mesh_.node
            ? DirectX::XMLoadFloat4x4(&mesh_.node->globalTransform)
            : DirectX::XMMatrixIdentity();

        const DirectX::XMMATRIX finalWorld_ = meshLocal_ * ownerWorld_;

        for (const auto& vertex_ : mesh_.vertices)
        {
            if ((vertexIndex_++ % sampleStride_) != 0)
            {
                continue;
            }

            DirectX::XMVECTOR pos_ = DirectX::XMLoadFloat3(&vertex_.position);
            pos_ = DirectX::XMVector3TransformCoord(pos_, finalWorld_);

            pos_ = DirectX::XMVectorAdd(pos_, rotatedOffset_);

            DirectX::XMFLOAT3 out_{};
            DirectX::XMStoreFloat3(&out_, pos_);
            positions_.push_back(out_);

            if (positions_.size() >= sampleLimit_)
            {
                break;
            }
        }

        if (positions_.size() >= sampleLimit_)
        {
            break;
        }
    }

}


void ParticleComponent::RebuildEmitterResources()
{
    // 設定変更後はGPUバッファとTexture SRVを作り直し、Emitterへ最新設定を反映する
    auto device_ = GraphicsDeviceManager::Instance().GetDevice();
    RefreshEmitterNameIndex();
    if (!device_)
    {
        return;
    }

    RefreshTextureList();

    for (auto& track_ : tracks)
    {
        const uint32_t useMaskTexture_ = track_.settings.useMaskTexture;
        const uint32_t useGradientTexture_ = track_.settings.useGradientTexture;
        const uint32_t useShadeGradientTexture_ = track_.settings.useShadeGradientTexture;
        track_.runtimeBlendMode = track_.blendMode;
        track_.nameId = StringIdUtility::Hash(track_.name);
        track_.emitter = std::make_unique<ParticleEmitter>(device_, track_.settings, track_.settings.maxParticleCount);
        if (!track_.materialPath.empty() || !track_.materialPaths.empty())
        {
            LoadEmitterMaterialResource(track_, track_.materialPath);
            ApplyEmitterMaterialTextures(track_);
        }
        if (!track_.texturePath.empty())
        {
            LoadEmitterTextureSlot(track_, track_.texturePath, 0);
        }
        if (!track_.maskTexturePath.empty())
        {
            LoadEmitterTextureSlot(track_, track_.maskTexturePath, 1);
        }
        if (!track_.gradientTexturePath.empty())
        {
            LoadEmitterTextureSlot(track_, track_.gradientTexturePath, 2);
        }
        if (!track_.shadeGradientTexturePath.empty())
        {
            LoadEmitterTextureSlot(track_, track_.shadeGradientTexturePath, 3);
        }
        track_.settings.useMaskTexture = useMaskTexture_;
        track_.settings.useGradientTexture = useGradientTexture_;
        track_.settings.useShadeGradientTexture = useShadeGradientTexture_;

        ParticleSystemConstants evaluatedSettings_ = track_.settings;
        ApplyEmitterMaterialToSettings(track_, evaluatedSettings_, timer);
        track_.emitter->ApplySettings(evaluatedSettings_);
        track_.emitter->SetDepthSortEnabled(track_.runtimeBlendMode == ParticleBlendMode::ALPHA);
        track_.emitter->SetEmissionEnabled(track_.isPlaybackEnabled);
    }
    RegisterActions();
}

void ParticleComponent::RefreshEmitterNameIndex()
{
    // Emitter名からTrackを引くための索引を、編集やAsset読み込み後に作り直す。
    emitterTrackIndexByNameId.clear();
    for (size_t index_ = 0; index_ < tracks.size(); ++index_)
    {
        tracks[index_].nameId = StringIdUtility::Hash(tracks[index_].name);
        emitterTrackIndexByNameId.emplace(tracks[index_].nameId, index_);
    }
}

std::string ParticleComponent::BuildAssetPathFromName() const
{
    std::string name_ = assetNameBuffer;
    if (name_.empty())
    {
        name_ = "NewParticle";
    }

    for (char& char_ : name_)
    {
        if (char_ == '\\' || char_ == '/' || char_ == ':' || char_ == '*' || char_ == '?' ||
            char_ == '"' || char_ == '<' || char_ == '>' || char_ == '|')
        {
            char_ = '_';
        }
    }

    return "Data/Particle/" + name_ + ".json";
}

ParticleComponent::EmitterTrack* ParticleComponent::FindEmitterTrack(const std::string& emitterName_)
{
    const auto indexIterator_ = emitterTrackIndexByNameId.find(StringIdUtility::Hash(emitterName_));
    if (indexIterator_ == emitterTrackIndexByNameId.end() ||
        indexIterator_->second >= tracks.size())
    {
        return nullptr;
    }
    return &tracks[indexIterator_->second];
}

const ParticleComponent::EmitterTrack* ParticleComponent::FindEmitterTrack(const std::string& emitterName_) const
{
    const auto indexIterator_ = emitterTrackIndexByNameId.find(StringIdUtility::Hash(emitterName_));
    if (indexIterator_ == emitterTrackIndexByNameId.end() ||
        indexIterator_->second >= tracks.size())
    {
        return nullptr;
    }
    return &tracks[indexIterator_->second];
}

std::string ParticleComponent::BuildEmitterAssetPathFromName(size_t emitterIndex_) const
{
    std::string name_ = emitterIndex_ < tracks.size() ? tracks[emitterIndex_].name : "Emitter";
    if (name_.empty())
    {
        name_ = "Emitter";
    }

    for (char& char_ : name_)
    {
        if (char_ == '\\' || char_ == '/' || char_ == ':' || char_ == '*' || char_ == '?' ||
            char_ == '"' || char_ == '<' || char_ == '>' || char_ == '|')
        {
            char_ = '_';
        }
    }

    return "Data/Particle/Emitter/" + name_ + ".json";
}


#include "ParticleComponent.h"


#include "Engine/Graphics/Core/CPUResourceLoader.h"
#include "Engine/Platform/WindowsDialogHelper.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Graphics/Resource/GlTFModel.h"
#include "Engine/GamePlay/Serialize/ActionRegistry.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>

void ParticleComponent::Start()
{
    RegisterActions();
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
    auto gameObject_ = GetGameObject();
    if (!gameObject_) return;

    std::string objectName_ = gameObject_->GetName();
    auto& registry_ = gameObject_->GetLocalRegistry();
    registry_.RegisterAction("Particle", objectName_ + "_Particle_Play", [this]() { Play(); });
    registry_.RegisterAction("Particle", objectName_ + "_Particle_Stop", [this]() { Stop(); });
}

void ParticleComponent::Gui()
{
    if (textureDialogKey.empty())
    {
        textureDialogKey = "ParticleTextureBrowser_" + std::to_string(reinterpret_cast<uintptr_t>(this));
    }

    RefreshAssetList();

    ImGui::SeparatorText("Particle Asset");
    ImGui::InputText("Asset Name", assetNameBuffer, IM_ARRAYSIZE(assetNameBuffer));
    ImGui::Text("Current: %s", assetPath.empty() ? "Unsaved" : assetPath.c_str());

    if (ImGui::Button("Save Asset"))
    {
        SaveAsset(BuildAssetPathFromName());
        RefreshAssetList();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload Asset") && !assetPath.empty())
    {
        LoadAsset(assetPath);
    }

    const char* assetPreview_ = selectedAssetIndex >= 0 && selectedAssetIndex < static_cast<int>(assetFiles.size())
        ? assetFiles[selectedAssetIndex].c_str()
        : "Select Particle Asset";
    if (ImGui::BeginCombo("Load From Data/Particle", assetPreview_))
    {
        for (int assetIndex_ = 0; assetIndex_ < static_cast<int>(assetFiles.size()); ++assetIndex_)
        {
            bool selected_ = selectedAssetIndex == assetIndex_;
            if (ImGui::Selectable(assetFiles[assetIndex_].c_str(), selected_))
            {
                selectedAssetIndex = assetIndex_;
                LoadAsset("Data/Particle/" + assetFiles[assetIndex_]);
            }
        }
        ImGui::EndCombo();
    }

    ImGui::SeparatorText("Playback");

    if (ImGui::Button("Play Effect")) Play();
    ImGui::SameLine();
    if (ImGui::Button("Stop")) Stop();
    ImGui::SameLine();
    if (ImGui::Button("Reset All"))
    {
        for (auto& track : tracks) track.Reset();
    }

    ImGui::Separator();

    if (ImGui::Button("Add Emitter"))
    {
        EmitterTrack newTrack;
        newTrack.settings = ParticleSystemConstants();
        newTrack.settings.maxParticleCount = 1000;
        newTrack.settings.burstCount = 100;
        newTrack.settings.shapeSize = { 1, 1, 1, 0 };
        newTrack.emitter = std::make_unique<ParticleEmitter>(
            GraphicsDeviceManager::Instance().GetDevice(),
            newTrack.settings,
            newTrack.settings.maxParticleCount);
        tracks.push_back(std::move(newTrack));
    }

    if (tracks.empty())
    {
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "No Emitters. Click 'Add Emitter'.");
        return;
    }

    if (selectedEmitterIndex >= static_cast<int>(tracks.size())) selectedEmitterIndex = static_cast<int>(tracks.size()) - 1;
    if (selectedEmitterIndex < 0) selectedEmitterIndex = 0;

    ImGui::Text("Emitter List");
    ImGui::BeginChild("EmitterList", ImVec2(160, 96), true);
    for (int i = 0; i < static_cast<int>(tracks.size()); ++i)
    {
        ImGui::PushID(i);
        char label[32];
        sprintf_s(label, "Emitter %d", i);
        if (ImGui::Selectable(label, selectedEmitterIndex == i)) selectedEmitterIndex = i;
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::SameLine();

    ImGui::BeginGroup();
    bool shouldRemove = ImGui::Button("Remove Selected");
    if (ImGui::Button("Refresh Textures")) RefreshTextureList();
    ImGui::EndGroup();

    if (shouldRemove)
    {
        tracks.erase(tracks.begin() + selectedEmitterIndex);
        if (selectedEmitterIndex > 0) --selectedEmitterIndex;
        return;
    }

    ImGui::Separator();

    ImGui::PushID(selectedEmitterIndex);
    auto& track = tracks[selectedEmitterIndex];
    auto& s = track.settings;

    bool openTextureBrowser_ = false;

    auto textureCombo = [&](const char* label, std::string& fileName, std::string& path, int slot)
    {
        const char* preview = fileName.empty() ? "None" : fileName.c_str();
        if (ImGui::BeginCombo(label, preview))
        {
            if (ImGui::Selectable("None", fileName.empty()))
            {
                fileName.clear();
                path.clear();
                if (track.emitter)
                {
                    auto device_ = GraphicsDeviceManager::Instance().GetDevice();
                    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> dummySRV_;
                    CPUResourceLoader::CreateDummyTexture(device_, 0xFFFFFFFFu, dummySRV_.GetAddressOf());

                    if (slot == 0) { track.emitter->SetTexture(dummySRV_); }
                    else if (slot == 1) { track.emitter->SetMaskTexture(dummySRV_); s.useMaskTexture = 0; }
                    else if (slot == 2) { track.emitter->SetGradientTexture(dummySRV_); s.useGradientTexture = 0; }
                }
            }

            for (const auto& file : textureFiles)
            {
                bool selected = fileName == file;
                if (ImGui::Selectable(file.c_str(), selected))
                {
                    fileName = file;
                    path = "Data/Texture/Particle/" + file;
                    LoadEmitterTextureSlot(track, path, slot);
                    if (slot == 1) s.useMaskTexture = 1;
                    if (slot == 2) s.useGradientTexture = 1;
                }
            }
            ImGui::EndCombo();
        }

        ID3D11ShaderResourceView* srv_ = nullptr;
        if (track.emitter)
        {
            if (slot == 0) srv_ = track.emitter->GetTextureSRV();
            else if (slot == 1) srv_ = track.emitter->GetMaskTextureSRV();
            else if (slot == 2) srv_ = track.emitter->GetGradientTextureSRV();
        }

        ImGui::PushID(slot);
        bool browsePressed_ = false;
        if (srv_)
        {
            browsePressed_ = ImGui::ImageButton("##TextureBrowse", reinterpret_cast<ImTextureID>(srv_), ImVec2(64.0f, 64.0f));
        }
        else
        {
            browsePressed_ = ImGui::Button("Select Texture", ImVec2(96.0f, 64.0f));
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Browse %s", label);
        }
        ImGui::PopID();

        if (browsePressed_)
        {
            pendingTextureSlot = slot;

            const wchar_t* title_ =
                slot == 0 ? L"Select Color Texture" :
                slot == 1 ? L"Select Mask Texture" :
                L"Select Gradient Texture";

            const std::string initDir_ = path.empty()
                ? "Data/Texture/Particle/"
                : std::filesystem::path(path).parent_path().generic_string();

            // モーダルダイアログを開く（この間、メインループは一時停止する）
            const std::string selected_ = WindowsDialogHelper::OpenTextureDialog(title_, initDir_);

            if (!selected_.empty())
            {
                // 【重要】いきなりEmitterや現在のパスを破壊せず、読み込みが成功するかを見極める
                // ※LoadEmitterTextureSlotの内部で、失敗時に古いテクスチャを維持するか、
                //   あるいは完全に安全な状態（nullptr等）にクリアされている必要があります。
                if (LoadEmitterTextureSlot(track, selected_, slot))
                {
                    // 読み込みが完全に成功した時のみ、聖なる碑文（パスと名前）を書き換える
                    fileName = std::filesystem::path(selected_).filename().string();
                    path = selected_;
                    if (slot == 1) s.useMaskTexture = 1;
                    if (slot == 2) s.useGradientTexture = 1;
                }
                else
                {
                    // 【対策】もし読み込みに失敗した場合、EmitterのSRVが nullptr になっている可能性が高い！
                    // その場合、次のフレームの ImGui::ImageButton でクラッシュするのを防ぐため、
                    // ここで安全なダミーテクスチャを再設定するか、fileNameを"None"に戻すなどの救済措置を行う。
                    fileName.clear();
                    path.clear();
                    if (slot == 1) s.useMaskTexture = 0;
                    if (slot == 2) s.useGradientTexture = 0;

                    // 安全なダミーを割り当てる、あるいはnullptrのままにしてImageButton側で弾く
                    if (track.emitter)
                    {
                        auto device_ = GraphicsDeviceManager::Instance().GetDevice();
                        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> dummySRV_;
                        CPUResourceLoader::CreateDummyTexture(device_, 0xFFFFFFFFu, dummySRV_.GetAddressOf());

                        if (slot == 0) { track.emitter->SetTexture(dummySRV_); }
                        else if (slot == 1) { track.emitter->SetMaskTexture(dummySRV_); }
                        else if (slot == 2) { track.emitter->SetGradientTexture(dummySRV_); }
                    }
                }
            }
            pendingTextureSlot = -1;
        }
    };

    if (ImGui::BeginTabBar("EmitterParams"))
    {
        if (ImGui::BeginTabItem("Emission"))
        {
            int maxCount = static_cast<int>(s.maxParticleCount);
            if (ImGui::SliderInt("Max Count", &maxCount, 1, 20000)) s.maxParticleCount = static_cast<uint32_t>(maxCount);
            ImGui::DragFloat("Delay", &track.delay, 0.01f, 0.0f, 60.0f);
            ImGui::DragFloat2("Life Min/Max", &s.lifespan.x, 0.05f, 0.01f, 60.0f);
            ImGui::DragFloat2("Speed Min/Max", &s.emissionSpeed.x, 0.05f, 0.0f, 100.0f);

            const char* modes[] = { "Continuous", "Burst" };
            ImGui::Combo("Mode", reinterpret_cast<int*>(&s.emissionMode), modes, IM_ARRAYSIZE(modes));
            if (s.emissionMode == 0)
            {
                ImGui::DragInt("Spawn Rate", reinterpret_cast<int*>(&s.burstCount), 1, 0, 20000);
            }
            else
            {
                ImGui::DragInt("Burst Count", reinterpret_cast<int*>(&s.burstCount), 1, 0, static_cast<int>(s.maxParticleCount));
                if (ImGui::Button("Fire Burst")) s.burstTrigger = 1.0f;
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Physics"))
        {
            ImGui::DragFloat("Gravity", &s.gravity, 0.01f, -100.0f, 100.0f);
            ImGui::DragFloat2("Cone Angle", &s.emissionConeAngle.x, 0.01f, 0.0f, 6.28f);
            ImGui::DragFloat2("Rotation Speed", &s.emissionAngularSpeed.x, 0.01f, -50.0f, 50.0f);
            ImGui::DragFloat("Noise Scale", &s.noiseScale, 0.01f, 0.0f, 100.0f);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Shape"))
        {
            const char* shapes[] = { "Point", "Box", "Circle", "Sphere", "Line", "Mesh Vertices" };
            ImGui::Combo("Spawn Shape", reinterpret_cast<int*>(&s.spawnShape), shapes, IM_ARRAYSIZE(shapes));
            ImGui::DragFloat3("Emitter Offset", &s.emissionPosition.x, 0.05f);
            ImGui::DragFloat3("Shape Size", &s.shapeSize.x, 0.05f, 0.0f, 100.0f);
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

            if (s.useColorOverLife != 0)
            {
                ImGui::ColorEdit4("Start Color", &s.colorStart.x);
                ImGui::ColorEdit4("End Color", &s.colorEnd.x);
            }
            else
            {
                ImGui::ColorEdit4("Color", &s.manualColor.x);
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
                if (s.sizeRandomStart.x > s.sizeRandomStart.y) std::swap(s.sizeRandomStart.x, s.sizeRandomStart.y);
                if (s.sizeRandomEnd.x > s.sizeRandomEnd.y) std::swap(s.sizeRandomEnd.x, s.sizeRandomEnd.y);
            }
            ImGui::DragFloat2("Fade In/Out", &s.fadeDuration.x, 0.01f, 0.0f, 10.0f);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Texture"))
        {
            textureCombo("Color Texture", track.textureFileName, track.texturePath, 0);
            
            textureCombo("Mask Texture", track.maskTextureFileName, track.maskTexturePath, 1);
            textureCombo("Gradient Texture", track.gradientTextureFileName, track.gradientTexturePath, 2);
            bool useMask = s.useMaskTexture != 0;
            bool useGradient = s.useGradientTexture != 0;
            if (ImGui::Checkbox("Use Mask", &useMask)) s.useMaskTexture = useMask ? 1u : 0u;
            if (ImGui::Checkbox("Use Gradient", &useGradient)) s.useGradientTexture = useGradient ? 1u : 0u;

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
            if (ImGui::Checkbox("Animate Sprite Index", &useSpriteAnimation_)) s.useSpriteAnimation = useSpriteAnimation_ ? 1u : 0u;
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
                if (ImGui::Checkbox("Loop Sprite Animation", &spriteAnimationLoop_)) s.spriteAnimationLoop = spriteAnimationLoop_ ? 1u : 0u;
            }
            ImGui::Text("Selected Linear Index: %u", s.spriteSheetIndex.y * s.textureSplitCount.x + s.spriteSheetIndex.x);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Render"))
        {
            const char* renderTypes[] = { "Billboard", "Stretched" };
            ImGui::Combo("Render Type", reinterpret_cast<int*>(&s.type), renderTypes, IM_ARRAYSIZE(renderTypes));
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
        if (wasPlaying_) Play();
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

    if (ImGui::BeginPopupModal(textureDialogKey.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted(pendingTextureSlot == 0 ? "Select Color Texture" : (pendingTextureSlot == 1 ? "Select Mask Texture" : "Select Gradient Texture"));
        ImGui::Separator();
        ImGui::TextWrapped("%s", textureBrowserDirectory.c_str());

        if (ImGui::Button("Up"))
        {
            const std::filesystem::path parentPath_ = std::filesystem::path(textureBrowserDirectory).parent_path();
            if (!parentPath_.empty()) textureBrowserDirectory = parentPath_.generic_string();
            textureBrowserSelection.clear();
        }
        ImGui::SameLine();
        if (ImGui::Button("Particle Textures"))
        {
            textureBrowserDirectory = "Data/Texture/Particle";
            textureBrowserSelection.clear();
        }

        ImGui::BeginChild("TextureFileList", ImVec2(620.0f, 360.0f), true);
        std::vector<std::filesystem::directory_entry> browserEntries_;
        std::error_code browserError_;
        for (const auto& entry_ : std::filesystem::directory_iterator(textureBrowserDirectory, browserError_))
        {
            if (entry_.is_directory())
            {
                browserEntries_.push_back(entry_);
                continue;
            }

            std::string extension_ = entry_.path().extension().string();
            std::transform(extension_.begin(), extension_.end(), extension_.begin(), [](unsigned char char_) { return static_cast<char>(std::tolower(char_)); });
            if (extension_ == ".png" || extension_ == ".jpg" || extension_ == ".jpeg" || extension_ == ".bmp" ||
                extension_ == ".tga" || extension_ == ".dds" || extension_ == ".hdr")
            {
                browserEntries_.push_back(entry_);
            }
        }
        std::sort(browserEntries_.begin(), browserEntries_.end(), [](const auto& left_, const auto& right_)
        {
            if (left_.is_directory() != right_.is_directory()) return left_.is_directory();
            return left_.path().filename().string() < right_.path().filename().string();
        });

        for (const auto& entry_ : browserEntries_)
        {
            const std::string entryPath_ = entry_.path().generic_string();
            const std::string entryLabel_ = entry_.is_directory()
                ? "[DIR] " + entry_.path().filename().string()
                : entry_.path().filename().string();
            const bool selected_ = textureBrowserSelection == entryPath_;
            if (ImGui::Selectable(entryLabel_.c_str(), selected_, ImGuiSelectableFlags_AllowDoubleClick))
            {
                if (entry_.is_directory() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    textureBrowserDirectory = entryPath_;
                    textureBrowserSelection.clear();
                    break;
                }
                textureBrowserSelection = entryPath_;
            }
        }
        ImGui::EndChild();

        ImGui::Text("Selected: %s", textureBrowserSelection.empty() ? "None" : textureBrowserSelection.c_str());
        const bool canLoad_ = !textureBrowserSelection.empty() && std::filesystem::is_regular_file(textureBrowserSelection);
        if (!canLoad_) ImGui::BeginDisabled();
        if (ImGui::Button("Load Texture", ImVec2(160.0f, 0.0f)))
        {
            if (LoadEmitterTextureSlot(track, textureBrowserSelection, pendingTextureSlot))
            {
                const std::string selectedFileName_ = std::filesystem::path(textureBrowserSelection).filename().string();
                if (pendingTextureSlot == 0) track.textureFileName = selectedFileName_;
                else if (pendingTextureSlot == 1) track.maskTextureFileName = selectedFileName_;
                else track.gradientTextureFileName = selectedFileName_;
                applyFeedbackUntil = ImGui::GetTime() + 2.5;
            }
            pendingTextureSlot = -1;
            ImGui::CloseCurrentPopup();
        }
        if (!canLoad_) ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
        {
            pendingTextureSlot = -1;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::PopID();
}

bool ParticleComponent::LoadEmitterTexture(EmitterTrack& track, const std::string& fullPath)
{
    return LoadEmitterTextureSlot(track, fullPath, 0);
}

bool ParticleComponent::LoadEmitterTextureSlot(EmitterTrack& track, const std::string& fullPath, int slot)
{
    if (!track.emitter || fullPath.empty()) return false;

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    std::wstring wpath(fullPath.begin(), fullPath.end());

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
        else
        {
            track.emitter->SetTexture(srv);
            track.texturePath = fullPath;
        }
        return true;
    }
    return false;
}

void ParticleComponent::Stop()
{
    isPlaying = false;
}
void ParticleComponent::RefreshTextureList()
{
    textureFiles.clear();
    std::string path = "Data/Texture/Particle/";
    if (!std::filesystem::exists(path)) return;

    for (const auto& entry : std::filesystem::directory_iterator(path))
    {
        if (entry.is_regular_file()) textureFiles.push_back(entry.path().filename().string());
    }
}

void ParticleComponent::RefreshAssetList()
{
    assetFiles.clear();
    std::string path_ = "Data/Particle/";
    if (!std::filesystem::exists(path_)) return;

    for (const auto& entry_ : std::filesystem::directory_iterator(path_))
    {
        if (!entry_.is_regular_file()) continue;
        if (entry_.path().extension() == ".json")
        {
            assetFiles.push_back(entry_.path().filename().string());
        }
    }
}

bool ParticleComponent::SaveAsset(const std::string& path_)
{
    if (path_.empty()) return false;

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
    if (path_.empty() || !std::filesystem::exists(path_)) return false;

    std::ifstream file_(path_);
    if (!file_.is_open()) return false;

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

void ParticleComponent::Play()
{
    isPlaying = true;
    timer = 0.0f;
    for (auto& track : tracks)
    {
        track.isStarted = true;
        if (track.settings.emissionMode == 1) track.settings.burstTrigger = 1.0f;
    }
}

void ParticleComponent::Update(float deltaTime)
{
    if (!isPlaying) return;

    timer += deltaTime;
    auto context = GraphicsDeviceManager::Instance().GetContext();
    auto owner = GetGameObject();
    if (!owner) return;

    Vector3 worldPos = owner->transform->GetWorldPosition();

    for (auto& track : tracks)
    {
        if (!track.isStarted && timer >= track.delay)
        {
            track.isStarted = true;
            if (track.settings.emissionMode == 1) track.settings.burstTrigger = 1.0f;
        }

        if (track.isStarted && track.emitter)
        {
            if (track.settings.spawnShape == 5)
            {
                UpdateMeshSpawnPositions(track);
            }

            ParticleSystemConstants tempSettings = track.settings;
            tempSettings.emissionPosition.x = worldPos.x + track.settings.emissionPosition.x;
            tempSettings.emissionPosition.y = worldPos.y + track.settings.emissionPosition.y;
            tempSettings.emissionPosition.z = worldPos.z + track.settings.emissionPosition.z;

            track.emitter->ApplySettings(tempSettings);
            track.emitter->Update(context, deltaTime);

            if (track.settings.burstTrigger > 0.0f) track.settings.burstTrigger = 0.0f;
        }
    }
}

void ParticleComponent::Serialize(nlohmann::json& j) const
{
    nlohmann::json emitterList = nlohmann::json::array();

    for (const auto& track : tracks)
    {
        nlohmann::json ej;
        const auto& s = track.settings;

        ej["TexturePath"] = track.texturePath;
        ej["MaskTexturePath"] = track.maskTexturePath;
        ej["GradientTexturePath"] = track.gradientTexturePath;
        ej["Delay"] = track.delay;
        ej["MeshVertexSampleCount"] = track.meshVertexSampleCount;
        ej["MaxCount"] = s.maxParticleCount;
        ej["EmissionMode"] = s.emissionMode;
        ej["BurstCount"] = s.burstCount;
        ej["SpawnShape"] = s.spawnShape;
        ej["UseMask"] = s.useMaskTexture;
        ej["UseGradient"] = s.useGradientTexture;
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
        ej["Color"] = { s.manualColor.x, s.manualColor.y, s.manualColor.z, s.manualColor.w };
        ej["ColorStart"] = { s.colorStart.x, s.colorStart.y, s.colorStart.z, s.colorStart.w };
        ej["ColorEnd"] = { s.colorEnd.x, s.colorEnd.y, s.colorEnd.z, s.colorEnd.w };
        ej["UseColorOverLife"] = s.useColorOverLife;
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

        emitterList.push_back(ej);
    }

    j["Emitters"] = emitterList;
    j["IsPlaying"] = isPlaying;
    j["AssetPath"] = assetPath;
}

void ParticleComponent::Deserialize(const nlohmann::json& j)
{
    tracks.clear();
    assetPath = j.value("AssetPath", assetPath);

    if (j.contains("Emitters"))
    {
        for (const auto& ej : j["Emitters"])
        {
            EmitterTrack track;
            auto& s = track.settings;

            track.texturePath = ej.value("TexturePath", "");
            track.maskTexturePath = ej.value("MaskTexturePath", "");
            track.gradientTexturePath = ej.value("GradientTexturePath", "");
            if (!track.texturePath.empty()) track.textureFileName = std::filesystem::path(track.texturePath).filename().string();
            if (!track.maskTexturePath.empty()) track.maskTextureFileName = std::filesystem::path(track.maskTexturePath).filename().string();
            if (!track.gradientTexturePath.empty()) track.gradientTextureFileName = std::filesystem::path(track.gradientTexturePath).filename().string();

            track.delay = ej.value("Delay", 0.0f);
            track.meshVertexSampleCount = ej.value("MeshVertexSampleCount", 2048u);
            s.maxParticleCount = ej.value("MaxCount", 1000u);
            s.emissionMode = ej.value("EmissionMode", 0u);
            s.burstCount = ej.value("BurstCount", 100u);
            s.spawnShape = ej.value("SpawnShape", 0u);
            s.useMaskTexture = ej.value("UseMask", 0u);
            s.useGradientTexture = ej.value("UseGradient", 0u);
            s.gravity = ej.value("Gravity", 0.0f);
            s.type = ej.value("RenderType", 0u);

            if (ej.contains("PosOffset")) s.emissionPosition = { ej["PosOffset"][0], ej["PosOffset"][1], ej["PosOffset"][2], 1.0f };
            if (ej.contains("ShapeStart")) s.shapeStart = { ej["ShapeStart"][0], ej["ShapeStart"][1], ej["ShapeStart"][2], 1.0f };
            if (ej.contains("ShapeEnd")) s.shapeEnd = { ej["ShapeEnd"][0], ej["ShapeEnd"][1], ej["ShapeEnd"][2], 1.0f };
            if (ej.contains("ShapeSize")) s.shapeSize = { ej["ShapeSize"][0], ej["ShapeSize"][1], ej["ShapeSize"][2], 0.0f };
            if (ej.contains("LegacyRadius")) s.emissionOffset = { ej["LegacyRadius"][0], ej["LegacyRadius"][1] };
            if (ej.contains("Lifespan")) s.lifespan = { ej["Lifespan"][0], ej["Lifespan"][1] };
            if (ej.contains("Speed")) s.emissionSpeed = { ej["Speed"][0], ej["Speed"][1] };
            if (ej.contains("ConeAngle")) s.emissionConeAngle = { ej["ConeAngle"][0], ej["ConeAngle"][1] };
            if (ej.contains("RotationSpeed")) s.emissionAngularSpeed = { ej["RotationSpeed"][0], ej["RotationSpeed"][1] };
            if (ej.contains("Color")) s.manualColor = { ej["Color"][0], ej["Color"][1], ej["Color"][2], ej["Color"][3] };
            if (ej.contains("ColorStart")) s.colorStart = { ej["ColorStart"][0], ej["ColorStart"][1], ej["ColorStart"][2], ej["ColorStart"][3] };
            else s.colorStart = s.manualColor;
            if (ej.contains("ColorEnd")) s.colorEnd = { ej["ColorEnd"][0], ej["ColorEnd"][1], ej["ColorEnd"][2], ej["ColorEnd"][3] };
            else s.colorEnd = s.manualColor;
            s.useColorOverLife = ej.value("UseColorOverLife", 0u);
            if (ej.contains("Size")) s.emissionSize = { ej["Size"][0], ej["Size"][1] };
            if (ej.contains("SizeRandomStart")) s.sizeRandomStart = { ej["SizeRandomStart"][0], ej["SizeRandomStart"][1] };
            if (ej.contains("SizeRandomEnd")) s.sizeRandomEnd = { ej["SizeRandomEnd"][0], ej["SizeRandomEnd"][1] };
            s.useRandomSize = ej.value("UseRandomSize", 0u);
            if (ej.contains("Fade")) s.fadeDuration = { ej["Fade"][0], ej["Fade"][1] };
            if (ej.contains("TextureSplitCount")) s.textureSplitCount = { ej["TextureSplitCount"][0], ej["TextureSplitCount"][1] };
            if (ej.contains("SpriteSheetIndex")) s.spriteSheetIndex = { ej["SpriteSheetIndex"][0], ej["SpriteSheetIndex"][1] };
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

    if (j.value("IsPlaying", false)) Play();
}

void ParticleComponent::UpdateMeshSpawnPositions(EmitterTrack& track_)
{
    if (!track_.emitter) return;

    auto owner_ = GetGameObject();
    auto model_ = owner_ ? owner_->GetComponent<GltfModelCommponennt>() : nullptr;
    if (!model_)
    {
        track_.emitter->SetMeshSpawnPositions({});
        return;
    }

    size_t totalVertexCount_ = 0;
    for (const auto& mesh_ : model_->GetMeshes()) totalVertexCount_ += mesh_.vertices.size();
    if (totalVertexCount_ == 0)
    {
        track_.emitter->SetMeshSpawnPositions({});
        return;
    }

    const size_t sampleLimit_ = (std::max)(static_cast<size_t>(1), static_cast<size_t>(track_.meshVertexSampleCount));
    const size_t sampleStride_ = (std::max)(static_cast<size_t>(1), totalVertexCount_ / sampleLimit_);
    std::vector<DirectX::XMFLOAT3> positions_;
    positions_.reserve((std::min)(totalVertexCount_, sampleLimit_));

    size_t vertexIndex_ = 0;
    for (const auto& mesh_ : model_->GetMeshes())
    {
        const DirectX::XMMATRIX meshTransform_ = mesh_.node
            ? DirectX::XMLoadFloat4x4(&mesh_.node->globalTransform)
            : DirectX::XMMatrixIdentity();
        for (const auto& vertex_ : mesh_.vertices)
        {
            if ((vertexIndex_++ % sampleStride_) != 0) continue;
            DirectX::XMVECTOR position_ = DirectX::XMLoadFloat3(&vertex_.position);
            position_ = DirectX::XMVector3TransformCoord(position_, meshTransform_);
            DirectX::XMFLOAT3 transformedPosition_{};
            DirectX::XMStoreFloat3(&transformedPosition_, position_);
            positions_.push_back(transformedPosition_);
            if (positions_.size() >= sampleLimit_) break;
        }
        if (positions_.size() >= sampleLimit_) break;
    }

    track_.emitter->SetMeshSpawnPositions(positions_);
}

void ParticleComponent::RebuildEmitterResources()
{
    auto device_ = GraphicsDeviceManager::Instance().GetDevice();
    if (!device_) return;

    RefreshTextureList();

    for (auto& track_ : tracks)
    {
        track_.emitter = std::make_unique<ParticleEmitter>(device_, track_.settings, track_.settings.maxParticleCount);
        if (!track_.texturePath.empty()) LoadEmitterTextureSlot(track_, track_.texturePath, 0);
        if (!track_.maskTexturePath.empty()) LoadEmitterTextureSlot(track_, track_.maskTexturePath, 1);
        if (!track_.gradientTexturePath.empty()) LoadEmitterTextureSlot(track_, track_.gradientTexturePath, 2);
    }
}

std::string ParticleComponent::BuildAssetPathFromName() const
{
    std::string name_ = assetNameBuffer;
    if (name_.empty()) name_ = "NewParticle";

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


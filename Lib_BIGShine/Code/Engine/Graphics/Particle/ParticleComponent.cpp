#include "ParticleComponent.h"

#include "Engine/Graphics/Core/CPUResourceLoader.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include <filesystem>

void ParticleComponent::Start()
{
    RefreshTextureList();

    auto device = GraphicsDeviceManager::Instance().GetDevice();
    for (auto& track : tracks)
    {
        if (!track.emitter)
        {
            track.emitter = std::make_unique<ParticleEmitter>(device, track.settings, track.settings.maxParticleCount);
        }

        if (!track.texturePath.empty()) LoadEmitterTextureSlot(track, track.texturePath, 0);
        if (!track.maskTexturePath.empty()) LoadEmitterTextureSlot(track, track.maskTexturePath, 1);
        if (!track.gradientTexturePath.empty()) LoadEmitterTextureSlot(track, track.gradientTexturePath, 2);
    }
}

void ParticleComponent::Gui()
{
    if (ImGui::Button("Play Effect")) Play();
    ImGui::SameLine();
    if (ImGui::Button("Stop")) isPlaying = false;
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
                    if (slot == 1) { track.emitter->SetMaskTexture(nullptr); s.useMaskTexture = 0; }
                    else if (slot == 2) { track.emitter->SetGradientTexture(nullptr); s.useGradientTexture = 0; }
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
            const char* shapes[] = { "Point", "Box", "Circle", "Sphere", "Line" };
            ImGui::Combo("Spawn Shape", reinterpret_cast<int*>(&s.spawnShape), shapes, IM_ARRAYSIZE(shapes));
            ImGui::DragFloat3("Emitter Offset", &s.emissionPosition.x, 0.05f);
            ImGui::DragFloat3("Shape Size", &s.shapeSize.x, 0.05f, 0.0f, 100.0f);
            if (s.spawnShape == 4)
            {
                ImGui::DragFloat3("Line Start", &s.shapeStart.x, 0.05f);
                ImGui::DragFloat3("Line End", &s.shapeEnd.x, 0.05f);
            }
            ImGui::DragFloat2("Legacy Radius Min/Max", &s.emissionOffset.x, 0.05f, 0.0f, 100.0f);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Visual"))
        {
            ImGui::ColorEdit4("Color", &s.manualColor.x);
            ImGui::DragFloat2("Size Start/End", &s.emissionSize.x, 0.02f, 0.01f, 100.0f);
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
        ej["Size"] = { s.emissionSize.x, s.emissionSize.y };
        ej["Fade"] = { s.fadeDuration.x, s.fadeDuration.y };
        ej["RenderType"] = s.type;

        emitterList.push_back(ej);
    }

    j["Emitters"] = emitterList;
    j["IsPlaying"] = isPlaying;
}

void ParticleComponent::Deserialize(const nlohmann::json& j)
{
    tracks.clear();

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
            if (ej.contains("Size")) s.emissionSize = { ej["Size"][0], ej["Size"][1] };
            if (ej.contains("Fade")) s.fadeDuration = { ej["Fade"][0], ej["Fade"][1] };

            tracks.push_back(std::move(track));
        }
    }

    if (j.value("IsPlaying", false)) Play();
}


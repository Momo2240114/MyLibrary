#include "ParticleComponent.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "CPUResourceLoader.h"
#include <filesystem>

void ParticleComponent::Start() {
    RefreshTextureList();
    // Start時点で各エミッターの実体がない場合は作成
    auto device = GraphicsDeviceManager::Instance().GetDevice();
    for (auto& track : tracks) {
        if (!track.emitter) {
            track.emitter = std::make_unique<ParticleEmitter>(device, track.settings, track.settings.maxParticleCount);
            if (!track.texturePath.empty()) {
                LoadEmitterTexture(track, track.texturePath);
            }
        }
    }
}


void ParticleComponent::Gui()
{
    // --- 制御系 ---
    if (ImGui::Button("Play Effect")) Play();
    ImGui::SameLine();
    if (ImGui::Button("Stop")) isPlaying = false;
    ImGui::SameLine();
    if (ImGui::Button("Reset All")) {
        for (auto& t : tracks) t.isStarted = false;
    }

    ImGui::Separator();

    // --- エミッター管理 ---
    if (ImGui::Button("Add Emitter")) {
        EmitterTrack newTrack;
        // 構造体の初期化（ゴミデータ防止）
        newTrack.settings = ParticleSystemConstants();
        newTrack.settings.maxParticleCount = 100;
        newTrack.emitter = std::make_unique<ParticleEmitter>(
            GraphicsDeviceManager::Instance().GetDevice(),
            newTrack.settings, newTrack.settings.maxParticleCount);
        tracks.push_back(std::move(newTrack));
    }

    if (tracks.empty()) {
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "No Emitters. Click 'Add Emitter'.");
        return;
    }

    // エミッター選択の安全チェック
    if (selectedEmitterIndex >= (int)tracks.size()) selectedEmitterIndex = (int)tracks.size() - 1;

    ImGui::Text("Emitter List:");
    // 子ウィンドウ ID の固定
    ImGui::BeginChild("EmitterList", ImVec2(150, 80), true);
    for (int i = 0; i < (int)tracks.size(); i++) {
        ImGui::PushID(i); // 要素ごとの ID プッシュ
        char label[32];
        sprintf_s(label, "Emitter %d", i);
        if (ImGui::Selectable(label, selectedEmitterIndex == i)) selectedEmitterIndex = i;
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::SameLine();

    ImGui::BeginGroup();
    // 削除フラグ
    bool shouldRemove = false;
    if (ImGui::Button("Remove Selected")) {
        shouldRemove = true;
    }
    if (ImGui::Button("Refresh Folder")) RefreshTextureList();
    ImGui::EndGroup();

    // 削除が実行された場合、UI の深部（TabBar）に入る前に return する
    if (shouldRemove) {
        tracks.erase(tracks.begin() + selectedEmitterIndex);
        if (selectedEmitterIndex > 0) selectedEmitterIndex--;
        // BeginTabBar を呼ぶ前に抜けるので安全
        return;
    }

    ImGui::Separator();

    // --- パラメータ詳細編集 ---
    // 選択中のエミッター ID でラップして、スライダーの値の誤認識を防ぐ
    ImGui::PushID(selectedEmitterIndex);

    auto& track = tracks[selectedEmitterIndex];
    auto& s = track.settings;

    if (ImGui::BeginTabBar("EmitterParams")) {

        // 1. 放出（Spawn）に関する設定
        if (ImGui::BeginTabItem("Emission")) {
            int maxCount = (int)s.maxParticleCount;
            if (ImGui::SliderInt("Max Particle Count", &maxCount, 1, 5000)) {
                s.maxParticleCount = (uint32_t)maxCount;
                // 注意：実行中に数を変える場合は Emitter 側のバッファ再生成が必要
            }

            ImGui::DragFloat3("Pos Offset", &s.emissionPosition.x, 0.1f);
            ImGui::DragFloat2("Spawn Range (Min/Max)", &s.emissionOffset.x, 0.1f);
            ImGui::SliderFloat2("Cone Angle (Rad)", &s.emissionConeAngle.x, 0.0f, 6.28f);
            ImGui::DragFloat2("Speed Range", &s.emissionSpeed.x, 0.1f);
            ImGui::DragFloat2("Spawn Delay (Min/Max)", &s.spawnDelay.x, 0.01f);

            const char* modes[] = { "Continuous (持続)", "Burst (一斉)" };
            ImGui::Combo("Emission Mode", (int*)&s.emissionMode, modes, IM_ARRAYSIZE(modes));

            if (s.emissionMode == 1) { // Burst モード
                ImGui::DragInt("Burst Count", (int*)&s.burstCount, 1, 0, (int)s.maxParticleCount);
                if (ImGui::Button("FIRE BURST!")) {
                    s.burstTrigger = 1.0f;
                }
            }
            ImGui::EndTabItem();
        }

        // 2. 挙動（Physics）に関する設定
        if (ImGui::BeginTabItem("Physics")) {
            ImGui::DragFloat2("Lifespan (Min/Max)", &s.lifespan.x, 0.1f, 0.1f, 20.0f);
            ImGui::DragFloat("Gravity", &s.gravity, 0.01f);
            ImGui::DragFloat("Noise Scale", &s.noiseScale, 0.01f);
            ImGui::DragFloat2("Rotation Speed", &s.emissionAngularSpeed.x, 0.1f);
            ImGui::EndTabItem();
        }

        // 3. 見た目（Visual）に関する設定
        if (ImGui::BeginTabItem("Visual")) {
            ImGui::ColorEdit4("Color Tint", &s.manualColor.x);
            ImGui::DragFloat2("Size (Start/End)", &s.emissionSize.x, 0.05f, 0.01f, 10.0f);
            ImGui::DragFloat2("Fade Duration (In/Out)", &s.fadeDuration.x, 0.01f, 0.0f, 5.0f);

            const char* types[] = { "Billboard", "Stretched" };
            ImGui::Combo("Render Type", (int*)&s.type, types, IM_ARRAYSIZE(types));

            ImGui::Separator();

            const char* preview = track.textureFileName.empty() ? "Select..." : track.textureFileName.c_str();
            if (ImGui::BeginCombo("Texture", preview)) {
                for (const auto& file : textureFiles) {
                    if (ImGui::Selectable(file.c_str(), track.textureFileName == file)) {
                        track.textureFileName = file;
                        track.texturePath = "Data/Texture/Particle/" + file;
                        LoadEmitterTexture(track, track.texturePath);
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar(); // BeginTabBar に対する End
    }

    ImGui::PopID(); // BeginChild 以降の PushID に対する Pop
}


bool ParticleComponent::LoadEmitterTexture(EmitterTrack& track, const std::string& fullPath) {
    if (!track.emitter) return false;

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    std::wstring wpath(fullPath.begin(), fullPath.end());

    // CPUResourceLoaderを使用してSRVを作成
    if (SUCCEEDED(CPUResourceLoader::LoadTextureFromFile(
        GraphicsDeviceManager::Instance().GetDevice(),
        wpath.c_str(), srv.GetAddressOf(), nullptr)))
    {
        track.emitter->SetTexture(srv); // EmitterにSRVをセットする関数が必要
        track.texturePath = fullPath;
        return true;
    }
    return false;
}

void ParticleComponent::RefreshTextureList() {
    textureFiles.clear();
    std::string path = "Data/Texture/Particle/";
    if (!std::filesystem::exists(path)) return;
    for (const auto& entry : std::filesystem::directory_iterator(path)) {
        if (entry.is_regular_file()) textureFiles.push_back(entry.path().filename().string());
    }
}

void ParticleComponent::Play()
{
    isPlaying = true; // これが必要！
    for (auto& track : tracks) {
        track.isStarted = true;
        // バーストモードにする場合
        track.settings.emissionMode = 1;
        track.settings.burstCount = 100; // 一気に100個出す
        track.settings.burstTrigger = 1.0f; // CS側でこれを見て放出させる
    }
}

void ParticleComponent::Update(float deltaTime)
{
    if (!isPlaying) return;

    timer += deltaTime;
    auto context = GraphicsDeviceManager::Instance().GetContext();
    auto owner = GetGameObject();
    if (!owner) return;

    // GameObjectのTransformから最新の座標を取得
    Vector3 worldPos = owner->transform->GetWorldPosition();

    for (auto& track : tracks)
    {
        // 1. 開始判定
        if (!track.isStarted && timer >= track.delay)
        {
            track.isStarted = true;
            // 発生直後の1フレームだけバーストトリガーをオンにする
            track.settings.burstTrigger = 1.0f;
        }

        // 2. 実行
        if (track.isStarted && track.emitter)
        {
            // GUIで編集した設定(settings)をエミッター側に同期
            // 座標はTransformの値を優先してオフセットを加算
            ParticleSystemConstants tempSettings = track.settings;

            // 計算結果のみを一時変数に入れる
            tempSettings.emissionPosition.x = worldPos.x + track.settings.emissionPosition.x;
            tempSettings.emissionPosition.y = worldPos.y + track.settings.emissionPosition.y;
            tempSettings.emissionPosition.z = worldPos.z + track.settings.emissionPosition.z;

            track.emitter->ApplySettings(tempSettings); // 一時的な設定を適用
            track.emitter->Update(context, deltaTime);

            // バーストフラグは1回 Dispatch したら下げておく
            if (track.settings.burstTrigger > 0.0f) {
                track.settings.burstTrigger = 0.0f;
            }
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

        // --- 共通パラメータ ---
        ej["TexturePath"] = track.texturePath;
        ej["Delay"] = track.delay;
        ej["MaxCount"] = s.maxParticleCount;

        // --- 構造体の中身 (主要なもの) ---
        ej["PosOffset"] = { s.emissionPosition.x, s.emissionPosition.y, s.emissionPosition.z };
        ej["Lifespan"] = { s.lifespan.x, s.lifespan.y };
        ej["Gravity"] = s.gravity;
        ej["Color"] = { s.manualColor.x, s.manualColor.y, s.manualColor.z, s.manualColor.w };
        ej["Speed"] = { s.emissionSpeed.x, s.emissionSpeed.y };
        ej["EmissionMode"] = s.emissionMode; // 0:Continuous, 1:Burst

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

            // --- 共通パラメータ復元 ---
            track.texturePath = ej.value("TexturePath", "");
            if (!track.texturePath.empty()) {
                // ファイル名だけ抽出して保持（GUIコンボボックス用）
                std::filesystem::path p(track.texturePath);
                track.textureFileName = p.filename().string();
            }
            track.delay = ej.value("Delay", 0.0f);
            s.maxParticleCount = ej.value("MaxCount", 1000);

            // --- 構造体復元 (配列からVectorへ) ---
            if (ej.contains("PosOffset")) {
                s.emissionPosition = Vector4(ej["PosOffset"][0], ej["PosOffset"][1], ej["PosOffset"][2], 1.0f);
            }
            if (ej.contains("Lifespan")) {
                s.lifespan = Vector2(ej["Lifespan"][0], ej["Lifespan"][1]);
            }
            s.gravity = ej.value("Gravity", 0.0f);
            if (ej.contains("Color")) {
                s.manualColor = Color(ej["Color"][0], ej["Color"][1], ej["Color"][2], ej["Color"][3]);
            }
            if (ej.contains("Speed")) {
                s.emissionSpeed = Vector2(ej["Speed"][0], ej["Speed"][1]);
            }
            s.emissionMode = ej.value("EmissionMode", 0);

            tracks.push_back(std::move(track));
        }
    }

    if (j.value("IsPlaying", false)) Play();
}
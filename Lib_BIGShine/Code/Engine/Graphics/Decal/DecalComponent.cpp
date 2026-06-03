#include "DecalComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Graphics/Core/CPUResourceLoader.h"

void DecalComponent::SetTexture(const std::string& path)
{
    auto device = GraphicsDeviceManager::Instance().GetDevice();

    // std::string を wchar_t* (std::wstring) に変換
    std::wstring wpath = std::wstring(path.begin(), path.end());

    // CPUResourceLoader を使ってロード
    // キャッシュ機能があるので、既に読み込み済みなら一瞬で終わります
    HRESULT hr = CPUResourceLoader::LoadTextureFromFile(
        device,
        wpath.c_str(),
        texture.ReleaseAndGetAddressOf()
    );

    if (FAILED(hr)) {
        // ロード失敗時はダミー（白いテクスチャなど）を入れると安全
        CPUResourceLoader::CreateDummyTexture(device, 0xFFFFFFFF, texture.ReleaseAndGetAddressOf());
    }
}

void DecalComponent::SubmitDecal(DecalManager& manager)
{
    if (!IsEnabled()) return;

    auto owner = GetGameObject();
    if (!owner) return;

    DecalData data;
    // Transformから最新のワールド行列を取得（Dirtyフラグ制御込み）
    DirectX::XMStoreFloat4x4(&data.worldMatrix, owner->transform->GetWorldMatrix());
    data.color = this->color;
    data.texture = this->texture.Get();

    manager.AddDecal(data);
}

void DecalComponent::Update(float et)
{
    // 開発時のみ、投影範囲をワイヤーフレームで表示する
    if (showDebugBounds) {
        auto transform = GetGameObject()->transform;

        // 現在「度」で返ってくる回転を取得
        DirectX::XMFLOAT3 rotDeg = transform->GetWorldRotation();

        // 描画用にラジアンへ変換
        DirectX::XMFLOAT3 rotRad = {
            DirectX::XMConvertToRadians(rotDeg.x),
            DirectX::XMConvertToRadians(rotDeg.y),
            DirectX::XMConvertToRadians(rotDeg.z)
        };

        GraphicsDeviceManager::Instance().GetShapeRendererSystem().DrawBox(
            transform->GetWorldPosition(),
            rotRad, // ラジアンに直したものを渡す
            transform->GetWorldScale(),
            DirectX::XMFLOAT4(1, 1, 0, 0.5f)
        );
    }
}

void DecalComponent::Gui()
{
    ImGui::ColorEdit4("Decal Color", &color.x);

    // --- ファイル選択コンボボックス ---
    const char* previewValue = (selectedFileIndex >= 0) ? decalFiles[selectedFileIndex].c_str() : "Select a decal...";

    if (ImGui::BeginCombo("Decal Texture", previewValue)) {
        for (int i = 0; i < (int)decalFiles.size(); i++) {
            const bool isSelected = (selectedFileIndex == i);
            if (ImGui::Selectable(decalFiles[i].c_str(), isSelected)) {
                selectedFileIndex = i;

                // ファイルが選ばれたら即座にロード
                std::string fullPath = "Data/Decal/" + decalFiles[i];
                SetTexture(fullPath); // 前に作ったロード関数を呼び出す
            }

            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    // 更新ボタン（フォルダに新しい画像を入れた時用）
    if (ImGui::Button("Refresh Folder")) {
        RefreshDecalFileList();
    }

    if (texture) {
        ImGui::Separator();
        ImGui::Text("Preview:");
        ImGui::Image((void*)texture.Get(), ImVec2(100, 100));
    }
}

void DecalComponent::Serialize(nlohmann::json& j) const
{
    // コンポーネント名をキーにしてデータを格納
    j["ComponentName"] = componentName;

    // 現在選択されているファイル名（パス）を保存
    if (selectedFileIndex >= 0 && selectedFileIndex < (int)decalFiles.size()) {
        j["TextureFile"] = decalFiles[selectedFileIndex];
    }
    else {
        j["TextureFile"] = "";
    }

    // 色情報も保存
    j["Color"] = { color.x, color.y, color.z, color.w };
}

void DecalComponent::Deserialize(const nlohmann::json& j)
{
    // 色の復元
    if (j.contains("Color")) {
        color.x = j["Color"][0];
        color.y = j["Color"][1];
        color.z = j["Color"][2];
        color.w = j["Color"][3];
    }

    // テクスチャの復元
    if (j.contains("TextureFile")) {
        std::string fileName = j["TextureFile"];
        if (!fileName.empty()) {
            // リスト内から一致するファイルを探してインデックスを更新
            RefreshDecalFileList(); // ロード前に最新のリストを確保

            auto it = std::find(decalFiles.begin(), decalFiles.end(), fileName);
            if (it != decalFiles.end()) {
                selectedFileIndex = static_cast<int>(std::distance(decalFiles.begin(), it));

                // 実際のテクスチャをロード
                std::string fullPath = "Data/Decal/" + fileName;
                SetTexture(fullPath);
            }
        }
    }
}


void DecalComponent::RefreshDecalFileList()
{
    decalFiles.clear();
    std::string path = "Data/Decal";

    if (!std::filesystem::exists(path)) return;

    for (const auto& entry : std::filesystem::directory_iterator(path)) {
        if (entry.is_regular_file()) {
            std::string ext = entry.path().extension().string();
            // 画像ファイルだけをリストに入れる
            if (ext == ".png" || ext == ".jpg" || ext == ".dds" || ext == ".tga" || ext == ".PNG") {
                decalFiles.push_back(entry.path().filename().string());
            }
        }
    }
}
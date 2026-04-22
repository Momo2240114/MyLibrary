#include "Sprite.h"
#include "CPUResourceLoader.h"
#include "ImGui.h"

#include "format"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/GamePlay/Transform/Transform.h"


void SpriteRenderer::Start()
{
    // 同じGameObjectからSpriteRendererを探して保持しておく
    if (auto obj = GetGameObject()) {
        transform = obj->transform;
    }
}


void SpriteRenderer::Update(float deltaTime)
{
    // 同じGameObjectからSpriteRendererを探して保持しておく
    if (!transform.lock()) {
        auto obj = GetGameObject();
        transform = obj->transform;
    }
}

void SpriteRenderer::Gui()
{
    auto t_ = transform.lock();
    if (!t_) return;

    DirectX::XMFLOAT2 h = localSize;
    if (ImGui::DragFloat2("size", &h.x, 0.01f)) localSize = h;

    h = catSize;
    if (ImGui::DragFloat2("catSize", &h.x, 0.01f)) catSize = h;

    h = catPos;
    if (ImGui::DragFloat2("catPosLeftTop", &h.x, 0.01f)) catPos = h;

    // --- Pivot設定の追加 ---
    const char* pivotNames[] = { "Top-Left", "Center", "Bottom-Center" };
    int currentPivot = (int)pivotType;
    if (ImGui::Combo("Pivot", &currentPivot, pivotNames, IM_ARRAYSIZE(pivotNames)))
    {
        pivotType = (PosOfsset)currentPivot;
    }

    ImGui::Separator();


    // --- ファイル選択コンボボックス ---
    const char* previewValue = (selectedFileIndex >= 0) ? spriteFiles[selectedFileIndex].c_str() : "Select a sprite...";

    if (ImGui::BeginCombo("Sprite Texture", previewValue)) {
        for (int i = 0; i < (int)spriteFiles.size(); i++) {
            const bool isSelected = (selectedFileIndex == i);
            if (ImGui::Selectable(spriteFiles[i].c_str(), isSelected)) {
                selectedFileIndex = i;

                // ファイルが選ばれたら即座にロード
                std::string fullPath = "Data/Sprite/" + spriteFiles[i];
                LoadTexture(GraphicsDeviceManager::Instance().GetDevice(), fullPath.c_str()); // 前に作ったロード関数を呼び出す
            }
            ;

            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    // 更新ボタン（フォルダに新しい画像を入れた時用）
    if (ImGui::Button("Refresh Folder")) {
        RefreshSpriteFileList();
    }

    if (texture) {
        ImGui::Separator();
        ImGui::Text("Preview:");
        ImGui::Image((void*)texture.Get(), ImVec2(100, 100));
    }
}

bool SpriteRenderer::LoadTexture(ID3D11Device* device_, const std::string& filename_)
{
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv_;
    //D3D11_TEXTURE2D_DESC desc_ = {};

        // std::string を wchar_t* (std::wstring) に変換
    std::wstring wpath = std::wstring(filename_.begin(), filename_.end());

    HRESULT hr = CPUResourceLoader::LoadTextureFromFile(
        device_,
        wpath.c_str(),
        srv_.GetAddressOf(),
        &texture2dDesc
    );

    if (FAILED(hr))
    {
        // ロード失敗
        return false;
    }


    texturePath = filename_;

    textureSize.x= static_cast<float>(texture2dDesc.Width);
    textureSize.y = static_cast<float>(texture2dDesc.Height);

    // ComPtrに引き継ぎ
    texture = srv_;

    return true;
}


DirectX::XMFLOAT3 SpriteRenderer::GetPosition() const
{
    if (auto t = transform.lock())
    {
        return t->GetWorldPosition();
    }
    return { 0,0,0 };
}


DirectX::XMFLOAT2 SpriteRenderer::GetScale() const
{
    if (auto t = transform.lock())
    {
        // Transformのスケールを取得
        XMFLOAT3 ws = t->GetWorldScale();

        // SpriteRendererのローカルスケール補正を掛け合わせる
        // localSize はピクセル単位、size は倍率と考える
        return {
            ws.x * localSize.x,
            ws.y * localSize.y
        };
    }
    return { 1,1 };
}

DirectX::XMFLOAT2 SpriteRenderer::GetSize()
{
    auto t_ = transform.lock();
    if (!t_) return{1,1};

    DirectX::XMFLOAT2 size;

    size.x = localSize.x * t_->GetWorldScale().x;
    size.y = localSize.y * t_->GetWorldScale().y;

   return  size;
}

float SpriteRenderer::GetRotationZ() const
{
    if (auto t = transform.lock())
    {
        XMFLOAT3 wr = t->GetWorldRotation();
        return wr.z + localRotation; // Z軸回転を加算
    }
    return localRotation;
}
bool SpriteRenderer::GetRenderData(RenderData& outData_) const
{
    // 非表示ならスキップ
    if (!visible)
        return false;

    // Transformが存在しない場合もスキップ
    auto t_ = transform.lock();
    if (!t_)
        return false;

    // 描画データを設定
    outData_.texture = texture;
    outData_.color = color;
    outData_.worldMatrix = t_->GetWorldMatrix(); // Transformが行列を返す前提
    return true;
}

void SpriteRenderer::RefreshSpriteFileList()
{
    spriteFiles.clear();
    std::string path = "Data/Sprite/";

    if (!std::filesystem::exists(path)) return;

    for (const auto& entry : std::filesystem::directory_iterator(path)) {
        if (entry.is_regular_file()) {
            std::string ext = entry.path().extension().string();
            // 画像ファイルだけをリストに入れる
            if (ext == ".png" || ext == ".jpg" || ext == ".dds" || ext == ".tga" || ext == ".PNG") {
                spriteFiles.push_back(entry.path().filename().string());
            }
        }
    }
}


void SpriteRenderer::Serialize(nlohmann::json& j) const
{
    // パス正規化ロジックの踏襲
    std::string pathForSave = texturePath; // 読み込み時のパスを保持しておく変数が必要
    if (!pathForSave.empty()) {
        std::replace(pathForSave.begin(), pathForSave.end(), '\\', '/');
        if (pathForSave.find("./") != 0 && pathForSave.find("../") != 0) {
            pathForSave = "./" + pathForSave;
        }
    }

    j["TexturePath"] = pathForSave;
    j["Color"] = { color.x, color.y, color.z, color.w };
    j["LocalSize"] = { localSize.x, localSize.y };
    j["CatSize"] = { catSize.x, catSize.y };
    j["CatPos"] = { catPos.x, catPos.y };
    j["Visible"] = visible;
    j["ShaderID"] = static_cast<int>(spriteShaderID);
    j["pivotType"] = (int)pivotType; // 追加
}


void SpriteRenderer::Deserialize(const nlohmann::json& j)
{
    if (j.contains("TexturePath")) {
        std::string name = j["TexturePath"];
        if (!name.empty()) {
            if (name.substr(0, 2) == "./") name = name.substr(2);
            std::replace(name.begin(), name.end(), '/', '\\');

            auto device = GraphicsDeviceManager::Instance().GetDevice();
            LoadTexture(device,name);
            texturePath = name; // 再保存用に保持
        }
    }

    if (j.contains("Color")) {
        color.x = j["Color"][0];
        color.y = j["Color"][1];
        color.z = j["Color"][2];
        color.w = j["Color"][3];
    }

    if (j.contains("LocalSize")) {
        localSize.x = j["LocalSize"][0];
        localSize.y = j["LocalSize"][1];
    }

    if (j.contains("CatSize")) {
        catSize.x = j["CatSize"][0];
        catSize.y = j["CatSize"][1];
    }

    if (j.contains("CatPos")) {
        catPos.x = j["CatPos"][0];
        catPos.y = j["CatPos"][1];
    }

    if (j.contains("Visible")) {
        visible = j["Visible"].get<bool>();
    }

    if (j.contains("ShaderID")) {
        spriteShaderID = static_cast<SpriteShaderID>(j["ShaderID"].get<int>());
    }

    // Pivotの復元
    if (j.contains("pivotType")) {
        pivotType = (PosOfsset)j["pivotType"].get<int>();
    }
}
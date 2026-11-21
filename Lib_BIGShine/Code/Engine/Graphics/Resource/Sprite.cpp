#include "PrecompiledHeader.h"

#include "Sprite.h"
#include "Editor/AssetSlotGui.h"
#include "Engine/Debug/TracyProfiler.h"
#include "Engine/Graphics/Core/CPUResourceLoader.h"
#include "ImGui.h"

#include "format"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/GamePlay/Transform/Transform.h"
#include "Engine/Platform/WindowsDialogHelper.h"
#include "Engine/UI/ImGuiColorEditor.h"


void SpriteRenderer::Start()
{
    BS_PROFILE_SCOPE("SpriteRenderer::Start");

    // 同じGameObjectからSpriteRendererを探して保持しておく
    if (auto obj = GetGameObject()) {
        transform = obj->transform;
    }
    EnsureFallbackTexture(GraphicsDeviceManager::Instance().GetDevice());
}


void SpriteRenderer::Update(float deltaTime)
{
    BS_PROFILE_SCOPE("SpriteRenderer::Update");
    (void)deltaTime;

    // 同じGameObjectからSpriteRendererを探して保持しておく
    if (!transform.lock()) {
        auto obj = GetGameObject();
        transform = obj->transform;
    }

    if (!texture)
    {
        EnsureFallbackTexture(GraphicsDeviceManager::Instance().GetDevice());
    }
}

void SpriteRenderer::Gui()
{
    BS_PROFILE_SCOPE("SpriteRenderer::Gui");

    auto t_ = transform.lock();
    if (!t_)
    {
        return;
    }

    DirectX::XMFLOAT2 h = localSize;
    if (ImGui::DragFloat2("size", &h.x, 0.01f))
    {
        localSize = h;
    }

    h = catSize;
    if (ImGui::DragFloat2("catSize", &h.x, 0.01f))
    {
        catSize = h;
    }

    h = catPos;
    if (ImGui::DragFloat2("catPosLeftTop", &h.x, 0.01f))
    {
        catPos = h;
    }
    ImGuiColorEditor::DrawColorEdit4("Color", &color.x);

    // --- Pivot設定の追加 ---
    const char* pivotNames[] = { "Top-Left", "Center", "Bottom-Center" };
    int currentPivot = (int)pivotType;
    if (ImGui::Combo("Pivot", &currentPivot, pivotNames, IM_ARRAYSIZE(pivotNames)))
    {
        pivotType = (PosOfsset)currentPivot;
    }

    ImGui::Separator();


    // --- Asset Browser compatible texture slot ---
    std::string selectedTexturePath_ = texturePath;
    if (AssetSlotGui::Draw(
        "Sprite Texture",
        AssetSlotType::TEXTURE,
        selectedTexturePath_,
        texture.Get(),
        "Data/Sprite"))
    {
        if (selectedTexturePath_.empty())
        {
            texture.Reset();
            texturePath.clear();
            texture2dDesc = {};
            textureSize = { 1.0f, 1.0f };
            selectedFileIndex = -1;
        }
        else
        {
            LoadTexture(GraphicsDeviceManager::Instance().GetDevice(), selectedTexturePath_);
        }
    }

    if (texture) {
        ImGui::Separator();
        ImGui::Text("Preview:");
        ImGui::Image((void*)texture.Get(), ImVec2(100, 100));
    }
}

void SpriteRenderer::SetTextureResource(
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture_,
    const D3D11_TEXTURE2D_DESC& textureDesc_)
{
    texture = texture_;
    texture2dDesc = textureDesc_;
    textureSize =
    {
        static_cast<float>((std::max)(1u, texture2dDesc.Width)),
        static_cast<float>((std::max)(1u, texture2dDesc.Height))
    };
    catPos = { 0.0f, 0.0f };
    catSize = textureSize;
    localSize = textureSize;
}

void SpriteRenderer::SetTextureResource(Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture_,
    const D3D11_TEXTURE2D_DESC& textureDesc_, const std::string& texturePath_)
{
    SetTextureResource(texture_,textureDesc_);
    texturePath = texturePath_;
}

bool SpriteRenderer::LoadTexture(ID3D11Device* device_, const std::string& filename_)
{
    BS_PROFILE_SCOPE("SpriteRenderer::LoadTexture");

    if (filename_.empty())
    {
        texturePath.clear();
        texture.Reset();
        return EnsureFallbackTexture(device_);
    }

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv_;
    //D3D11_TEXTURE2D_DESC desc_ = {};

        // std::string を wchar_t* (std::wstring) に変換
    const std::wstring wpath_ = WindowsDialogHelper::Utf8ToPath(filename_).wstring();

    HRESULT hr = CPUResourceLoader::LoadTextureFromFile(
        device_,
        wpath_.c_str(),
        srv_.GetAddressOf(),
        &texture2dDesc,
        sdrTexture ? TextureType::Data : TextureType::Color
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

bool SpriteRenderer::EnsureFallbackTexture(ID3D11Device* device_)
{
    BS_PROFILE_SCOPE("SpriteRenderer::EnsureFallbackTexture");

    if (texture)
    {
        return true;
    }
    if (!device_)
    {
        return false;
    }

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> fallbackTexture_;
    if (FAILED(CPUResourceLoader::CreateDummyTexture(
        device_,
        0xFFFFFFFFu,
        fallbackTexture_.GetAddressOf())))
    {
        return false;
    }

    texture = fallbackTexture_;
    texture2dDesc = {};
    texture2dDesc.Width = 1;
    texture2dDesc.Height = 1;
    texture2dDesc.MipLevels = 1;
    texture2dDesc.ArraySize = 1;
    texture2dDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texture2dDesc.SampleDesc.Count = 1;
    texture2dDesc.Usage = D3D11_USAGE_IMMUTABLE;
    texture2dDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    textureSize = { 1.0f, 1.0f };
    catPos = { 0.0f, 0.0f };
    catSize = { 1.0f, 1.0f };
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
    if (!t_)
    {
        return{1,1};
    }

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
    if (!IsVisible())
    {
        return false;
    }

    // Transformが存在しない場合もスキップ
    auto t_ = transform.lock();
    if (!t_)
    {
        return false;
    }

    // 描画データを設定
    outData_.texture = texture;
    outData_.color = GetColor();
    outData_.worldMatrix = t_->GetWorldMatrix(); // Transformが行列を返す前提
    return true;
}

void SpriteRenderer::RefreshSpriteFileList()
{
    BS_PROFILE_SCOPE("SpriteRenderer::RefreshSpriteFileList");

    spriteFiles.clear();
    std::string path = "Data/Sprite/";

    if (!std::filesystem::exists(path))
    {
        return;
    }

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
    BS_PROFILE_SCOPE("SpriteRenderer::Serialize");

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
    j["LinearSampling"] = linearSampling;
    j["SdrTexture"] = sdrTexture;
    j["ShaderID"] = static_cast<int>(spriteShaderID);
    j["pivotType"] = (int)pivotType; // 追加
}


void SpriteRenderer::Deserialize(const nlohmann::json& j)
{
    BS_PROFILE_SCOPE("SpriteRenderer::Deserialize");
    hasRuntimeColor = false;
    hasRuntimeVisibility = false;
    linearSampling = j.value("LinearSampling", false);
    sdrTexture = j.value("SdrTexture", false);

    if (j.contains("TexturePath")) {
        std::string name = j["TexturePath"];
        if (!name.empty()) {
            if (name.substr(0, 2) == "./")
            {
                name = name.substr(2);
            }
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

#pragma once

#include <string>
#include <memory>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <d3d11.h>
#include <vector>

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/Graphics/Shader/SpriteShaderID.h"
#include "Engine/GamePlay/Transform/Transform.h"

enum class PosOfsset
{
    TOP_L,
    MID,
    UNDER_M
};

//=========================================
// SpriteRenderer (画像描画用コンポーネント)
//=========================================
class SpriteRenderer : public Component
{
public:
    struct RenderData
    {
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture;
        DirectX::XMFLOAT4 color;
        DirectX::XMMATRIX worldMatrix;
    };

public:
    void Start();
    void Update(float deltaTime);
    void Gui();

    const std::string& GetName() const override
    {
        static std::string name = "SpriteRenderer";
        return name;
    }

    bool LoadTexture(ID3D11Device* device_, const std::string& filename_);

    // ===== セッター  =====
    void SetTexture(Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> tex) { texture = tex; }
    void SetColor(const DirectX::XMFLOAT4& c) { color = c; }
    void SetVisible(bool v) { visible = v; }
    void SetLocalSize(const DirectX::XMFLOAT2& s) { localSize = s; }
    void SetPivot(PosOfsset p) { pivotType = p; }
    void SetUVRect(float x_, float y_, float width_, float height_) {
        catPos = { x_, y_ };
        catSize = { width_, height_ };
    }

    void SetUVGridIndex(int index_, int columns_, const DirectX::XMFLOAT2& cellSize_) {
        if (columns_ < 1) columns_ = 1;
        const int column_ = index_ % columns_;
        const int row_ = index_ / columns_;
        SetUVRect(cellSize_.x * column_, cellSize_.y * row_, cellSize_.x, cellSize_.y);
    }

    // ===== ゲッター  =====
    std::shared_ptr<Transform> GetTransform() const { return transform.lock(); }
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetTexture() const { return texture; }
    D3D11_TEXTURE2D_DESC GetTexture2dDesc() { return texture2dDesc; }
    bool IsVisible() { return visible; };
    DirectX::XMFLOAT4 GetColor() const { return color; }
    PosOfsset GetPivot() const { return pivotType; }
    // 実際の描画位置（ワールド座標）
    DirectX::XMFLOAT3 GetPosition() const;
    // 実際の描画スケール（Transform × Spriteのサイズ）
    DirectX::XMFLOAT2 GetScale() const;
    DirectX::XMFLOAT2 GetSize();

    DirectX::XMFLOAT2 GetTextureSize() { return textureSize; };
    DirectX::XMFLOAT2 GetCatSize() { return catSize; };
    DirectX::XMFLOAT2 GetCatPos() { return catPos; };
    // 実際の描画回転（Transform × Spriteのローカル回転）
    float GetRotationZ() const;


    // RenderSystemに渡す描画データを生成
    bool GetRenderData(RenderData& outData_) const;

    void SetSpriteShaderID(SpriteShaderID id_) { spriteShaderID = id_; }
    SpriteShaderID GetSpriteShaderID() { return spriteShaderID; }

    void RefreshSpriteFileList();
    void Serialize(nlohmann::json& j) const;

    void Deserialize(const nlohmann::json& j);
 
private:
    // Sprite 独自設定
    DirectX::XMFLOAT2 localSize{ 1280,710 };//描画時のスクリーン座標の大きさ
    DirectX::XMFLOAT2 catSize{ 350,350 };//描画時のスクリーン座標の大きさ
    DirectX::XMFLOAT2 catPos{ 0,0 };//描画時のスクリーン座標の大きさ
    DirectX::XMFLOAT2 textureSize;
    float localRotation = 0;   

    DirectX::XMFLOAT4 color{ 1,1,1,1 };
    DirectX::XMFLOAT2 size = { 1, 1 };      // ピクセル単位のサイズ
    SpriteShaderID spriteShaderID = SpriteShaderID::Default;
    bool visible = true;

    std::weak_ptr<Transform> transform;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture;
    D3D11_TEXTURE2D_DESC texture2dDesc;

    PosOfsset pivotType = PosOfsset::TOP_L; // デフォルト左上s

    std::string texturePath;
    std::vector<std::string> spriteFiles;
    int selectedFileIndex = -1;
};
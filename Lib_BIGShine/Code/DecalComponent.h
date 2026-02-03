#pragma once

#include "ComponentBase.h"
#include "DecalManager.h"

class DecalComponent : public Component
{
public:
    // シェーダーの b10 と一致させる構造体
    struct DecalConstantBuffer {
        DirectX::XMMATRIX decal_inverse_transform;
        DirectX::XMFLOAT4 decal_direction; // 必要に応じて使用（法線減衰など）
    };
    const std::string& GetName() const override { return componentName; }

    // シーンの decal_textures に相当する SRV
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture;

    // 投影する色（アルファは透明度）
    DirectX::XMFLOAT4 color{ 1.0f, 1.0f, 1.0f, 1.0f };

    // 投影計算に使用する行列（RenderSystemに渡す用）
    DirectX::XMFLOAT4X4 invWorldMatrix;

    void Start() override
    {
        // リストが空なら初期スキャンを行う
        if (decalFiles.empty()) {
            RefreshDecalFileList();
        }
    }

    void SetTexture(const std::string& path);

    // LightComponentのように専用の登録関数を作成
    void SubmitDecal(DecalManager& manager);

    void Update(float et) override;

    void Gui() override;

    void Serialize(nlohmann::json& j) const;
    // ロード用：引数の json オブジェクトから自分の変数を読み出す
    void Deserialize(const nlohmann::json& j);

private:
    bool showDebugBounds = true;
    // コンポーネント名の定義
    static inline const std::string componentName = "DecalComponent";
    std::vector<std::string> decalFiles;
    int selectedFileIndex = -1;

    // 起動時やボタンが押された時にフォルダをスキャンする関数
    void RefreshDecalFileList();
};
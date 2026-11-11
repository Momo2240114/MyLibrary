#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/Graphics/Resource/ModelMaterial.h"
#include <map>

// 所有Objectのモデルへ、全体、マテリアル番号別、パーツ別のテクスチャ差し替えを適用する。
class ModelMaterialComponent : public Component
{
public:
    const std::string& GetName() const override
    {
        static const std::string COMPONENT_NAME = "ModelMaterialComponent";
        return COMPONENT_NAME;
    }

    // 初期設定に含まれるテクスチャの読み込みを開始する。
    void Start() override;
    // デバイス準備後に、未ロードの差し替えテクスチャを読み込む。
    void UpdateLoadState() override;
    // パーツまたはマテリアル番号を選び、各テクスチャスロットの設定をEditorで編集する。
    void Gui() override;
    // 適用先のマテリアル番号、メッシュ番号とテクスチャパスを保存する。
    void Serialize(nlohmann::json& json_) const override;
    // 保存済みの設定を復元し、GPUリソースの読み込みを予約する。
    void Deserialize(const nlohmann::json& json_) override;

    // -1は全マテリアル共通の設定。0以上はモデル内のマテリアル番号を指定する。
    ModelMaterial& GetMaterial(int materialIndex_ = -1);
    // 個別設定を解除し、共通設定または元のモデルのテクスチャへ戻す。
    void RemoveMaterial(int materialIndex_);
    // 同じマテリアルを共有するパーツも、メッシュ番号別に独立した設定を保持する。
    ModelMaterial& GetMeshMaterial(int meshIndex_);
    // パーツの設定を解除し、マテリアル番号別または共通設定へ戻す。
    void RemoveMeshMaterial(int meshIndex_);
    // パーツ、マテリアル番号、共通設定、元のモデルの順でスロットごとに選ぶ。
    ID3D11ShaderResourceView* ResolveTexture(
        int materialIndex_, ModelMaterial::TextureSlot slot_,
        ID3D11ShaderResourceView* originalTexture_, int meshIndex_ = -1) const;
    // 採用されたテクスチャと同じ設定元から参照チャンネルを選ぶ。
    ModelMaterial::TextureChannel ResolveChannel(
        int materialIndex_, ModelMaterial::MaterialValue value_, int meshIndex_ = -1) const;

private:
    // 読み込み済みの差し替えを優先順で探し、画像とチャンネルの設定元を揃える。
    const ModelMaterial* FindTextureOverride(
        int materialIndex_, ModelMaterial::TextureSlot slot_, int meshIndex_) const;
    std::map<int, ModelMaterial> materials;
    std::map<int, ModelMaterial> meshMaterials; // メッシュごとの設定。元のモデルや共有マテリアルは変更しない。
    int selectedMaterialIndex = -1; // GUIで編集中のマテリアル番号。保存対象には含めない。
    int selectedMeshIndex = -1; // -1はマテリアル番号別の編集、それ以外はパーツ別の編集。
};


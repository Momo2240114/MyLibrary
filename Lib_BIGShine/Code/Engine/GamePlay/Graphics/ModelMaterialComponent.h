#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/Graphics/Resource/ModelMaterial.h"
#include <map>

// 所有Objectのモデルへ、全体またはマテリアル番号別のテクスチャ差し替えを適用する。
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
    // マテリアル番号と各テクスチャスロットの設定をEditorで編集する。
    void Gui() override;
    // 適用先のマテリアル番号とテクスチャパスを保存する。
    void Serialize(nlohmann::json& json_) const override;
    // 保存済みの設定を復元し、GPUリソースの読み込みを予約する。
    void Deserialize(const nlohmann::json& json_) override;

    // -1は全マテリアル共通の設定。0以上はモデル内のマテリアル番号を指定する。
    ModelMaterial& GetMaterial(int materialIndex_ = -1);
    // 個別設定を解除し、共通設定または元のモデルのテクスチャへ戻す。
    void RemoveMaterial(int materialIndex_);
    // 個別設定、共通設定、元のモデルの順で、使用可能なテクスチャを選ぶ。
    ID3D11ShaderResourceView* ResolveTexture(
        int materialIndex_, ModelMaterial::TextureSlot slot_,
        ID3D11ShaderResourceView* originalTexture_) const;

private:
    std::map<int, ModelMaterial> materials;
    int selectedMaterialIndex = -1; // GUIで編集中のマテリアル番号。保存対象には含めない。
};
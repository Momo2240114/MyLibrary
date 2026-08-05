#pragma once

#include "EditorWindow.h"

// EffectManagerが認識しているEffect系Assetを一覧し、Keyと実ファイルの対応を確認するWindow。
class EffectAssetListWindow : public EditorWindow
{
public:
    explicit EffectAssetListWindow(GameObjectManager* manager_);

    void Draw() override;

private:
    // Effectの用途ごとに一覧を分け、Particle/MeshVFX/Sequence/Impactを混ぜずに確認する。
    enum class EFFECT_ASSET_LIST_TYPE
    {
        PARTICLE,
        MESH_VFX,
        VFX_SEQUENCE,
        IMPACT_EFFECT,
    };

    // 指定種類のAsset一覧をEffectManagerから取り出し、Key/File/FullPathの表として描画する。
    void DrawAssetTable(EFFECT_ASSET_LIST_TYPE type_);
};

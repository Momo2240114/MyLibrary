#pragma once

#include "EditorWindow.h"

class EffectAssetListWindow : public EditorWindow
{
public:
    explicit EffectAssetListWindow(GameObjectManager* manager_);

    void Draw() override;

private:
    enum class EFFECT_ASSET_LIST_TYPE
    {
        PARTICLE,
        MESH_VFX,
        VFX_SEQUENCE,
        IMPACT_EFFECT,
    };

    void DrawAssetTable(EFFECT_ASSET_LIST_TYPE type_);
};

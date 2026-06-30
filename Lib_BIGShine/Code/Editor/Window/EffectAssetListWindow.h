#pragma once

#include "EditorWindow.h"

class EffectAssetListWindow : public EditorWindow
{
public:
    explicit EffectAssetListWindow(GameObjectManager* manager_);

    void Draw() override;

private:
    void DrawAssetTable(bool isParticle_);
};

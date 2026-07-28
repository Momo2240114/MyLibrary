#pragma once

#include "EditorWindow.h"
#include "Engine/Graphics/VFXMaterial/VFXMaterialResource.h"

#include <memory>
#include <string>
#include <vector>

class VFXMaterialEditorWindow : public EditorWindow
{
public:
    explicit VFXMaterialEditorWindow(GameObjectManager* manager_);
    void Draw() override;

private:
    struct MaterialEditorSlot
    {
        std::shared_ptr<VFXMaterialResource> materialResource;
        std::string materialPath;
    };

    void DrawMaterialToolbar();
    void DrawMaterialEditor();
    void DrawUvEditor(const char* label_, VFXMaterialUVData& uv_);
    void DrawColorRampEditor(VFXMaterialColorRampData& colorRamp_);
    void DrawRandomColorEditor(VFXMaterialRandomColorData& randomColor_);
    void LoadMaterial(const std::string& path_);
    void SaveMaterial(const std::string& path_);
    void EnsureEditableMaterial();
    void SyncSelectedMaterialSlot();
    void StoreSelectedMaterialSlot();

private:
    std::vector<MaterialEditorSlot> materialSlots;
    std::shared_ptr<VFXMaterialResource> materialResource;
    std::string materialPath;
    int selectedMaterialIndex = 0;
};

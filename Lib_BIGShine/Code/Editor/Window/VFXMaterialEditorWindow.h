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
        // 複数Materialを切り替えながら編集するための1スロット分の状態。
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
    // 現在Inspectorで編集しているMaterial。選択スロットと同期して保存/読込する。
    std::shared_ptr<VFXMaterialResource> materialResource;
    std::string materialPath;
    int selectedMaterialIndex = 0;
};

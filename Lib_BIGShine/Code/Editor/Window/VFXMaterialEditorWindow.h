#pragma once

#include "EditorWindow.h"
#include "Engine/Graphics/VFXMaterial/VFXMaterialResource.h"

#include <memory>
#include <string>

class VFXMaterialEditorWindow : public EditorWindow
{
public:
    explicit VFXMaterialEditorWindow(GameObjectManager* manager_);
    void Draw() override;

private:
    void DrawMaterialToolbar();
    void DrawMaterialEditor();
    void DrawUvEditor(const char* label_, VFXMaterialUVData& uv_);
    void LoadMaterial(const std::string& path_);
    void SaveMaterial(const std::string& path_);
    void EnsureEditableMaterial();

private:
    std::shared_ptr<VFXMaterialResource> materialResource;
    std::string materialPath;
};

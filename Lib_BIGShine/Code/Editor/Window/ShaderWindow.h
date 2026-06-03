#pragma once

#include "EditorWindow.h" 

class ModelRenderSystem;

class ShaderWindow : public EditorWindow
{
public:
    ShaderWindow(const std::string& name, GameObjectManager* manager,ModelRenderSystem& renderSystem)
        : EditorWindow("Shader" , manager), modelRenderSystem(renderSystem)
    {
    }

    void Draw() override;

private:
    ModelRenderSystem& modelRenderSystem;
};

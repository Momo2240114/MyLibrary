#pragma once

#include "EditorWindow.h" 

class ModelRenderSystem;

// ModelRenderSystemが持つShader関連のDebug/切り替え設定を表示するWindow。
class ShaderWindow : public EditorWindow
{
public:
    ShaderWindow(const std::string& name, GameObjectManager* manager,ModelRenderSystem& renderSystem)
        : EditorWindow("Shader" , manager), modelRenderSystem(renderSystem)
    {
    }

    void Draw() override;

private:
    // Shader設定は描画パイプライン側の状態と直結するため、ModelRenderSystemを直接参照する。
    ModelRenderSystem& modelRenderSystem;
};

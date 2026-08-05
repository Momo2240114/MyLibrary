#pragma once

#include "EditorWindow.h" 

class ModelRenderSystem;

// ModelRenderSystemが持つShadowMapや影描画設定をEditorから調整するWindow。
class ShadowWindow : public EditorWindow
{
public:
    ShadowWindow(const std::string& name, GameObjectManager* manager, ModelRenderSystem& renderSystem)
        : EditorWindow("Shadow", manager), modelRenderSystem(renderSystem)
    {
    }

    void Draw() override;

private:
    // 影設定は描画Systemの内部状態なので、Windowは参照を保持してGui呼び出しだけを行う。
    ModelRenderSystem& modelRenderSystem;
};

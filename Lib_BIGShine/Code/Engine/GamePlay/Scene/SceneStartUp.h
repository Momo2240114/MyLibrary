#pragma once

#include "IScene.h"
#include "Engine/Graphics/Resource/ModelLoadTypes.h"

class SceneStartUp : public Scene
{
public:
    SceneStartUp()
    {
        sceneName = "StartUpScene";
    }

    void Initialize() override;
    void Finalize() override;
    void Update(float elapsedTime_) override;
    void Render(float elapsedTime_) override;
    void DrawGUI() override;

private:
    PreloadHandle criticalPreloadHandle;
    float elapsedSeconds = 0.0f;
    bool titleSceneRequested = false;
    bool startupLoadFailed = false;
};

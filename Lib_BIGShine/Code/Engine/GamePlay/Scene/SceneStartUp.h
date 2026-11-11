#pragma once

#include "SceneBase.h"
#include "Engine/Graphics/Resource/ModelLoadTypes.h"

class SceneStartUp : public SceneBase
{
public:
    SceneStartUp()
    {
        sceneName = "StartupPresentation";
        isLoadingPresentation = true;
    }

    void Initialize() override;
    void Finalize() override;
    void Update(float elapsedTime_) override;
    void Render(float elapsedTime_) override;
    void DrawGUI() override;

private:
    // 最初のシーンへ進む前に必ず完了させるCriticalモデル先読みハンドル。
    PreloadHandle criticalPreloadHandle;

    // Loading表示の簡単なアニメーション用経過時間。
    float elapsedSeconds = 0.0f;

    // ChangeSceneを二重に呼ばないためのフラグ。
    bool titleSceneRequested = false;

    // Criticalロード失敗時にGUIへ失敗表示を出す。
    bool startupLoadFailed = false;
};

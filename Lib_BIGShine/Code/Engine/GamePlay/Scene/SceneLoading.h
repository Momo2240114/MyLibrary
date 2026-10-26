#pragma once
#include "SceneBase.h"
#include "LoadingTypes.h"

// 独立した表示Sceneを生存させながら、次SceneのCPU読込と主スレッド準備を進める。
class SceneLoading : public SceneBase
{
public:
    explicit SceneLoading(std::unique_ptr<SceneBase> next_);
    ~SceneLoading() override;
    void Initialize() override;
    void Finalize() override;
    void Update(float deltaTime_) override;
    void Render(float deltaTime_) override;
    void DrawGUI() override;
    bool IsEnginePresentation() const { return enginePresentation; }
    LoadingStatus GetLoadingStatus() const { return loadingStatus; }
    // 完了後の演出が終了した時に呼ぶ。自動遷移を止めた用途にも対応する。
    void RequestTransition();
    void SetAutomaticTransition(bool enabled_) { automaticTransition = enabled_; }
    void SetCompletionHoldDuration(float duration_);
    void SetSilhouetteMode(LOADING_SILHOUETTE_MODE mode_) { silhouetteMode = mode_; silhouetteModeOverridden = true; }

private:
    void FailLoading(const std::string& error_);
    void Retry();
    std::unique_ptr<SceneBase> nextScene;
    LoadingStatus loadingStatus;
    LOADING_SILHOUETTE_MODE silhouetteMode = LOADING_SILHOUETTE_MODE::WHITE;
    bool nextInitialized = false;
    bool transferRequested = false;
    bool transferQueued = false;
    bool automaticTransition = true;
    float completionHoldDuration = 0.25f;
    float completedTime = 0.0f;
    // 外部から明示された色指定だけをScene JSONより優先する。
    bool silhouetteModeOverridden = false;
    std::string presentationWarning;
    bool enginePresentation = false;
};

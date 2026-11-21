#pragma once
#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "ScreenTransitionManager.h"
#include <functional>

// GameObjectから汎用トランジションを要求し、Sceneやゲーム固有処理をActionへ委譲する。
class ScreenTransitionComponent : public Component
{
public:
    const std::string& GetName() const override;
    void Start() override;
    void OnDestroy() override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;
    bool Cover(std::function<void()> onCovered_ = {});
    bool Reveal();
    bool Play(std::function<void()> onCovered_ = {},std::function<void()> onCompleted_ = {});
    void Hold();
    void CompleteWork();
    void Cancel(TransitionCancelMode mode_ = TransitionCancelMode::CLEAR);
    bool IsCovered() const;

    TransitionHandle GetHandle() const
    {
        return handle;
    }

private:
    bool Begin(TransitionPlayback mode_,std::function<void()> onCovered_,std::function<void()> onCompleted_);
    TransitionParameters parameters;
    std::string preset = "DefaultFade";
    std::string coveredAction;
    std::string completedAction;
    bool usePreset = true;
    bool playOnStart = false;
    bool waitForCompletion = false;
    int priority = 0;
    TransitionHandle handle = 0;
};

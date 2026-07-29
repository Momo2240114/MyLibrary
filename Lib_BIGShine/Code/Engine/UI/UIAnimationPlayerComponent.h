#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"

#include <string>
#include <vector>
#include <memory>

class SpriteRenderer;
class UIWidgetComponent;
class UIVertexModifierComponent;

enum class UI_ANIMATION_PROPERTY
{
    ANCHORED_POSITION_X,
    ANCHORED_POSITION_Y,
    SIZE_X,
    SIZE_Y,
    SCALE_X,
    SCALE_Y,
    ROTATION,
    COLOR_R,
    COLOR_G,
    COLOR_B,
    COLOR_A,
    VERTEX_LEFT_TOP_POSITION_X,
    VERTEX_LEFT_TOP_POSITION_Y,
    VERTEX_RIGHT_TOP_POSITION_X,
    VERTEX_RIGHT_TOP_POSITION_Y,
    VERTEX_LEFT_BOTTOM_POSITION_X,
    VERTEX_LEFT_BOTTOM_POSITION_Y,
    VERTEX_RIGHT_BOTTOM_POSITION_X,
    VERTEX_RIGHT_BOTTOM_POSITION_Y,
    VERTEX_LEFT_TOP_COLOR_R,
    VERTEX_LEFT_TOP_COLOR_G,
    VERTEX_LEFT_TOP_COLOR_B,
    VERTEX_LEFT_TOP_COLOR_A,
    VERTEX_RIGHT_TOP_COLOR_R,
    VERTEX_RIGHT_TOP_COLOR_G,
    VERTEX_RIGHT_TOP_COLOR_B,
    VERTEX_RIGHT_TOP_COLOR_A,
    VERTEX_LEFT_BOTTOM_COLOR_R,
    VERTEX_LEFT_BOTTOM_COLOR_G,
    VERTEX_LEFT_BOTTOM_COLOR_B,
    VERTEX_LEFT_BOTTOM_COLOR_A,
    VERTEX_RIGHT_BOTTOM_COLOR_R,
    VERTEX_RIGHT_BOTTOM_COLOR_G,
    VERTEX_RIGHT_BOTTOM_COLOR_B,
    VERTEX_RIGHT_BOTTOM_COLOR_A,
    VISIBLE
};

enum class UI_ANIMATION_VALUE_MODE
{
    VALUE_ABSOLUTE,
    VALUE_RELATIVE_TO_INITIAL,
    VALUE_ADDITIVE
};

enum class UI_ANIMATION_STOP_MODE
{
    KEEP_CURRENT,
    RESTORE_INITIAL,
    MOVE_TO_END
};

enum class UI_ANIMATION_EASING
{
    LINEAR,
    EASE_IN_CUBIC,
    EASE_OUT_CUBIC,
    EASE_IN_OUT_CUBIC,
    EASE_OUT_BACK
};

struct UIAnimationKeyframe
{
    float time = 0.0f;
    float value = 0.0f;
    UI_ANIMATION_EASING easingType = UI_ANIMATION_EASING::LINEAR;
};

struct UIAnimationTrack
{
    std::string trackName;
    std::string targetWidgetId;
    UI_ANIMATION_PROPERTY property = UI_ANIMATION_PROPERTY::ANCHORED_POSITION_X;
    UI_ANIMATION_VALUE_MODE valueMode = UI_ANIMATION_VALUE_MODE::VALUE_ABSOLUTE;
    bool enabled = true;
    std::vector<UIAnimationKeyframe> keyframes;
    float initialValue = 0.0f;
    std::weak_ptr<UIWidgetComponent> targetWidget;
    std::weak_ptr<SpriteRenderer> targetSprite;
    std::weak_ptr<UIVertexModifierComponent> targetVertexModifier;
};

class UIAnimationPlayerComponent : public Component
{
public:
    void Start() override;
    void Update(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    const std::string& GetName() const override
    {
        static std::string name = "UIAnimationPlayerComponent";
        return name;
    }

    bool LoadAnimation(const std::string& assetPath_);
    void Play(bool restart_ = true, float playbackSpeed_ = -1.0f);
    void Stop(UI_ANIMATION_STOP_MODE stopMode_);
    void Pause();
    void Resume();
    void Seek(float time_);

private:
    void RegisterActions();
    void ResolveTrackTargets();
    void CaptureInitialValues();
    float EvaluateTrack(const UIAnimationTrack& track_) const;
    float ApplyValueMode(const UIAnimationTrack& track_, float sampledValue_) const;
    void ApplyTrackValue(const UIAnimationTrack& track_, float value_);
    bool TryGetCurrentValue(const UIAnimationTrack& track_, float& value_) const;

    std::string animationName = "UIAnimation";
    std::string animationAssetPath = "Data/UI/Animations/";
    std::string actionPrefix;
    float duration = 0.0f;
    float playbackSpeed = 1.0f;
    float currentTime = 0.0f;
    bool loop = false;
    bool playing = false;
    bool paused = false;
    bool registerGlobalActions = true;
    UI_ANIMATION_STOP_MODE stopMode = UI_ANIMATION_STOP_MODE::KEEP_CURRENT;
    std::vector<UIAnimationTrack> tracks;
};

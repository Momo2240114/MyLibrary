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
    // UIWidgetComponentのアンカー座標。
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

    // UIVertexModifierComponentの頂点別位置補正。
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

    // Widgetの表示/非表示を切り替える。
    VISIBLE
};

enum class UI_ANIMATION_VALUE_MODE
{
    // Keyframe値をそのまま適用する。
    VALUE_ABSOLUTE,

    // 再生開始時の値を基準に相対値として適用する。
    VALUE_RELATIVE_TO_INITIAL,

    // 現在値へ加算する。
    VALUE_ADDITIVE
};

enum class UI_ANIMATION_STOP_MODE
{
    // Stop時点の値を保持する。
    KEEP_CURRENT,

    // 再生開始時の値へ戻す。
    RESTORE_INITIAL,

    // 最終Keyframeの値へ移動する。
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
    // アニメーション内の秒数。
    float time = 0.0f;

    // 対象Propertyへ適用する値。
    float value = 0.0f;
    UI_ANIMATION_EASING easingType = UI_ANIMATION_EASING::LINEAR;
};

struct UIAnimationTrack
{
    // Editor表示用のTrack名。
    std::string trackName;

    // 対象WidgetのID。ResolveTrackTargetsで参照へ変換する。
    std::string targetWidgetId;
    UI_ANIMATION_PROPERTY property = UI_ANIMATION_PROPERTY::ANCHORED_POSITION_X;
    UI_ANIMATION_VALUE_MODE valueMode = UI_ANIMATION_VALUE_MODE::VALUE_ABSOLUTE;
    bool enabled = true;
    std::vector<UIAnimationKeyframe> keyframes;

    // VALUE_RELATIVE_TO_INITIALやRESTORE_INITIALで使う再生開始時の値。
    float initialValue = 0.0f;

    // 対象Component参照。Propertyに応じて必要なものだけ使う。
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

    // restart_がtrueなら先頭から、falseなら現在時刻から再生する。
    void Play(bool restart_ = true, float playbackSpeed_ = -1.0f);
    void Stop(UI_ANIMATION_STOP_MODE stopMode_);
    void Pause();
    void Resume();

    // TimelineやEditorから任意時刻へ移動する。
    void Seek(float time_);

private:
    // ActionSequenceからPlay/Stop/Pauseなどを呼べるよう登録する。
    void RegisterActions();

    // targetWidgetIdからUIWidget/Sprite/VertexModifierを探してTrackへ保持する。
    void ResolveTrackTargets();

    // 再生開始時の各Property値をTrack.initialValueへ保存する。
    void CaptureInitialValues();

    // 現在時刻におけるTrack値をKeyframe補間で求める。
    float EvaluateTrack(const UIAnimationTrack& track_) const;
    float ApplyValueMode(const UIAnimationTrack& track_, float sampledValue_) const;
    void ApplyTrackValue(const UIAnimationTrack& track_, float value_);
    bool TryGetCurrentValue(const UIAnimationTrack& track_, float& value_) const;

    // 読み込んだAnimation名とAssetパス。
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

    // 複数Propertyを同じ時間軸で再生するTrack群。
    std::vector<UIAnimationTrack> tracks;
};

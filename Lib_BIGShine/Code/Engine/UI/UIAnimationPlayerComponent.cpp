#include "PrecompiledHeader.h"

#include "UIAnimationPlayerComponent.h"

#include "Engine/Debug/TracyProfiler.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/Graphics/Resource/Sprite.h"
#include "Engine/UI/UIVertexModifierComponent.h"
#include "Engine/UI/UIWidgetComponent.h"
#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <unordered_map>

namespace
{
    const std::unordered_map<std::string, UI_ANIMATION_PROPERTY> PROPERTY_MAP =
    {
        { "ANCHORED_POSITION_X", UI_ANIMATION_PROPERTY::ANCHORED_POSITION_X },
        { "ANCHORED_POSITION_Y", UI_ANIMATION_PROPERTY::ANCHORED_POSITION_Y },
        { "SIZE_X", UI_ANIMATION_PROPERTY::SIZE_X },
        { "SIZE_Y", UI_ANIMATION_PROPERTY::SIZE_Y },
        { "SCALE_X", UI_ANIMATION_PROPERTY::SCALE_X },
        { "SCALE_Y", UI_ANIMATION_PROPERTY::SCALE_Y },
        { "ROTATION", UI_ANIMATION_PROPERTY::ROTATION },
        { "COLOR_R", UI_ANIMATION_PROPERTY::COLOR_R },
        { "COLOR_G", UI_ANIMATION_PROPERTY::COLOR_G },
        { "COLOR_B", UI_ANIMATION_PROPERTY::COLOR_B },
        { "COLOR_A", UI_ANIMATION_PROPERTY::COLOR_A },
        { "VERTEX_LEFT_TOP_POSITION_X", UI_ANIMATION_PROPERTY::VERTEX_LEFT_TOP_POSITION_X },
        { "VERTEX_LEFT_TOP_POSITION_Y", UI_ANIMATION_PROPERTY::VERTEX_LEFT_TOP_POSITION_Y },
        { "VERTEX_RIGHT_TOP_POSITION_X", UI_ANIMATION_PROPERTY::VERTEX_RIGHT_TOP_POSITION_X },
        { "VERTEX_RIGHT_TOP_POSITION_Y", UI_ANIMATION_PROPERTY::VERTEX_RIGHT_TOP_POSITION_Y },
        { "VERTEX_LEFT_BOTTOM_POSITION_X", UI_ANIMATION_PROPERTY::VERTEX_LEFT_BOTTOM_POSITION_X },
        { "VERTEX_LEFT_BOTTOM_POSITION_Y", UI_ANIMATION_PROPERTY::VERTEX_LEFT_BOTTOM_POSITION_Y },
        { "VERTEX_RIGHT_BOTTOM_POSITION_X", UI_ANIMATION_PROPERTY::VERTEX_RIGHT_BOTTOM_POSITION_X },
        { "VERTEX_RIGHT_BOTTOM_POSITION_Y", UI_ANIMATION_PROPERTY::VERTEX_RIGHT_BOTTOM_POSITION_Y },
        { "VERTEX_LEFT_TOP_COLOR_R", UI_ANIMATION_PROPERTY::VERTEX_LEFT_TOP_COLOR_R },
        { "VERTEX_LEFT_TOP_COLOR_G", UI_ANIMATION_PROPERTY::VERTEX_LEFT_TOP_COLOR_G },
        { "VERTEX_LEFT_TOP_COLOR_B", UI_ANIMATION_PROPERTY::VERTEX_LEFT_TOP_COLOR_B },
        { "VERTEX_LEFT_TOP_COLOR_A", UI_ANIMATION_PROPERTY::VERTEX_LEFT_TOP_COLOR_A },
        { "VERTEX_RIGHT_TOP_COLOR_R", UI_ANIMATION_PROPERTY::VERTEX_RIGHT_TOP_COLOR_R },
        { "VERTEX_RIGHT_TOP_COLOR_G", UI_ANIMATION_PROPERTY::VERTEX_RIGHT_TOP_COLOR_G },
        { "VERTEX_RIGHT_TOP_COLOR_B", UI_ANIMATION_PROPERTY::VERTEX_RIGHT_TOP_COLOR_B },
        { "VERTEX_RIGHT_TOP_COLOR_A", UI_ANIMATION_PROPERTY::VERTEX_RIGHT_TOP_COLOR_A },
        { "VERTEX_LEFT_BOTTOM_COLOR_R", UI_ANIMATION_PROPERTY::VERTEX_LEFT_BOTTOM_COLOR_R },
        { "VERTEX_LEFT_BOTTOM_COLOR_G", UI_ANIMATION_PROPERTY::VERTEX_LEFT_BOTTOM_COLOR_G },
        { "VERTEX_LEFT_BOTTOM_COLOR_B", UI_ANIMATION_PROPERTY::VERTEX_LEFT_BOTTOM_COLOR_B },
        { "VERTEX_LEFT_BOTTOM_COLOR_A", UI_ANIMATION_PROPERTY::VERTEX_LEFT_BOTTOM_COLOR_A },
        { "VERTEX_RIGHT_BOTTOM_COLOR_R", UI_ANIMATION_PROPERTY::VERTEX_RIGHT_BOTTOM_COLOR_R },
        { "VERTEX_RIGHT_BOTTOM_COLOR_G", UI_ANIMATION_PROPERTY::VERTEX_RIGHT_BOTTOM_COLOR_G },
        { "VERTEX_RIGHT_BOTTOM_COLOR_B", UI_ANIMATION_PROPERTY::VERTEX_RIGHT_BOTTOM_COLOR_B },
        { "VERTEX_RIGHT_BOTTOM_COLOR_A", UI_ANIMATION_PROPERTY::VERTEX_RIGHT_BOTTOM_COLOR_A },
        { "VISIBLE", UI_ANIMATION_PROPERTY::VISIBLE }
    };

    UI_ANIMATION_PROPERTY PropertyFromString(const std::string& name_)
    {
        const auto iterator_ = PROPERTY_MAP.find(name_);
        if (iterator_ == PROPERTY_MAP.end()) return UI_ANIMATION_PROPERTY::ANCHORED_POSITION_X;
        return iterator_->second;
    }

    const char* ToString(UI_ANIMATION_STOP_MODE mode_)
    {
        switch (mode_)
        {
        case UI_ANIMATION_STOP_MODE::RESTORE_INITIAL: return "RESTORE_INITIAL";
        case UI_ANIMATION_STOP_MODE::MOVE_TO_END: return "MOVE_TO_END";
        case UI_ANIMATION_STOP_MODE::KEEP_CURRENT:
        default: return "KEEP_CURRENT";
        }
    }

    UI_ANIMATION_STOP_MODE StopModeFromString(const std::string& name_)
    {
        if (name_ == "RESTORE_INITIAL") return UI_ANIMATION_STOP_MODE::RESTORE_INITIAL;
        if (name_ == "MOVE_TO_END") return UI_ANIMATION_STOP_MODE::MOVE_TO_END;
        return UI_ANIMATION_STOP_MODE::KEEP_CURRENT;
    }

    UI_ANIMATION_VALUE_MODE ValueModeFromString(const std::string& name_)
    {
        if (name_ == "RELATIVE_TO_INITIAL") return UI_ANIMATION_VALUE_MODE::VALUE_RELATIVE_TO_INITIAL;
        if (name_ == "ADDITIVE") return UI_ANIMATION_VALUE_MODE::VALUE_ADDITIVE;
        return UI_ANIMATION_VALUE_MODE::VALUE_ABSOLUTE;
    }

    UI_ANIMATION_EASING EasingFromString(const std::string& name_)
    {
        if (name_ == "EASE_IN_CUBIC") return UI_ANIMATION_EASING::EASE_IN_CUBIC;
        if (name_ == "EASE_OUT_CUBIC") return UI_ANIMATION_EASING::EASE_OUT_CUBIC;
        if (name_ == "EASE_IN_OUT_CUBIC") return UI_ANIMATION_EASING::EASE_IN_OUT_CUBIC;
        if (name_ == "EASE_OUT_BACK") return UI_ANIMATION_EASING::EASE_OUT_BACK;
        return UI_ANIMATION_EASING::LINEAR;
    }

    float Ease(float ratio_, UI_ANIMATION_EASING easing_)
    {
        ratio_ = std::clamp(ratio_, 0.0f, 1.0f);
        switch (easing_)
        {
        case UI_ANIMATION_EASING::EASE_IN_CUBIC:
            return ratio_ * ratio_ * ratio_;
        case UI_ANIMATION_EASING::EASE_OUT_CUBIC:
            return 1.0f - std::pow(1.0f - ratio_, 3.0f);
        case UI_ANIMATION_EASING::EASE_IN_OUT_CUBIC:
            return ratio_ < 0.5f
                ? 4.0f * ratio_ * ratio_ * ratio_
                : 1.0f - std::pow(-2.0f * ratio_ + 2.0f, 3.0f) * 0.5f;
        case UI_ANIMATION_EASING::EASE_OUT_BACK:
        {
            constexpr float c1_ = 1.70158f;
            constexpr float c3_ = c1_ + 1.0f;
            return 1.0f + c3_ * std::pow(ratio_ - 1.0f, 3.0f) + c1_ * std::pow(ratio_ - 1.0f, 2.0f);
        }
        case UI_ANIMATION_EASING::LINEAR:
        default:
            return ratio_;
        }
    }

    UI_VERTEX_INDEX GetVertexIndex(UI_ANIMATION_PROPERTY property_)
    {
        switch (property_)
        {
        case UI_ANIMATION_PROPERTY::VERTEX_LEFT_TOP_POSITION_X:
        case UI_ANIMATION_PROPERTY::VERTEX_LEFT_TOP_POSITION_Y:
        case UI_ANIMATION_PROPERTY::VERTEX_LEFT_TOP_COLOR_R:
        case UI_ANIMATION_PROPERTY::VERTEX_LEFT_TOP_COLOR_G:
        case UI_ANIMATION_PROPERTY::VERTEX_LEFT_TOP_COLOR_B:
        case UI_ANIMATION_PROPERTY::VERTEX_LEFT_TOP_COLOR_A:
            return UI_VERTEX_INDEX::LEFT_TOP;
        case UI_ANIMATION_PROPERTY::VERTEX_RIGHT_TOP_POSITION_X:
        case UI_ANIMATION_PROPERTY::VERTEX_RIGHT_TOP_POSITION_Y:
        case UI_ANIMATION_PROPERTY::VERTEX_RIGHT_TOP_COLOR_R:
        case UI_ANIMATION_PROPERTY::VERTEX_RIGHT_TOP_COLOR_G:
        case UI_ANIMATION_PROPERTY::VERTEX_RIGHT_TOP_COLOR_B:
        case UI_ANIMATION_PROPERTY::VERTEX_RIGHT_TOP_COLOR_A:
            return UI_VERTEX_INDEX::RIGHT_TOP;
        case UI_ANIMATION_PROPERTY::VERTEX_LEFT_BOTTOM_POSITION_X:
        case UI_ANIMATION_PROPERTY::VERTEX_LEFT_BOTTOM_POSITION_Y:
        case UI_ANIMATION_PROPERTY::VERTEX_LEFT_BOTTOM_COLOR_R:
        case UI_ANIMATION_PROPERTY::VERTEX_LEFT_BOTTOM_COLOR_G:
        case UI_ANIMATION_PROPERTY::VERTEX_LEFT_BOTTOM_COLOR_B:
        case UI_ANIMATION_PROPERTY::VERTEX_LEFT_BOTTOM_COLOR_A:
            return UI_VERTEX_INDEX::LEFT_BOTTOM;
        default:
            return UI_VERTEX_INDEX::RIGHT_BOTTOM;
        }
    }
}

void UIAnimationPlayerComponent::Start()
{
    BS_PROFILE_SCOPE("UIAnimationPlayerComponent::Start");
    if (!animationAssetPath.empty() && animationAssetPath != "Data/UI/Animations/")
    {
        LoadAnimation(animationAssetPath);
    }
    RegisterActions();
}

void UIAnimationPlayerComponent::Update(float deltaTime_)
{
    BS_PROFILE_SCOPE("UIAnimationPlayerComponent::Update");

    if (!playing || paused) return;

    currentTime += deltaTime_ * playbackSpeed;
    if (duration > 0.0f && currentTime >= duration)
    {
        if (loop)
        {
            currentTime = std::fmod(currentTime, duration);
        }
        else
        {
            currentTime = duration;
            playing = false;
        }
    }

    for (const UIAnimationTrack& track_ : tracks)
    {
        if (!track_.enabled) continue;
        ApplyTrackValue(track_, ApplyValueMode(track_, EvaluateTrack(track_)));
    }
}

void UIAnimationPlayerComponent::Gui()
{
    BS_PROFILE_SCOPE("UIAnimationPlayerComponent::Gui");

    char pathBuffer_[260]{};
    strncpy_s(pathBuffer_, sizeof(pathBuffer_), animationAssetPath.c_str(), _TRUNCATE);
    if (ImGui::InputText("Animation Asset", pathBuffer_, sizeof(pathBuffer_)))
    {
        animationAssetPath = pathBuffer_;
    }

    ImGui::Checkbox("Loop", &loop);
    ImGui::DragFloat("Playback Speed", &playbackSpeed, 0.01f, 0.0f, 8.0f);
    ImGui::Checkbox("Register Actions", &registerGlobalActions);

    if (ImGui::Button("Load")) LoadAnimation(animationAssetPath);
    ImGui::SameLine();
    if (ImGui::Button("Play")) Play(true);
    ImGui::SameLine();
    if (ImGui::Button("Stop")) Stop(stopMode);

    ImGui::Text("Animation: %s", animationName.c_str());
    ImGui::Text("Time: %.3f / %.3f", currentTime, duration);
    ImGui::Text("Tracks: %zu", tracks.size());
}

void UIAnimationPlayerComponent::Serialize(nlohmann::json& json_) const
{
    BS_PROFILE_SCOPE("UIAnimationPlayerComponent::Serialize");

    json_["Version"] = 1;
    json_["AnimationAssetPath"] = animationAssetPath;
    json_["PlaybackSpeed"] = playbackSpeed;
    json_["Loop"] = loop;
    json_["StopMode"] = ToString(stopMode);
    json_["RegisterGlobalActions"] = registerGlobalActions;
    json_["ActionPrefix"] = actionPrefix;
}

void UIAnimationPlayerComponent::Deserialize(const nlohmann::json& json_)
{
    BS_PROFILE_SCOPE("UIAnimationPlayerComponent::Deserialize");

    animationAssetPath = json_.value("AnimationAssetPath", animationAssetPath);
    playbackSpeed = json_.value("PlaybackSpeed", playbackSpeed);
    loop = json_.value("Loop", loop);
    stopMode = StopModeFromString(json_.value("StopMode", std::string(ToString(stopMode))));
    registerGlobalActions = json_.value("RegisterGlobalActions", registerGlobalActions);
    actionPrefix = json_.value("ActionPrefix", actionPrefix);
}

bool UIAnimationPlayerComponent::LoadAnimation(const std::string& assetPath_)
{
    BS_PROFILE_SCOPE("UIAnimationPlayerComponent::LoadAnimation");

    std::ifstream file_(assetPath_);
    if (!file_.is_open()) return false;

    nlohmann::json json_;
    try
    {
        file_ >> json_;
    }
    catch (...)
    {
        return false;
    }

    animationAssetPath = assetPath_;
    animationName = json_.value("AnimationName", animationName);
    duration = json_.value("Duration", 0.0f);
    loop = json_.value("Loop", loop);
    playbackSpeed = json_.value("PlaybackSpeed", playbackSpeed);
    stopMode = StopModeFromString(json_.value("StopMode", std::string(ToString(stopMode))));
    tracks.clear();

    if (json_.contains("Tracks") && json_["Tracks"].is_array())
    {
        for (const nlohmann::json& trackJson_ : json_["Tracks"])
        {
            UIAnimationTrack track_;
            track_.trackName = trackJson_.value("TrackName", std::string{});
            track_.targetWidgetId = trackJson_.value("TargetWidgetId", std::string{});
            track_.property = PropertyFromString(trackJson_.value("Property", std::string{}));
            track_.valueMode = ValueModeFromString(trackJson_.value("ValueMode", std::string{}));
            track_.enabled = trackJson_.value("Enabled", true);

            if (trackJson_.contains("Keyframes") && trackJson_["Keyframes"].is_array())
            {
                for (const nlohmann::json& keyJson_ : trackJson_["Keyframes"])
                {
                    UIAnimationKeyframe keyframe_;
                    keyframe_.time = keyJson_.value("Time", 0.0f);
                    keyframe_.value = keyJson_.value("Value", 0.0f);
                    keyframe_.easingType = EasingFromString(keyJson_.value("EasingType", std::string{}));
                    track_.keyframes.push_back(keyframe_);
                }
            }

            std::sort(
                track_.keyframes.begin(),
                track_.keyframes.end(),
                [](const UIAnimationKeyframe& left_, const UIAnimationKeyframe& right_)
                {
                    return left_.time < right_.time;
                });
            if (!track_.keyframes.empty())
            {
                duration = (std::max)(duration, track_.keyframes.back().time);
            }
            tracks.push_back(track_);
        }
    }

    RegisterActions();
    ResolveTrackTargets();
    return true;
}

void UIAnimationPlayerComponent::Play(bool restart_, float playbackSpeed_)
{
    BS_PROFILE_SCOPE("UIAnimationPlayerComponent::Play");

    if (restart_) currentTime = 0.0f;
    if (playbackSpeed_ >= 0.0f) playbackSpeed = playbackSpeed_;
    paused = false;
    playing = true;
    ResolveTrackTargets();
    CaptureInitialValues();
}

void UIAnimationPlayerComponent::Stop(UI_ANIMATION_STOP_MODE stopMode_)
{
    BS_PROFILE_SCOPE("UIAnimationPlayerComponent::Stop");

    playing = false;
    paused = false;

    if (stopMode_ == UI_ANIMATION_STOP_MODE::RESTORE_INITIAL)
    {
        for (const UIAnimationTrack& track_ : tracks)
        {
            ApplyTrackValue(track_, track_.initialValue);
        }
    }
    else if (stopMode_ == UI_ANIMATION_STOP_MODE::MOVE_TO_END)
    {
        Seek(duration);
    }
}

void UIAnimationPlayerComponent::Pause()
{
    paused = true;
}

void UIAnimationPlayerComponent::Resume()
{
    paused = false;
}

void UIAnimationPlayerComponent::Seek(float time_)
{
    BS_PROFILE_SCOPE("UIAnimationPlayerComponent::Seek");

    currentTime = std::clamp(time_, 0.0f, (std::max)(0.0f, duration));
    for (const UIAnimationTrack& track_ : tracks)
    {
        if (!track_.enabled) continue;
        ApplyTrackValue(track_, ApplyValueMode(track_, EvaluateTrack(track_)));
    }
}

void UIAnimationPlayerComponent::RegisterActions()
{
    BS_PROFILE_SCOPE("UIAnimationPlayerComponent::RegisterActions");

    if (!registerGlobalActions) return;

    auto owner_ = GetGameObject();
    if (!owner_) return;

    const std::string prefix_ = actionPrefix.empty() ? animationName : actionPrefix;
    owner_->RegisterGlobalAction("UIAnimation", prefix_ + "_Play", [this]() { Play(true); });
    owner_->RegisterGlobalAction("UIAnimation", prefix_ + "_Stop", [this]() { Stop(stopMode); });
    owner_->RegisterGlobalAction("UIAnimation", prefix_ + "_Pause", [this]() { Pause(); });
    owner_->RegisterGlobalAction("UIAnimation", prefix_ + "_Resume", [this]() { Resume(); });
}

void UIAnimationPlayerComponent::ResolveTrackTargets()
{
    BS_PROFILE_SCOPE("UIAnimationPlayerComponent::ResolveTrackTargets");

    auto owner_ = GetGameObject();
    if (!owner_) return;

    for (UIAnimationTrack& track_ : tracks)
    {
        track_.targetWidget.reset();
        track_.targetSprite.reset();
        track_.targetVertexModifier.reset();

        for (const std::shared_ptr<GameObject>& object_ : owner_->GetManager().GetObjects())
        {
            if (!object_) continue;

            auto widget_ = object_->GetComponent<UIWidgetComponent>();
            if (!widget_ || widget_->GetWidgetId() != track_.targetWidgetId) continue;

            track_.targetWidget = widget_;
            track_.targetSprite = object_->GetComponent<SpriteRenderer>();
            track_.targetVertexModifier = object_->GetComponent<UIVertexModifierComponent>();
            break;
        }
    }
}

void UIAnimationPlayerComponent::CaptureInitialValues()
{
    BS_PROFILE_SCOPE("UIAnimationPlayerComponent::CaptureInitialValues");

    for (UIAnimationTrack& track_ : tracks)
    {
        float value_ = 0.0f;
        if (TryGetCurrentValue(track_, value_))
        {
            track_.initialValue = value_;
        }
    }
}

float UIAnimationPlayerComponent::EvaluateTrack(const UIAnimationTrack& track_) const
{
    if (track_.keyframes.empty()) return 0.0f;
    if (currentTime <= track_.keyframes.front().time) return track_.keyframes.front().value;
    if (currentTime >= track_.keyframes.back().time) return track_.keyframes.back().value;

    for (size_t index_ = 0; index_ + 1 < track_.keyframes.size(); ++index_)
    {
        const UIAnimationKeyframe& left_ = track_.keyframes[index_];
        const UIAnimationKeyframe& right_ = track_.keyframes[index_ + 1];
        if (currentTime < left_.time || currentTime > right_.time) continue;

        const float span_ = (std::max)(0.0001f, right_.time - left_.time);
        const float ratio_ = Ease((currentTime - left_.time) / span_, right_.easingType);
        return std::lerp(left_.value, right_.value, ratio_);
    }

    return track_.keyframes.back().value;
}

float UIAnimationPlayerComponent::ApplyValueMode(const UIAnimationTrack& track_, float sampledValue_) const
{
    switch (track_.valueMode)
    {
    case UI_ANIMATION_VALUE_MODE::VALUE_RELATIVE_TO_INITIAL:
        return track_.initialValue + sampledValue_;
    case UI_ANIMATION_VALUE_MODE::VALUE_ADDITIVE:
    {
        float currentValue_ = 0.0f;
        if (TryGetCurrentValue(track_, currentValue_)) return currentValue_ + sampledValue_;
        return sampledValue_;
    }
    case UI_ANIMATION_VALUE_MODE::VALUE_ABSOLUTE:
    default:
        return sampledValue_;
    }
}

void UIAnimationPlayerComponent::ApplyTrackValue(const UIAnimationTrack& track_, float value_)
{
    std::shared_ptr<UIWidgetComponent> widgetShared_ = track_.targetWidget.lock();
    UIWidgetComponent* widget_ = widgetShared_.get();
    if (!widget_) return;
    SpriteRenderer* sprite_ = track_.targetSprite.lock().get();
    UIVertexModifierComponent* vertexModifier_ = track_.targetVertexModifier.lock().get();

    UIRectTransform& rectTransform_ = widget_->GetRectTransform();
    switch (track_.property)
    {
    case UI_ANIMATION_PROPERTY::ANCHORED_POSITION_X:
        rectTransform_.anchoredPosition.x = value_;
        widget_->MarkLayoutDirty();
        break;
    case UI_ANIMATION_PROPERTY::ANCHORED_POSITION_Y:
        rectTransform_.anchoredPosition.y = value_;
        widget_->MarkLayoutDirty();
        break;
    case UI_ANIMATION_PROPERTY::SIZE_X:
        rectTransform_.sizeDelta.x = value_;
        widget_->MarkLayoutDirty();
        break;
    case UI_ANIMATION_PROPERTY::SIZE_Y:
        rectTransform_.sizeDelta.y = value_;
        widget_->MarkLayoutDirty();
        break;
    case UI_ANIMATION_PROPERTY::SCALE_X:
        rectTransform_.localScale.x = value_;
        widget_->MarkLayoutDirty();
        break;
    case UI_ANIMATION_PROPERTY::SCALE_Y:
        rectTransform_.localScale.y = value_;
        widget_->MarkLayoutDirty();
        break;
    case UI_ANIMATION_PROPERTY::ROTATION:
        rectTransform_.localRotation = value_;
        widget_->MarkLayoutDirty();
        break;
    case UI_ANIMATION_PROPERTY::COLOR_R:
    case UI_ANIMATION_PROPERTY::COLOR_G:
    case UI_ANIMATION_PROPERTY::COLOR_B:
    case UI_ANIMATION_PROPERTY::COLOR_A:
        if (sprite_)
        {
            DirectX::XMFLOAT4 color_ = sprite_->GetColor();
            if (track_.property == UI_ANIMATION_PROPERTY::COLOR_R) color_.x = value_;
            if (track_.property == UI_ANIMATION_PROPERTY::COLOR_G) color_.y = value_;
            if (track_.property == UI_ANIMATION_PROPERTY::COLOR_B) color_.z = value_;
            if (track_.property == UI_ANIMATION_PROPERTY::COLOR_A) color_.w = value_;
            sprite_->SetColor(color_);
        }
        break;
    case UI_ANIMATION_PROPERTY::VISIBLE:
        widget_->SetEnabled(value_ > 0.5f);
        break;
    default:
        if (vertexModifier_)
        {
            UIVertexModification& modification_ =
                vertexModifier_->GetMutableVertexModification(GetVertexIndex(track_.property));
            if (track_.property == UI_ANIMATION_PROPERTY::VERTEX_LEFT_TOP_POSITION_X ||
                track_.property == UI_ANIMATION_PROPERTY::VERTEX_RIGHT_TOP_POSITION_X ||
                track_.property == UI_ANIMATION_PROPERTY::VERTEX_LEFT_BOTTOM_POSITION_X ||
                track_.property == UI_ANIMATION_PROPERTY::VERTEX_RIGHT_BOTTOM_POSITION_X)
            {
                modification_.positionOffset.x = value_;
            }
            else if (track_.property == UI_ANIMATION_PROPERTY::VERTEX_LEFT_TOP_POSITION_Y ||
                track_.property == UI_ANIMATION_PROPERTY::VERTEX_RIGHT_TOP_POSITION_Y ||
                track_.property == UI_ANIMATION_PROPERTY::VERTEX_LEFT_BOTTOM_POSITION_Y ||
                track_.property == UI_ANIMATION_PROPERTY::VERTEX_RIGHT_BOTTOM_POSITION_Y)
            {
                modification_.positionOffset.y = value_;
            }
            else if (track_.property == UI_ANIMATION_PROPERTY::VERTEX_LEFT_TOP_COLOR_R ||
                track_.property == UI_ANIMATION_PROPERTY::VERTEX_RIGHT_TOP_COLOR_R ||
                track_.property == UI_ANIMATION_PROPERTY::VERTEX_LEFT_BOTTOM_COLOR_R ||
                track_.property == UI_ANIMATION_PROPERTY::VERTEX_RIGHT_BOTTOM_COLOR_R)
            {
                modification_.colorMultiplier.x = value_;
            }
            else if (track_.property == UI_ANIMATION_PROPERTY::VERTEX_LEFT_TOP_COLOR_G ||
                track_.property == UI_ANIMATION_PROPERTY::VERTEX_RIGHT_TOP_COLOR_G ||
                track_.property == UI_ANIMATION_PROPERTY::VERTEX_LEFT_BOTTOM_COLOR_G ||
                track_.property == UI_ANIMATION_PROPERTY::VERTEX_RIGHT_BOTTOM_COLOR_G)
            {
                modification_.colorMultiplier.y = value_;
            }
            else if (track_.property == UI_ANIMATION_PROPERTY::VERTEX_LEFT_TOP_COLOR_B ||
                track_.property == UI_ANIMATION_PROPERTY::VERTEX_RIGHT_TOP_COLOR_B ||
                track_.property == UI_ANIMATION_PROPERTY::VERTEX_LEFT_BOTTOM_COLOR_B ||
                track_.property == UI_ANIMATION_PROPERTY::VERTEX_RIGHT_BOTTOM_COLOR_B)
            {
                modification_.colorMultiplier.z = value_;
            }
            else
            {
                modification_.colorMultiplier.w = value_;
            }
        }
        break;
    }
}

bool UIAnimationPlayerComponent::TryGetCurrentValue(const UIAnimationTrack& track_, float& value_) const
{
    std::shared_ptr<UIWidgetComponent> widget_ = track_.targetWidget.lock();
    if (!widget_) return false;

    const UIRectTransform& rectTransform_ = widget_->GetRectTransform();
    switch (track_.property)
    {
        case UI_ANIMATION_PROPERTY::ANCHORED_POSITION_X:
            value_ = rectTransform_.anchoredPosition.x;
            return true;
        case UI_ANIMATION_PROPERTY::ANCHORED_POSITION_Y:
            value_ = rectTransform_.anchoredPosition.y;
            return true;
        case UI_ANIMATION_PROPERTY::SIZE_X:
            value_ = rectTransform_.sizeDelta.x;
            return true;
        case UI_ANIMATION_PROPERTY::SIZE_Y:
            value_ = rectTransform_.sizeDelta.y;
            return true;
        case UI_ANIMATION_PROPERTY::SCALE_X:
            value_ = rectTransform_.localScale.x;
            return true;
        case UI_ANIMATION_PROPERTY::SCALE_Y:
            value_ = rectTransform_.localScale.y;
            return true;
        case UI_ANIMATION_PROPERTY::ROTATION:
            value_ = rectTransform_.localRotation;
            return true;
        case UI_ANIMATION_PROPERTY::COLOR_R:
        case UI_ANIMATION_PROPERTY::COLOR_G:
        case UI_ANIMATION_PROPERTY::COLOR_B:
        case UI_ANIMATION_PROPERTY::COLOR_A:
        {
            auto sprite_ = track_.targetSprite.lock();
            if (!sprite_) return false;
            const DirectX::XMFLOAT4 color_ = sprite_->GetColor();
            if (track_.property == UI_ANIMATION_PROPERTY::COLOR_R) value_ = color_.x;
            if (track_.property == UI_ANIMATION_PROPERTY::COLOR_G) value_ = color_.y;
            if (track_.property == UI_ANIMATION_PROPERTY::COLOR_B) value_ = color_.z;
            if (track_.property == UI_ANIMATION_PROPERTY::COLOR_A) value_ = color_.w;
            return true;
        }
        case UI_ANIMATION_PROPERTY::VISIBLE:
            value_ = widget_->IsEnabled() ? 1.0f : 0.0f;
            return true;
        default:
        {
            auto vertexModifier_ = track_.targetVertexModifier.lock();
            if (!vertexModifier_) return false;
            const UIVertexModification& modification_ =
                vertexModifier_->GetVertexModification(GetVertexIndex(track_.property));
            if (track_.property == UI_ANIMATION_PROPERTY::VERTEX_LEFT_TOP_POSITION_X ||
                track_.property == UI_ANIMATION_PROPERTY::VERTEX_RIGHT_TOP_POSITION_X ||
                track_.property == UI_ANIMATION_PROPERTY::VERTEX_LEFT_BOTTOM_POSITION_X ||
                track_.property == UI_ANIMATION_PROPERTY::VERTEX_RIGHT_BOTTOM_POSITION_X)
            {
                value_ = modification_.positionOffset.x;
            }
            else if (track_.property == UI_ANIMATION_PROPERTY::VERTEX_LEFT_TOP_POSITION_Y ||
                track_.property == UI_ANIMATION_PROPERTY::VERTEX_RIGHT_TOP_POSITION_Y ||
                track_.property == UI_ANIMATION_PROPERTY::VERTEX_LEFT_BOTTOM_POSITION_Y ||
                track_.property == UI_ANIMATION_PROPERTY::VERTEX_RIGHT_BOTTOM_POSITION_Y)
            {
                value_ = modification_.positionOffset.y;
            }
            else if (track_.property == UI_ANIMATION_PROPERTY::VERTEX_LEFT_TOP_COLOR_R ||
                track_.property == UI_ANIMATION_PROPERTY::VERTEX_RIGHT_TOP_COLOR_R ||
                track_.property == UI_ANIMATION_PROPERTY::VERTEX_LEFT_BOTTOM_COLOR_R ||
                track_.property == UI_ANIMATION_PROPERTY::VERTEX_RIGHT_BOTTOM_COLOR_R)
            {
                value_ = modification_.colorMultiplier.x;
            }
            else if (track_.property == UI_ANIMATION_PROPERTY::VERTEX_LEFT_TOP_COLOR_G ||
                track_.property == UI_ANIMATION_PROPERTY::VERTEX_RIGHT_TOP_COLOR_G ||
                track_.property == UI_ANIMATION_PROPERTY::VERTEX_LEFT_BOTTOM_COLOR_G ||
                track_.property == UI_ANIMATION_PROPERTY::VERTEX_RIGHT_BOTTOM_COLOR_G)
            {
                value_ = modification_.colorMultiplier.y;
            }
            else if (track_.property == UI_ANIMATION_PROPERTY::VERTEX_LEFT_TOP_COLOR_B ||
                track_.property == UI_ANIMATION_PROPERTY::VERTEX_RIGHT_TOP_COLOR_B ||
                track_.property == UI_ANIMATION_PROPERTY::VERTEX_LEFT_BOTTOM_COLOR_B ||
                track_.property == UI_ANIMATION_PROPERTY::VERTEX_RIGHT_BOTTOM_COLOR_B)
            {
                value_ = modification_.colorMultiplier.z;
            }
            else
            {
                value_ = modification_.colorMultiplier.w;
            }
            return true;
        }
    }

    return false;
}

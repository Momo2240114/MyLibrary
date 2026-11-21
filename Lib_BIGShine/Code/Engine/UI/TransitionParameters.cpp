#include "PrecompiledHeader.h"
#include "TransitionParameters.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr const char* TYPE_NAMES[] = {"FADE","RADIAL","SLASH","DISSOLVE","SHUTTER","WIPE","TEXTURE_MASK","DIAMOND","CURTAIN","CHECKER","SPIRAL","GLASS"};
constexpr const char* EASING_NAMES[] = {"LINEAR","EASE_IN_CUBIC","EASE_OUT_CUBIC","EASE_IN_OUT_CUBIC","EASE_OUT_EXPO","EASE_OUT_BACK"};
}

TransitionParameters TransitionParameters::Deserialize(const nlohmann::json& json_)
{
    TransitionParameters result_;
    if (!json_.is_object())
    {
        return result_;
    }
    const auto number_ = [&](const char* key_, float fallback_, float minimum_, float maximum_)
    {
        if (!json_.contains(key_) || !json_[key_].is_number())
        {
            return fallback_;
        }
        const float value_ = json_[key_].get<float>();
        return std::isfinite(value_) ? std::clamp(value_, minimum_, maximum_) : fallback_;
    };
    const auto vector_ = [&](const char* key_, float* target_, int count_, float minimum_, float maximum_)
    {
        if (!json_.contains(key_) || !json_[key_].is_array() || json_[key_].size() != count_)
        {
            return;
        }
        for (int index_ = 0; index_ < count_; ++index_)
        {
            if (json_[key_][index_].is_number())
            {
                const float value_ = json_[key_][index_].get<float>();
                if (std::isfinite(value_))
                {
                    target_[index_] = std::clamp(value_, minimum_, maximum_);
                }
            }
        }
    };
    if (json_.contains("TransitionType") && json_["TransitionType"].is_string())
    {
        for (int index_ = 0; index_ < 12; ++index_)
        {
            if (json_["TransitionType"] == TYPE_NAMES[index_])
            {
                result_.type = static_cast<TransitionType>(index_);
            }
        }
    }
    if (json_.contains("Easing") && json_["Easing"].is_string())
    {
        for (int index_ = 0; index_ < 6; ++index_)
        {
            if (json_["Easing"] == EASING_NAMES[index_])
            {
                result_.easing = static_cast<TransitionEasing>(index_);
            }
        }
    }
    result_.durationOut = number_("DurationOut", result_.durationOut, 0, 10);
    result_.durationIn = number_("DurationIn", result_.durationIn, 0, 10);
    result_.delay = number_("Delay", 0, 0, 10);
    result_.edgeWidth = number_("EdgeWidth", result_.edgeWidth, 0.0001f, 0.2f);
    result_.noiseScale = number_("NoiseScale", result_.noiseScale, 0.1f, 50);
    result_.noiseSpeed = number_("NoiseSpeed", result_.noiseSpeed, -5, 5);
    result_.glowIntensity = number_("GlowIntensity", result_.glowIntensity, 0, 8);
    result_.panelCount = static_cast<int>(number_("PanelCount", 8, 2, 32));
    result_.panelDelay = number_("PanelDelay", result_.panelDelay, 0, 0.5f);
    result_.boundaryCurve = number_("BoundaryCurve", 0, -0.3f, 0.3f);
    result_.softness = number_("Softness", result_.softness, 0.0001f, 0.1f);
    vector_("Color", &result_.color.x, 4, 0, 1);
    vector_("EdgeColor", &result_.edgeColor.x, 4, 0, 1);
    vector_("Direction", &result_.direction.x, 2, -1, 1);
    vector_("CenterPosition", &result_.centerPosition.x, 2, 0, 1);
    const float length_ = std::hypot(result_.direction.x, result_.direction.y);
    if (length_ < 0.001f)
    {
        result_.direction = {1, 0};
    }
    else
    {
        result_.direction.x /= length_;
        result_.direction.y /= length_;
    }
    result_.holdDuration = number_("HoldDuration", 0.15f, 0, 60);
    result_.playbackSpeed = number_("PlaybackSpeed", 1.0f, 0.05f, 10);
    if (json_.contains("BlockInput") && json_["BlockInput"].is_boolean())
    {
        result_.blockInput = json_["BlockInput"].get<bool>();
    }
    if (json_.contains("Effect") && json_["Effect"].is_object())
    {
        result_.effectSettings = json_["Effect"];
    }
    // 半透明の指定でも、終端は必ず不透明としてSceneの切替を隠す。
    result_.color.w = 1.0f;
    return result_;
}

nlohmann::json TransitionParameters::Serialize(const TransitionParameters& parameters_)
{
    return {{"TransitionType", TYPE_NAMES[static_cast<int>(parameters_.type)]},
        {"Easing", EASING_NAMES[static_cast<int>(parameters_.easing)]},
        {"DurationOut", parameters_.durationOut}, {"DurationIn", parameters_.durationIn}, {"Delay", parameters_.delay},
        {"Color", {parameters_.color.x, parameters_.color.y, parameters_.color.z, 1.0f}},
        {"EdgeColor", {parameters_.edgeColor.x, parameters_.edgeColor.y, parameters_.edgeColor.z, parameters_.edgeColor.w}},
        {"EdgeWidth", parameters_.edgeWidth}, {"Direction", {parameters_.direction.x, parameters_.direction.y}},
        {"NoiseScale", parameters_.noiseScale}, {"NoiseSpeed", parameters_.noiseSpeed}, {"GlowIntensity", parameters_.glowIntensity},
        {"CenterPosition", {parameters_.centerPosition.x, parameters_.centerPosition.y}}, {"PanelCount", parameters_.panelCount},
        {"PanelDelay", parameters_.panelDelay}, {"BoundaryCurve", parameters_.boundaryCurve}, {"Softness", parameters_.softness}, {"HoldDuration",parameters_.holdDuration}, {"PlaybackSpeed",parameters_.playbackSpeed}, {"BlockInput",parameters_.blockInput}, {"Effect",parameters_.effectSettings}};
}


const char* TransitionParameters::GetTypeName(TransitionType type_)
{
    return TYPE_NAMES[std::clamp(static_cast<int>(type_),0,11)];
}

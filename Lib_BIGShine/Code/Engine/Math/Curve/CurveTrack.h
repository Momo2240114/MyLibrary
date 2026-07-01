#pragma once

#include "ImCurveEdit.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

enum class CURVE_INTERPOLATION_TYPE
{
    CONSTANT,
    LINEAR,
    SMOOTH,
    EASE_IN,
    EASE_OUT,
    EASE_IN_OUT,
    BEZIER
};

struct CurvePointOption
{
    CURVE_INTERPOLATION_TYPE interpolationType = CURVE_INTERPOLATION_TYPE::LINEAR;
    float leftTangent = 0.0f;
    float rightTangent = 0.0f;
};

struct CurveTrack
{
    std::string name;
    std::string assetPath;
    std::vector<ImVec2> points;
    std::vector<CurvePointOption> pointOptions;
    ImCurveEdit::CurveType type;
    uint32_t color;
    bool visible;

    explicit CurveTrack(const std::string& name_ = "New Curve", uint32_t color_ = 0xFF00FFFF)
        : name(name_), type(ImCurveEdit::CurveSmooth), color(color_), visible(true)
    {
        points.push_back({ 0.0f, 0.0f });
        points.push_back({ 1.0f, 1.0f });
        EnsurePointOptions();
    }

    void EnsurePointOptions()
    {
        pointOptions.resize(points.size());
    }

    void Sort()
    {
        EnsurePointOptions();
        std::vector<std::pair<ImVec2, CurvePointOption>> entries_;
        entries_.reserve(points.size());
        for (size_t index_ = 0; index_ < points.size(); ++index_)
        {
            entries_.emplace_back(points[index_], pointOptions[index_]);
        }
        std::stable_sort(entries_.begin(), entries_.end(),
            [](const auto& left_, const auto& right_) { return left_.first.x < right_.first.x; });
        for (size_t index_ = 0; index_ < entries_.size(); ++index_)
        {
            points[index_] = entries_[index_].first;
            pointOptions[index_] = entries_[index_].second;
        }
    }

    float ApplyInterpolation(float rate_, CURVE_INTERPOLATION_TYPE interpolationType_) const
    {
        rate_ = (std::clamp)(rate_, 0.0f, 1.0f);
        switch (interpolationType_)
        {
        case CURVE_INTERPOLATION_TYPE::CONSTANT:
            return 0.0f;
        case CURVE_INTERPOLATION_TYPE::SMOOTH:
            return rate_ * rate_ * (3.0f - 2.0f * rate_);
        case CURVE_INTERPOLATION_TYPE::EASE_IN:
            return rate_ * rate_;
        case CURVE_INTERPOLATION_TYPE::EASE_OUT:
            return 1.0f - (1.0f - rate_) * (1.0f - rate_);
        case CURVE_INTERPOLATION_TYPE::EASE_IN_OUT:
            return rate_ < 0.5f
                ? 2.0f * rate_ * rate_
                : 1.0f - std::pow(-2.0f * rate_ + 2.0f, 2.0f) * 0.5f;
        case CURVE_INTERPOLATION_TYPE::LINEAR:
        default:
            return rate_;
        }
    }

    float Evaluate(float time_) const
    {
        if (points.empty()) return 0.0f;
        if (time_ <= points.front().x) return points.front().y;
        if (time_ >= points.back().x) return points.back().y;

        for (size_t index_ = 0; index_ + 1 < points.size(); ++index_)
        {
            const ImVec2& startPoint_ = points[index_];
            const ImVec2& endPoint_ = points[index_ + 1];
            if (time_ < startPoint_.x || time_ > endPoint_.x) continue;

            const float timeRange_ = endPoint_.x - startPoint_.x;
            float rate_ = timeRange_ > 0.0f ? (time_ - startPoint_.x) / timeRange_ : 0.0f;
            const CURVE_INTERPOLATION_TYPE interpolationType_ = index_ < pointOptions.size()
                ? pointOptions[index_].interpolationType
                : CURVE_INTERPOLATION_TYPE::LINEAR;
            if (interpolationType_ == CURVE_INTERPOLATION_TYPE::BEZIER)
            {
                const float startTangent_ = index_ < pointOptions.size()
                    ? pointOptions[index_].rightTangent : 0.0f;
                const float endTangent_ = index_ + 1 < pointOptions.size()
                    ? pointOptions[index_ + 1].leftTangent : 0.0f;
                const float rateSquared_ = rate_ * rate_;
                const float rateCubed_ = rateSquared_ * rate_;
                const float startBasis_ = 2.0f * rateCubed_ - 3.0f * rateSquared_ + 1.0f;
                const float startTangentBasis_ = rateCubed_ - 2.0f * rateSquared_ + rate_;
                const float endBasis_ = -2.0f * rateCubed_ + 3.0f * rateSquared_;
                const float endTangentBasis_ = rateCubed_ - rateSquared_;
                return startBasis_ * startPoint_.y +
                    startTangentBasis_ * timeRange_ * startTangent_ +
                    endBasis_ * endPoint_.y +
                    endTangentBasis_ * timeRange_ * endTangent_;
            }
            rate_ = ApplyInterpolation(rate_, interpolationType_);
            return startPoint_.y + (endPoint_.y - startPoint_.y) * rate_;
        }
        return points.back().y;
    }
};

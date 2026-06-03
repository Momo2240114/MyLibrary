#pragma once
#include "ImCurveEdit.h"
#include <vector>
#include <algorithm>
#include <nlohmann/json.hpp>
#include <string>


struct CurveTrack 
{
    std::string name;
    std::vector<ImVec2> points;
    ImCurveEdit::CurveType type;
    uint32_t color;
    bool visible;

    CurveTrack(const std::string& n = "New Curve", uint32_t c = 0xFF00FFFF)
        : name(n), type(ImCurveEdit::CurveSmooth), color(c), visible(true) {
        // 初期値として 0.0~1.0 の直線を持たせる
        points.push_back({ 0.0f, 0.0f });
        points.push_back({ 1.0f, 1.0f });
    }

    // X軸（時間軸）でソート
    void Sort() {
        std::sort(points.begin(), points.end(), [](const ImVec2& a, const ImVec2& b) {
            return a.x < b.x;
            });
    }

    // 線形補間による値の取得
    float Evaluate(float t) const {
        if (points.empty()) return 0.0f;
        if (t <= points.front().x) return points.front().y;
        if (t >= points.back().x) return points.back().y;

        for (size_t i = 0; i < points.size() - 1; ++i) {
            if (t >= points[i].x && t <= points[i + 1].x) {
                float t_diff = points[i + 1].x - points[i].x;
                float ratio = (t_diff > 0.0f) ? (t - points[i].x) / t_diff : 0.0f;
                return points[i].y + (points[i + 1].y - points[i].y) * ratio;
            }
        }
        return points.back().y;
    }
};

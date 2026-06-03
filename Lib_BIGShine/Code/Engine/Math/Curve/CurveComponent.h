#pragma once
#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "CurveTrack.h"
#include "Engine/GamePlay/Serialize/ActionRegistry.h"
#include <unordered_map>
#include <memory>

class CurveComponent : public Component, public ImCurveEdit::Delegate 
{
public:
    const std::string& GetName() const override { static std::string name = "CurveComponent"; return name; }

    // --- ImCurveEdit::Delegate の定義にあるもののみを実装 ---
    size_t GetCurveCount() override { return tracks.size(); }
    bool IsVisible(size_t curveIndex) override { return tracks[curveIndex].visible; }
    ImCurveEdit::CurveType GetCurveType(size_t curveIndex) const override { return tracks[curveIndex].type; }
    ImVec2& GetMin() override { return minBounds; }
    ImVec2& GetMax() override { return maxBounds; }
    size_t GetPointCount(size_t curveIndex) override { return tracks[curveIndex].points.size(); }
    uint32_t GetCurveColor(size_t curveIndex) override { return tracks[curveIndex].color; }
    ImVec2* GetPoints(size_t curveIndex) override { return tracks[curveIndex].points.data(); }

    int EditPoint(size_t curveIndex, int pointIndex, ImVec2 value) override {
        tracks[curveIndex].points[pointIndex] = value;
        tracks[curveIndex].Sort();
        for (int i = 0; i < (int)tracks[curveIndex].points.size(); ++i) {
            if (tracks[curveIndex].points[i].x == value.x && tracks[curveIndex].points[i].y == value.y) return i;
        }
        return pointIndex;
    }

    void AddPoint(size_t curveIndex, ImVec2 value) override {
        tracks[curveIndex].points.push_back(value);
        tracks[curveIndex].Sort();
    }

    std::vector<std::string> GetTrackNames() const;

    unsigned int GetBackgroundColor() override { return 0xFF202020; }
    void BeginEdit(int index) override {}
    void EndEdit() override {}

    // --- コンポーネント用 ---
    void Gui() override; 
    // セーブ・ロードの実装
    void Serialize(nlohmann::json& j) const override;
    void Deserialize(const nlohmann::json& j) override;

    float GetValue(const std::string& name, float t) const;

private:
    bool isWindowOpen = false; // エディタウィンドウの表示フラグ
    std::vector<CurveTrack> tracks;
    ImVec2 minBounds = { 0.0f, 0.0f };
    ImVec2 maxBounds = { 1.0f, 1.0f };
};
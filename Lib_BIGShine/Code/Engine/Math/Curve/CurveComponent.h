#pragma once
#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "CurveTrack.h"
#include "Engine/GamePlay/Serialize/ActionRegistry.h"
#include <unordered_map>
#include <memory>

class CurveComponent : public Component, public ImCurveEdit::Delegate
{
public:
    const std::string& GetName() const override { static std::string name_ = "CurveComponent"; return name_; }

    size_t GetCurveCount() override { return tracks.size(); }
    bool IsVisible(size_t curveIndex_) override;
    bool IsEditable(size_t curveIndex_) const override { return IsEditableTrack(curveIndex_); }
    ImCurveEdit::CurveType GetCurveType(size_t curveIndex_) const override { return tracks[curveIndex_].type; }
    float Interpolate(size_t curveIndex_, size_t pointIndex_, float rate_) const override;
    float EvaluateSegment(size_t curveIndex_, size_t pointIndex_, float rate_) override;
    ImVec2& GetMin() override { return minBounds; }
    ImVec2& GetMax() override { return maxBounds; }
    size_t GetPointCount(size_t curveIndex_) override { return tracks[curveIndex_].points.size(); }
    uint32_t GetCurveColor(size_t curveIndex_) override { return tracks[curveIndex_].color; }
    ImVec2* GetPoints(size_t curveIndex_) override { return tracks[curveIndex_].points.data(); }

    int EditPoint(size_t curveIndex_, int pointIndex_, ImVec2 value_) override;
    void AddPoint(size_t curveIndex_, ImVec2 value_) override;

    std::vector<std::string> GetTrackNames() const;
    bool HasTrack(const std::string& name_) const;
    std::string CreateTrack(const std::string& preferredName_, float defaultValue_ = 1.0f);
    std::string CreateLinearTrack(const std::string& preferredName_, float startValue_, float endValue_);
    bool SaveTrackAsset(size_t trackIndex_, const std::string& assetPath_ = "");
    bool LoadTrackAsset(const std::string& assetPath_);
    void DrawTrackManager();
    void DrawEditorContent(const ImVec2& size_ = ImVec2(0.0f, 0.0f));

    unsigned int GetBackgroundColor() override { return 0xFF202020; }
    void BeginEdit(int index_) override {}
    void EndEdit() override {}

    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    float GetValue(const std::string& name_, float time_) const;

private:
    bool IsEditableTrack(size_t curveIndex_) const;
    bool IsValidTrackIndex(int trackIndex_) const;
    void ValidateEditingTrack();
    std::string MakeUniqueTrackName(const std::string& preferredName_, int ignoredTrackIndex_ = -1) const;
    void DrawSelectedPointEditor();
    bool DeleteSelectedPoint();
    bool DuplicateSelectedPoint();

    bool isWindowOpen = false;
    std::string curveAssetStatus;
    ImVector<ImCurveEdit::EditPoint> selectedPoints;
    std::vector<CurveTrack> tracks;
    ImVec2 minBounds = { 0.0f, 0.0f };
    ImVec2 maxBounds = { 1.0f, 1.0f };
    int editingTrackIndex = -1;
    bool showOnlyEditingTrack = false;
    bool lockNonEditingTracks = true;
    float trackListWidth = 260.0f;
};

#include "CurveComponent.h"

#include "Engine/Platform/WindowsDialogHelper.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace
{
    std::string MakeCurveAssetFileName(const std::string& name_)
    {
        std::string fileName_ = name_.empty() ? "Curve" : name_;
        constexpr const char* INVALID_CHARACTERS = "<>:\"/\\|?*";
        for (char& character_ : fileName_)
        {
            if (std::strchr(INVALID_CHARACTERS, character_))
            {
                character_ = '_';
            }
        }
        return fileName_;
    }

    std::string GetDefaultCurveAssetPath(const CurveTrack& track_)
    {
        return "Data/Curve/" + MakeCurveAssetFileName(track_.name) + ".json";
    }
}

std::vector<std::string> CurveComponent::GetTrackNames() const
{
    std::vector<std::string> names_;
    for (const auto& track_ : tracks)
    {
        names_.push_back(track_.name);
    }
    return names_;
}

bool CurveComponent::HasTrack(const std::string& name_) const
{
    return std::any_of(
        tracks.begin(),
        tracks.end(),
        [&name_](const CurveTrack& track_)
        {
            return track_.name == name_;
        });
}

std::string CurveComponent::CreateTrack(const std::string& preferredName_, float defaultValue_)
{
    const std::string baseName_ = preferredName_.empty() ? "Curve" : preferredName_;
    std::string trackName_ = baseName_;
    int suffix_ = 1;
    auto nameExists_ = [this](const std::string& name_)
        {
            return std::any_of(
                tracks.begin(),
                tracks.end(),
                [&name_](const CurveTrack& track_) { return track_.name == name_; });
        };

    while (nameExists_(trackName_))
    {
        trackName_ = baseName_ + "_" + std::to_string(suffix_++);
    }

    tracks.emplace_back(trackName_);
    tracks.back().points =
    {
        { 0.0f, defaultValue_ },
        { 1.0f, defaultValue_ }
    };
    return trackName_;
}

bool CurveComponent::SaveTrackAsset(size_t trackIndex_, const std::string& assetPath_)
{
    if (trackIndex_ >= tracks.size()) return false;

    CurveTrack& track_ = tracks[trackIndex_];
    std::string pathString_ = assetPath_.empty()
        ? (track_.assetPath.empty() ? GetDefaultCurveAssetPath(track_) : track_.assetPath)
        : assetPath_;
    std::filesystem::path path_ = WindowsDialogHelper::Utf8ToPath(pathString_);
    if (path_.extension() != ".json")
    {
        path_.replace_extension(".json");
        pathString_ = WindowsDialogHelper::PathToUtf8(path_);
    }

    const std::filesystem::path parentPath_ = path_.parent_path();
    if (!parentPath_.empty())
    {
        std::filesystem::create_directories(parentPath_);
    }

    nlohmann::json curveJson_;
    curveJson_["Name"] = track_.name;
    curveJson_["Type"] = static_cast<int>(track_.type);
    curveJson_["Color"] = track_.color;
    curveJson_["Visible"] = track_.visible;
    curveJson_["Points"] = nlohmann::json::array();
    for (const ImVec2& point_ : track_.points)
    {
        curveJson_["Points"].push_back({ point_.x, point_.y });
    }

    nlohmann::json assetJson_;
    assetJson_["AssetType"] = "CurveTrack";
    assetJson_["AssetVersion"] = 1;
    assetJson_["MinBounds"] = { minBounds.x, minBounds.y };
    assetJson_["MaxBounds"] = { maxBounds.x, maxBounds.y };
    assetJson_["Curve"] = std::move(curveJson_);

    std::ofstream file_(path_, std::ios::out | std::ios::trunc);
    if (!file_.is_open()) return false;
    file_ << assetJson_.dump(4);
    if (!file_.good()) return false;

    track_.assetPath = pathString_;
    return true;
}

bool CurveComponent::LoadTrackAsset(const std::string& assetPath_)
{
    if (assetPath_.empty()) return false;

    const std::filesystem::path path_ = WindowsDialogHelper::Utf8ToPath(assetPath_);
    std::ifstream file_(path_);
    if (!file_.is_open()) return false;

    try
    {
        nlohmann::json assetJson_;
        file_ >> assetJson_;
        if (assetJson_.contains("AssetType") &&
            assetJson_.value("AssetType", "") != "CurveTrack")
        {
            return false;
        }

        if (assetJson_.contains("MinBounds") && assetJson_["MinBounds"].is_array())
        {
            minBounds = { assetJson_["MinBounds"][0], assetJson_["MinBounds"][1] };
        }
        if (assetJson_.contains("MaxBounds") && assetJson_["MaxBounds"].is_array())
        {
            maxBounds = { assetJson_["MaxBounds"][0], assetJson_["MaxBounds"][1] };
        }

        const nlohmann::json& curveJson_ = assetJson_.contains("Curve")
            ? assetJson_.at("Curve")
            : assetJson_;
        if (!curveJson_.is_object() || !curveJson_.contains("Name"))
        {
            return false;
        }

        CurveTrack loadedTrack_(curveJson_.value("Name", path_.stem().string()));
        loadedTrack_.assetPath = assetPath_;
        loadedTrack_.type = static_cast<ImCurveEdit::CurveType>(
            curveJson_.value("Type", static_cast<int>(ImCurveEdit::CurveSmooth)));
        loadedTrack_.color = curveJson_.value("Color", loadedTrack_.color);
        loadedTrack_.visible = curveJson_.value("Visible", true);
        if (curveJson_.contains("Points") && curveJson_["Points"].is_array())
        {
            loadedTrack_.points.clear();
            for (const auto& pointJson_ : curveJson_["Points"])
            {
                if (pointJson_.is_array() && pointJson_.size() >= 2)
                {
                    loadedTrack_.points.push_back({ pointJson_[0], pointJson_[1] });
                }
            }
        }
        loadedTrack_.Sort();

        auto existingTrack_ = std::find_if(
            tracks.begin(),
            tracks.end(),
            [&loadedTrack_](const CurveTrack& track_) { return track_.name == loadedTrack_.name; });
        if (existingTrack_ != tracks.end())
        {
            *existingTrack_ = std::move(loadedTrack_);
        }
        else
        {
            tracks.push_back(std::move(loadedTrack_));
        }
        return true;
    }
    catch (const nlohmann::json::exception&)
    {
        return false;
    }
}

void CurveComponent::DrawTrackManager()
{
    if (ImGui::Button("Add Track"))
    {
        CreateTrack("Track " + std::to_string(tracks.size()), 1.0f);
    }
    ImGui::SameLine();

    bool loadRequested_ = ImGui::Button("Load Curve...");
    std::string droppedPath_;
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload_ = ImGui::AcceptDragDropPayload("ASSET_JSON"))
        {
            droppedPath_ = static_cast<const char*>(payload_->Data);
        }
        ImGui::EndDragDropTarget();
    }
    if (loadRequested_)
    {
        droppedPath_ = WindowsDialogHelper::OpenJsonDialog(L"Load Curve Track", "Data/Curve");
    }
    if (!droppedPath_.empty())
    {
        curveAssetStatus = LoadTrackAsset(droppedPath_)
            ? "Loaded: " + droppedPath_
            : "Not a CurveTrack asset: " + droppedPath_;
    }

    if (!curveAssetStatus.empty())
    {
        ImGui::TextWrapped("%s", curveAssetStatus.c_str());
    }
    ImGui::Separator();

    for (int index_ = 0; index_ < static_cast<int>(tracks.size()); ++index_)
    {
        ImGui::PushID(index_);
        ImGui::Checkbox("##Visible", &tracks[index_].visible);
        ImGui::SameLine();

        if (ImGui::TreeNode(tracks[index_].name.c_str()))
        {
            char nameBuffer_[64]{};
            strcpy_s(nameBuffer_, tracks[index_].name.c_str());
            if (ImGui::InputText("Name", nameBuffer_, IM_ARRAYSIZE(nameBuffer_)))
            {
                tracks[index_].name = nameBuffer_;
            }

            const char* types_[] = { "None", "Discrete", "Linear", "Smooth", "Bezier" };
            int curveType_ = static_cast<int>(tracks[index_].type);
            if (ImGui::Combo("Type", &curveType_, types_, IM_ARRAYSIZE(types_)))
            {
                tracks[index_].type = static_cast<ImCurveEdit::CurveType>(curveType_);
            }

            ImGui::TextDisabled(
                "%s",
                tracks[index_].assetPath.empty() ? "Unsaved curve asset" : tracks[index_].assetPath.c_str());
            if (ImGui::Button("Save"))
            {
                const std::string savePath_ = tracks[index_].assetPath.empty()
                    ? GetDefaultCurveAssetPath(tracks[index_])
                    : tracks[index_].assetPath;
                curveAssetStatus = SaveTrackAsset(index_, savePath_)
                    ? "Saved: " + savePath_
                    : "Failed to save: " + savePath_;
            }
            ImGui::SameLine();
            if (ImGui::Button("Save As..."))
            {
                const std::string initialPath_ = tracks[index_].assetPath.empty()
                    ? GetDefaultCurveAssetPath(tracks[index_])
                    : tracks[index_].assetPath;
                const std::string selectedPath_ = WindowsDialogHelper::SaveJsonDialog(
                    L"Save Curve Track",
                    initialPath_);
                if (!selectedPath_.empty())
                {
                    curveAssetStatus = SaveTrackAsset(index_, selectedPath_)
                        ? "Saved: " + selectedPath_
                        : "Failed to save: " + selectedPath_;
                }
            }
            if (!tracks[index_].assetPath.empty())
            {
                ImGui::SameLine();
                if (ImGui::Button("Reload"))
                {
                    const std::string reloadPath_ = tracks[index_].assetPath;
                    curveAssetStatus = LoadTrackAsset(reloadPath_)
                        ? "Reloaded: " + reloadPath_
                        : "Failed to reload: " + reloadPath_;
                }
            }

            if (ImGui::Button("Remove"))
            {
                tracks.erase(tracks.begin() + index_);
                ImGui::TreePop();
                ImGui::PopID();
                break;
            }

            ImGui::TreePop();
        }
        ImGui::PopID();
    }
}

void CurveComponent::DrawEditorContent(const ImVec2& size_)
{
    if (tracks.empty())
    {
        ImGui::TextDisabled("No curves. Add or load a track first.");
        return;
    }

    if (selectedPoints.Size == 1)
    {
        ImCurveEdit::EditPoint& selectedPoint_ = selectedPoints[0];
        const bool validCurve_ =
            selectedPoint_.curveIndex >= 0 &&
            selectedPoint_.curveIndex < static_cast<int>(tracks.size());
        const bool validPoint_ =
            validCurve_ &&
            selectedPoint_.pointIndex >= 0 &&
            selectedPoint_.pointIndex < static_cast<int>(tracks[selectedPoint_.curveIndex].points.size());
        if (validPoint_)
        {
            CurveTrack& track_ = tracks[selectedPoint_.curveIndex];
            ImVec2 point_ = track_.points[selectedPoint_.pointIndex];
            ImGui::Text("Selected: %s / Point %d", track_.name.c_str(), selectedPoint_.pointIndex);
            ImGui::SetNextItemWidth(140.0f);
            bool changed_ = ImGui::DragFloat("Time (X)", &point_.x, 0.001f);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(140.0f);
            changed_ |= ImGui::DragFloat("Value (Y)", &point_.y, 0.001f);
            if (changed_)
            {
                selectedPoint_.pointIndex = EditPoint(
                    selectedPoint_.curveIndex,
                    selectedPoint_.pointIndex,
                    point_);
            }
        }
        else
        {
            selectedPoints.clear();
        }
    }
    else if (selectedPoints.Size > 1)
    {
        ImGui::Text("Selected Points: %d", selectedPoints.Size);
    }
    const float availableWidth_ = ImGui::GetContentRegionAvail().x;
    const float boundsWidth_ = (std::max)(
        80.0f,
        (availableWidth_ - ImGui::GetStyle().ItemSpacing.x) * 0.5f);
    ImGui::SetNextItemWidth(boundsWidth_);
    ImGui::DragFloat2("Min Bounds", &minBounds.x, 0.01f);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(boundsWidth_);
    ImGui::DragFloat2("Max Bounds", &maxBounds.x, 0.01f);

    ImVec2 editorSize_ = size_;
    if (editorSize_.x <= 0.0f || editorSize_.y <= 0.0f)
    {
        editorSize_ = ImGui::GetContentRegionAvail();
    }
    editorSize_.x = (std::max)(editorSize_.x, 1.0f);
    editorSize_.y = (std::max)(editorSize_.y - 4.0f, 80.0f);
    ImCurveEdit::Edit(
        *this,
        editorSize_,
        static_cast<unsigned int>(reinterpret_cast<uintptr_t>(this)),
        nullptr,
        &selectedPoints);
}

void CurveComponent::Gui()
{
    DrawTrackManager();

    if (ImGui::Button("Open Curve Editor"))
    {
        isWindowOpen = true;
    }

    if (isWindowOpen)
    {
        ImGui::SetNextWindowSize(ImVec2(600.0f, 400.0f), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Curve Editor", &isWindowOpen))
        {
            DrawEditorContent();
        }
        ImGui::End();
    }
}

void CurveComponent::Serialize(nlohmann::json& json_) const
{
    json_["MinBounds"] = { minBounds.x, minBounds.y };
    json_["MaxBounds"] = { maxBounds.x, maxBounds.y };
    nlohmann::json tracksJson_ = nlohmann::json::array();
    for (const auto& track_ : tracks)
    {
        nlohmann::json trackJson_;
        trackJson_["Name"] = track_.name;
        trackJson_["AssetPath"] = track_.assetPath;
        trackJson_["Type"] = static_cast<int>(track_.type);
        trackJson_["Color"] = track_.color;
        trackJson_["Visible"] = track_.visible;
        trackJson_["Points"] = nlohmann::json::array();
        for (const auto& point_ : track_.points)
        {
            trackJson_["Points"].push_back({ point_.x, point_.y });
        }
        tracksJson_.push_back(std::move(trackJson_));
    }
    json_["Tracks"] = std::move(tracksJson_);
}

void CurveComponent::Deserialize(const nlohmann::json& json_)
{
    if (json_.contains("MinBounds"))
    {
        minBounds.x = json_["MinBounds"][0];
        minBounds.y = json_["MinBounds"][1];
    }
    if (json_.contains("MaxBounds"))
    {
        maxBounds.x = json_["MaxBounds"][0];
        maxBounds.y = json_["MaxBounds"][1];
    }

    tracks.clear();
    if (json_.contains("Tracks"))
    {
        for (const auto& trackJson_ : json_["Tracks"])
        {
            CurveTrack track_(trackJson_.value("Name", "Track"));
            track_.assetPath = trackJson_.value("AssetPath", "");
            track_.type = static_cast<ImCurveEdit::CurveType>(
                trackJson_.value("Type", static_cast<int>(ImCurveEdit::CurveSmooth)));
            track_.color = trackJson_.value("Color", track_.color);
            track_.visible = trackJson_.value("Visible", true);
            if (trackJson_.contains("Points"))
            {
                track_.points.clear();
                for (const auto& point_ : trackJson_["Points"])
                {
                    track_.points.push_back({ point_[0], point_[1] });
                }
            }
            track_.Sort();
            tracks.push_back(std::move(track_));
        }
    }
}

float CurveComponent::GetValue(const std::string& name_, float time_) const
{
    for (const auto& track_ : tracks)
    {
        if (track_.name == name_)
        {
            return track_.Evaluate(time_);
        }
    }
    return 0.0f;
}
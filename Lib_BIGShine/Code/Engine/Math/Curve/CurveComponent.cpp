#include "CurveComponent.h"


std::vector<std::string> CurveComponent::GetTrackNames() const
{
    std::vector<std::string> names;
    for (const auto& t : tracks) {
        names.push_back(t.name);
    }
    return names;
}

void CurveComponent::Gui()
{
    // --- 1. 元のウィンドウ：トラックの管理と選択 ---
    if (ImGui::Button("Add Track")) 
    {
        tracks.emplace_back("Track " + std::to_string(tracks.size()));
    }

    ImGui::SameLine();

    // エディタを開くボタン
    if (ImGui::Button("Open Curve Editor"))
    {
        isWindowOpen = true;
    }

    ImGui::Separator();

    for (int i = 0; i < (int)tracks.size(); ++i) 
    {
        ImGui::PushID(i);
        // 可視性チェックボックス（エディタ上の表示切り替え）
        ImGui::Checkbox("##v", &tracks[i].visible); ImGui::SameLine();

        if (ImGui::TreeNode(tracks[i].name.c_str()))
        {
            char buf[64];
            strcpy_s(buf, tracks[i].name.c_str());
            if (ImGui::InputText("Name", buf, 64)) tracks[i].name = buf;

            const char* types[] = { "None", "Discrete", "Linear", "Smooth", "Bezier" };
            int curType = (int)tracks[i].type;
            if (ImGui::Combo("Type", &curType, types, 5)) tracks[i].type = (ImCurveEdit::CurveType)curType;

            // 削除ボタンもここにあると便利です
            if (ImGui::Button("Remove"))
            {
                tracks.erase(tracks.begin() + i);
                ImGui::TreePop(); ImGui::PopID();
                break;
            }

            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    // --- 2. 別ウィンドウ：カーブエディタ ---
    if (isWindowOpen) 
    {
        ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Curve Editor", &isWindowOpen)) 
        {
            if (!tracks.empty()) 
{
                // ウィンドウ上部に境界設定を配置
                ImGui::SetNextItemWidth(150);
                ImGui::DragFloat2("Min Bounds", &minBounds.x, 0.01f);
                ImGui::SameLine();
                ImGui::SetNextItemWidth(150);
                ImGui::DragFloat2("Max Bounds", &maxBounds.x, 0.01f);

                // ウィンドウの残りの領域いっぱいにエディタを表示
                ImVec2 editorSize = ImGui::GetContentRegionAvail();
                editorSize.y -= 10; // 少し余白を作る

                ImCurveEdit::Edit(*this, editorSize, (unsigned int)((uintptr_t)this));
            }
            else
            {
                ImGui::Text("No tracks available. Add a track in the inspector.");
            }
        }
        ImGui::End();
    }
}

void CurveComponent::Serialize(nlohmann::json& j) const 
{
    j["MinBounds"] = { minBounds.x, minBounds.y };
    j["MaxBounds"] = { maxBounds.x, maxBounds.y };
    nlohmann::json tArray = nlohmann::json::array();
    for (const auto& t : tracks) {
        nlohmann::json tj;
        tj["Name"] = t.name;
        tj["Type"] = (int)t.type;
        nlohmann::json pts;
        for (const auto& p : t.points) pts.push_back({ p.x, p.y });
        tj["Points"] = pts;
        tArray.push_back(tj);
    }
    j["Tracks"] = tArray;
}

void CurveComponent::Deserialize(const nlohmann::json& j)
{
    if (j.contains("MinBounds")) {
        minBounds.x = j["MinBounds"][0];
        minBounds.y = j["MinBounds"][1];
    }
    if (j.contains("MaxBounds")) {
        maxBounds.x = j["MaxBounds"][0];
        maxBounds.y = j["MaxBounds"][1];
    }

    tracks.clear();
    if (j.contains("Tracks")) {
        for (const auto& tj : j["Tracks"]) {
            CurveTrack t(tj.value("Name", "Track"));
            t.type = (ImCurveEdit::CurveType)tj.value("Type", (int)ImCurveEdit::CurveSmooth);
            if (tj.contains("Points")) {
                t.points.clear();
                for (const auto& p : tj["Points"]) t.points.push_back({ p[0], p[1] });
            }
            t.Sort();
            tracks.push_back(t);
        }
    }
}

float CurveComponent::GetValue(const std::string& name, float t) const {
    for (const auto& tr : tracks) {
        if (tr.name == name) return tr.Evaluate(t);
    }
    return 0.0f;
}
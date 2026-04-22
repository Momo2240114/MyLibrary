#include "FovDirector.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "CameraComponent.h"

void FovDirector::Start()
{
    auto easingComp = GetGameObject()->GetOrAddComponent<EasingComponent>();
    easingIndex = easingComp->AddEasingAndGetIndex();
}

void FovDirector::Update(float et)
{
    auto cam = GetGameObject()->GetComponent<CameraComponent>();
    auto easingComp = GetGameObject()->GetComponent<EasingComponent>();
    if (!isMoving || !cam || !easingComp) return;

    float ratio = easingComp->GetRatio(easingIndex);
    float easedT = easingComp->GetValue(easingIndex);
    float currentFov = (targetData.peakRate > 0.0f) ?
        (ratio < targetData.peakRate ?
            Lerp(startFov, targetData.peakValue, ratio / targetData.peakRate) :
            Lerp(targetData.peakValue, targetData.fov, (ratio - targetData.peakRate) / (1.0f - targetData.peakRate)))
        : Lerp(startFov, targetData.fov, easedT);

    cam->SetFov(currentFov);
    if (easingComp->GetEndFlag(easingIndex)) isMoving = false;
}

void FovDirector::PlayFov(const FovWayPoint& wp, EasingType type)
{
    auto cam = GetGameObject()->GetComponent<CameraComponent>();
    auto easingComp = GetGameObject()->GetOrAddComponent<EasingComponent>();
    startFov = cam->GetFovY();
    targetData = wp;
    // 2. イージング進捗のリセット
    easingComp->SetStartValue(easingIndex, 0.0f);
    // 3. 再生
    easingComp->PlayEasing(easingIndex, type, 1.0f / wp.duration, 1.0f);
    isMoving = true;
}

void FovDirector::Gui()
{
    auto cam = GetGameObject()->GetComponent<CameraComponent>();
    if (!cam) { ImGui::Text("CameraComponent not found!"); return; }

    if (ImGui::Button("Capture Current FOV")) {
        CaptureCurrentState();
    }

    ImGui::Separator();

    for (int i = 0; i < (int)EditorWaypoints.size(); ++i) {
        ImGui::PushID(i);
        // インデックスを表示して親(CameraDirector)の指定番号と一致するようにする
        std::string label = "[" + std::to_string(i) + "] FOV Waypoint";
        if (ImGui::TreeNode(label.c_str())) {
            auto& wp = EditorWaypoints[i];

            ImGui::DragFloat("Target FOV", &wp.fov, 0.5f, 1.0f, 170.0f);
            ImGui::DragFloat("Duration", &wp.duration, 0.1f, 0.0f, 10.0f);

            if (ImGui::TreeNode("Special Peak Settings")) {
                ImGui::SliderFloat("Peak Rate", &wp.peakRate, 0.0f, 1.0f);
                ImGui::DragFloat("Peak Value", &wp.peakValue, 0.5f);
                ImGui::TreePop();
            }

            if (ImGui::Button("Play This")) {
                PlayFov(wp, EasingType::OutExpo);
            }
            ImGui::SameLine();
            if (ImGui::Button("Remove")) {
                EditorWaypoints.erase(EditorWaypoints.begin() + i);
                i--;
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
}

void FovDirector::Serialize(nlohmann::json& j) const
{
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& wp : EditorWaypoints) {
        arr.push_back({ {"fov", wp.fov}, {"dur", wp.duration}, {"pr", wp.peakRate}, {"pv", wp.peakValue} });
    }
    j["FovWaypoints"] = arr;
}

void FovDirector::Deserialize(const nlohmann::json& j)
{
    if (!j.contains("FovWaypoints") || !j["FovWaypoints"].is_array()) return;

    EditorWaypoints.clear();
    for (const auto& wj : j["FovWaypoints"]) {
        FovWayPoint wp;
        // .value("key", default) を使うことで、キーが万が一無くてもクラッシュを防ぐ
        wp.fov = wj.value("fov", 60.0f);
        wp.duration = wj.value("dur", 1.0f);
        wp.peakRate = wj.value("pr", 0.0f);
        wp.peakValue = wj.value("pv", 0.0f);

        EditorWaypoints.push_back(wp);
    }
}
void FovDirector::CaptureCurrentState()
{
        auto cam = GetGameObject()->GetComponent<CameraComponent>();
        if (!cam) return;

        FovWayPoint wp;
        wp.fov = cam->GetFovY();
        wp.duration = 1.0f; // デフォルト
        wp.peakRate = 0.0f;
        wp.peakValue = 0.0f;
        EditorWaypoints.push_back(wp);
}

void FovDirector::PlayFovByIndex(int idx, EasingType type)
{
    if (idx >= 0 && idx < (int)EditorWaypoints.size()) {
        PlayFov(EditorWaypoints[idx], type);
    }
}

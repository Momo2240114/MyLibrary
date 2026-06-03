#include "CameraDirectorComponent.h"
#include "Engine/GamePlay/Camera/CameraComponent.h"
#include "Engine/GamePlay/Serialize/ActionRegistry.h"


void CameraDirector::Start()
{
    auto go = GetGameObject();
    // 依存コンポーネントを自動で追加・取得
    transformDirector = go->GetOrAddComponent<TransformDirector>();
    fovDirector = go->GetOrAddComponent<FovDirector>();

    RegisterActions();
}

void CameraDirector::Update(float et)
{
    if(transformDirector)
    { 
        transformDirector = GetGameObject()->GetComponent<TransformDirector>();
        RegisterActions();
    }
    if (fovDirector)
    {
        fovDirector = GetGameObject()->GetComponent<FovDirector>();
        RegisterActions();
    }
}

void CameraDirector::Gui()
{
    ImGui::Text("Camera Director: %s", GetGameObject()->GetName().c_str());

    if (ImGui::Button("Capture Combined (Current View)")) {
        CaptureCombinedWaypoint();
    }

    for (int i = 0; i < (int)combinedWaypoints.size(); ++i) {
        ImGui::PushID(i);
        auto& cp = combinedWaypoints[i];

        if (ImGui::TreeNode(("Combined WP " + std::to_string(i)).c_str())) {
            // インデックスの変更も可能にする（子の最大数を超えないようにSliderで制限）
            ImGui::SliderInt("T-Idx", &cp.transformIdx, 0, transformDirector->GetCount() - 1);
            ImGui::SliderInt("F-Idx", &cp.fovIdx, 0, fovDirector->GetCount() - 1);

            ImGui::Checkbox("Auto Return", &cp.m_autoReturn);
            if (cp.m_autoReturn) {
                ImGui::DragFloat("Wait Time", &cp.m_waitTime, 0.1f, 0.0f, 10.0f);
            }

            if (ImGui::Button("Play Pair")) {
                PlayCombined(i);
            }

            if (ImGui::Button("Remove")) {
                combinedWaypoints.erase(combinedWaypoints.begin() + i);
                i--;
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
}

void CameraDirector::PlayCombined(int index, EasingType transEase, EasingType fovEase)
{
    if (index < 0 || index >= (int)combinedWaypoints.size()) return;

    auto& cp = combinedWaypoints[index];

    // 子 Director にインデックスを渡して再生（PlayByIndex という関数を子に用意する）
    if (transformDirector) {
        transformDirector->PlayMoveByIndex(cp.transformIdx);
    }
    if (fovDirector) {
        fovDirector->PlayFovByIndex(cp.fovIdx, fovEase);
    }
}

// --- Capture: 今の見た目をそれぞれのリストに追加してペアリング ---
void CameraDirector::CaptureCombinedWaypoint() {
    if (!transformDirector || !fovDirector) return;

    // 1. 子に「今の状態を自分のリストに保存しろ」と命令
    transformDirector->CaptureCurrentState();
    fovDirector->CaptureCurrentState();

    // 2. 親は「末尾のインデックス同士」をペアとして記録
    CombinedWaypoint cp;
    cp.transformIdx = transformDirector->GetCount() - 1;
    cp.fovIdx = fovDirector->GetCount() - 1;
    combinedWaypoints.push_back(cp);
}

void CameraDirector::RegisterActions()
{
    auto& reg = ActionRegistry::Instance();
    std::string objName = GetGameObject()->GetName();

    for (int i = 0; i < (int)combinedWaypoints.size(); ++i) {
        // 名前を "ObjName_WP_0" にする
        std::string actionName = objName + "_WP_" + std::to_string(i);

        // 第1引数にカテゴリ "Camera" を指定
        reg.RegisterAction("Camera", actionName, [this, i]() {
            this->PlayCombined(i);
            });
    }
}

void CameraDirector::Serialize(nlohmann::json& j) const 
{
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& cp : combinedWaypoints) {
        nlohmann::json item;
        item["t_idx"] = cp.transformIdx;
        item["f_idx"] = cp.fovIdx;
        item["autoReturn"] = cp.m_autoReturn; // 追加
        item["waitTime"] = cp.m_waitTime;     // 追加
        arr.push_back(item);
    }
    j["CombinedWaypoints"] = arr;

}

void CameraDirector::Deserialize(const nlohmann::json& j)
{
    combinedWaypoints.clear();
    for (const auto& item : j["CombinedWaypoints"]) {
        CombinedWaypoint cp;
        // JSONにあるキー "t_idx", "f_idx" を読み込む
        cp.transformIdx = item.value("t_idx", 0);
        cp.fovIdx = item.value("f_idx", 0);
        cp.m_autoReturn = item.value("autoReturn", false); // 追加
        cp.m_waitTime = item.value("waitTime", 0.0f);      // 追加
        combinedWaypoints.push_back(cp);
    }
}
#include "ColorFilterDirector.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/GamePlay/Serialize/ActionRegistry.h"


void ColorFilterDirector::Start() 
{
    auto go = GetGameObject();
    auto easingComp = go->GetOrAddComponent<EasingComponent>();
    easingIndex = easingComp->AddEasingAndGetIndex();
    RegisterActions();
}

void ColorFilterDirector::Update(float et) 
{
    auto easingComp = GetGameObject()->GetComponent<EasingComponent>();
    if (!isChanging || !easingComp) return;

    float t = easingComp->GetValue(easingIndex);

    // 現在のシステム設定を取得 (シングルトン等を想定)
    auto& current = GraphicsDeviceManager::Instance().GetFilterParams();

    // 各パラメータを補完
    current.hueShift = Lerp(startParams.hueShift, targetParams.hueShift, t);
    current.saturation = Lerp(startParams.saturation, targetParams.saturation, t);
    current.brightness = Lerp(startParams.brightness, targetParams.brightness, t);
    current.contrast = Lerp(startParams.contrast, targetParams.contrast, t);
    current.colorBalance = LerpVec3(startParams.colorBalance, targetParams.colorBalance, t);
    current.filterIntensity = Lerp(startParams.filterIntensity, targetParams.filterIntensity, t);

    if (easingComp->GetEndFlag(easingIndex)) isChanging = false;
}

void ColorFilterDirector::PlayFilter(const ColorFilterWayPoint& wp) 
{
    auto easingComp = GetGameObject()->GetOrAddComponent<EasingComponent>();

    // 現在の値を開始地点にする
    startParams = GraphicsDeviceManager::Instance().GetFilterParams();
    targetParams = wp.filter;

    easingComp->SetStartValue(easingIndex, 0.0f);
    easingComp->PlayEasing(easingIndex, wp.easing, 1.0f / wp.duration, 1.0f);
    isChanging = true;
}

void ColorFilterDirector::Gui() 
{

    if (ImGui::Button("Capture Current Filter")) {
        ColorFilterWayPoint wp;
        wp.filter = GraphicsDeviceManager::Instance().GetFilterParams();
        wp.duration = 1.0f;
        wp.easing = EasingType::Lerp;
        EditorWaypoints.push_back(wp);
    }

    for (int i = 0; i < (int)EditorWaypoints.size(); ++i) {
        ImGui::PushID(i);
        if (ImGui::TreeNode(("Filter WP " + std::to_string(i)).c_str())) {
            auto& f = EditorWaypoints[i].filter;
            ImGui::SliderFloat("Hue", &f.hueShift, -180.0f, 180.0f);
            ImGui::SliderFloat("Saturation", &f.saturation, 0.0f, 2.0f);
            ImGui::SliderFloat("Brightness", &f.brightness, 0.0f, 2.0f);
            ImGui::SliderFloat("Contrast", &f.contrast, 0.0f, 2.0f);
            ImGui::ColorEdit3("Balance", &f.colorBalance.x);
            ImGui::SliderFloat("Intensity", &f.filterIntensity, 0.0f, 1.0f);

            ImGui::DragFloat("Duration", &EditorWaypoints[i].duration, 0.1f);

            // イージング選択 (EasingTypeに合わせて項目を追加)
            const char* easingNames[] = { "None", "Lerp", "OutLerp", "OutQuint", "InExpo", "OutExpo" };
            int currentEasing = (int)EditorWaypoints[i].easing;
            if (ImGui::Combo("Easing", &currentEasing, easingNames, IM_ARRAYSIZE(easingNames))) {
                EditorWaypoints[i].easing = (EasingType)currentEasing;
            }

            if (ImGui::Button("Play")) PlayFilter(EditorWaypoints[i]);
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

void ColorFilterDirector::RegisterActions() {
    auto& reg = ActionRegistry::Instance();
    std::string objName = GetGameObject()->GetName();

    for (int i = 0; i < (int)EditorWaypoints.size(); ++i) {
        std::string actionName = objName + "_Color_" + std::to_string(i);
        reg.RegisterAction("PostProcess", actionName, [this, i]() {
            this->PlayFilter(EditorWaypoints[i]);
            });
    }
}

void ColorFilterDirector::Serialize(nlohmann::json& j) const
{
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& wp : EditorWaypoints) {
        nlohmann::json item;
        // フィルタパラメータ
        item["hue"] = wp.filter.hueShift;
        item["sat"] = wp.filter.saturation;
        item["bri"] = wp.filter.brightness;
        item["con"] = wp.filter.contrast;
        item["intensity"] = wp.filter.filterIntensity;

        // XMFLOAT3 は [r, g, b] の配列形式で保存
        item["balance"] = { wp.filter.colorBalance.x, wp.filter.colorBalance.y, wp.filter.colorBalance.z };

        // 制御パラメータ
        item["dur"] = wp.duration;
        item["ease"] = (int)wp.easing;

        arr.push_back(item);
    }
    j["ColorFilterWaypoints"] = arr;
}

void ColorFilterDirector::Deserialize(const nlohmann::json& j)
{
    if (!j.contains("ColorFilterWaypoints") || !j["ColorFilterWaypoints"].is_array()) return;

    EditorWaypoints.clear();
    for (const auto& wj : j["ColorFilterWaypoints"]) {
        ColorFilterWayPoint wp;

        // フィルタパラメータの復元
        wp.filter.hueShift = wj.value("hue", 0.0f);
        wp.filter.saturation = wj.value("sat", 1.0f);
        wp.filter.brightness = wj.value("bri", 1.0f);
        wp.filter.contrast = wj.value("con", 1.0f);
        wp.filter.filterIntensity = wj.value("intensity", 1.0f);

        // カラーバランスの復元
        if (wj.contains("balance") && wj["balance"].is_array()) {
            wp.filter.colorBalance.x = wj["balance"][0];
            wp.filter.colorBalance.y = wj["balance"][1];
            wp.filter.colorBalance.z = wj["balance"][2];
        }
        else {
            wp.filter.colorBalance = { 1.0f, 1.0f, 1.0f };
        }

        // 制御パラメータの復元
        wp.duration = wj.value("dur", 1.0f);
        wp.easing = (EasingType)wj.value("ease", (int)EasingType::Lerp);

        EditorWaypoints.push_back(wp);
    }
}
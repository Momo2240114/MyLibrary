#include "LightBlinkerComponent.h"
#include "Engine/Graphics/Core/LightManager.h"

void LightBlinkerComponent::Start()
{
    light = GetGameObject()->GetComponent<LightComponent>();
    easingComp = GetGameObject()->GetComponent<EasingComponent>();

    if (light) {

    }

    if (easingComp) {
        // Intensity用とRange用のイージングスロットを確保
        intensityIdx = easingComp->AddEasingAndGetIndex();
        rangeIdx = easingComp->AddEasingAndGetIndex();

        // 最初のイージングを開始
        RestartIntensityEasing();
    }
}

void LightBlinkerComponent::Update(float et)
{
    if (!active || !light || !easingComp) return;

    // イージング値をライトに適用
    light->intensity = easingComp->GetValue(intensityIdx);

    if (useRangeBlink) {
        light->range = easingComp->GetValue(rangeIdx);
    }

    // 完了したら反転して再開
    if (easingComp->GetEndFlag(intensityIdx)) {
        isTargetState = !isTargetState;
        RestartIntensityEasing();
    }
}

void LightBlinkerComponent::RestartIntensityEasing()
{
    if (!easingComp) return;

    // Intensityの計算
    // 次の点滅速度をランダムに決定
    float currentSpeed = minSpeed + (static_cast<float>(rand()) / RAND_MAX) * (maxSpeed - minSpeed);

    float targetI = isTargetState ? (baseIntensity * intensityMultiplier) : baseIntensity;

    // speed の代わりに currentSpeed を使う
    easingComp->SetStartValue(intensityIdx, light->intensity);
    easingComp->PlayEasing(intensityIdx, type, currentSpeed, targetI);

    // Rangeの計算 (有効な場合のみ)
    if (useRangeBlink) {
        float targetR = isTargetState ? (baseRange * rangeMultiplier) : baseRange;
        easingComp->SetStartValue(rangeIdx, light->range);
        easingComp->PlayEasing(rangeIdx, type, currentSpeed, targetR);
    }
}

void LightBlinkerComponent::Gui()
{
    ImGui::Checkbox("Active", &active);

    if (ImGui::TreeNode("Intensity Settings")) {
        if (ImGui::DragFloat("Base Intensity", &baseIntensity, 0.1f)) {
            if (!active) light->intensity = baseIntensity; // 非実行時は即反映
        }
        ImGui::DragFloat("Multiplier", &intensityMultiplier, 0.05f, 0.0f, 5.0f);
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Range Settings")) {
        ImGui::Checkbox("Use Range Blink", &useRangeBlink);
        if (ImGui::DragFloat("Base Range", &baseRange, 0.1f)) {
            if (!active) light->range = baseRange;
        }
        ImGui::DragFloat("Range Multiplier", &rangeMultiplier, 0.05f, 0.0f, 5.0f);
        ImGui::TreePop();
    }

    ImGui::Separator();

    // 最小・最大速度をまとめて設定できるようにする
    float speeds[2] = { minSpeed, maxSpeed };
    if (ImGui::DragFloat2("Speed Range (Min/Max)", speeds, 0.01f, 0.01f, 10.0f)) {
        minSpeed = speeds[0];
        maxSpeed = speeds[1];

        // 最小値が最大値を上回らないように調整
        if (minSpeed > maxSpeed) maxSpeed = minSpeed;
    }



    const char* easingNames[] = { "None", "Lerp", "OutLerp", "OutQuint", "InExpo", "OutExpo" };
    int typeIdx = static_cast<int>(type);
    if (ImGui::Combo("Easing Type", &typeIdx, easingNames, 6)) {
        type = static_cast<EasingType>(typeIdx);
    }
}

    // --- シリアライズ ---
void LightBlinkerComponent::Serialize(nlohmann::json& j) const 
{
    j["Active"] = active;
    j["MinSpeed"] = minSpeed;  // 追加
    j["MaxSpeed"] = maxSpeed;  // 追加
    j["EasingType"] = static_cast<int>(type);
    j["BaseIntensity"] = baseIntensity;
    j["IntensityMultiplier"] = intensityMultiplier;
    j["BaseRange"] = baseRange;
    j["RangeMultiplier"] = rangeMultiplier;
    j["UseRangeBlink"] = useRangeBlink;
}

void LightBlinkerComponent::Deserialize(const nlohmann::json& j)
{
    active = j.value("Active", true);
    minSpeed = j.value("MinSpeed", 0.5f);
    maxSpeed = j.value("MaxSpeed", 2.0f);
    type = static_cast<EasingType>(j.value("EasingType", 1));
    baseIntensity = j.value("BaseIntensity", 1.0f);
    intensityMultiplier = j.value("IntensityMultiplier", 0.0f);
    baseRange = j.value("BaseRange", 10.0f);
    rangeMultiplier = j.value("RangeMultiplier", 1.0f);
    useRangeBlink = j.value("UseRangeBlink", false);
}
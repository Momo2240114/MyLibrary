#include "CurveEditor.h"
#include "CurveEditorDelegate.h"
#include "ImGuiBezier.h"
#include "Wip.hpp"
#include "imgui.h"

//---------------------------
// CurveEditor
//---------------------------
void CurveEditor::Draw(CurveData& curve)
{
    ImGui::Text("Curve: %s", curve.name.c_str());
    ImGui::Separator();

    // モード切替
    int currentMode = static_cast<int>(mode);
    const char* modes[] = { "Free", "Bezier" };
    if (ImGui::Combo("Mode", &currentMode, modes, IM_ARRAYSIZE(modes)))
        mode = (CurveEditorMode)currentMode;

    DrawInterpolation(curve);

    switch (mode)
    {
    case CurveEditorMode::FREE:
        DrawKeyList(curve);
        DrawGraph(curve);
        break;
    case CurveEditorMode::BEZIER:
        DrawBezierEditor(curve);
        break;
    default:
        break;
    }

    ImGui::Separator();
}

void CurveEditor::DrawInterpolation(CurveData& curve)
{
    const char* types[] = { "Linear", "Smooth", "Constant" };
    int current = (int)curve.interpolation;
    if (ImGui::Combo("Interpolation", &current, types, IM_ARRAYSIZE(types)))
        curve.interpolation = (CurveInterpolation)current;
}

void CurveEditor::DrawKeyList(CurveData& curve)
{
    if (!ImGui::CollapsingHeader("Keys")) return;

    // 正規化ボタン
    if (ImGui::Button("Normalize AllKeys"))
    {
        if (!curve.keys.empty())
        {
            // 時間の最大値を取得
            float maxTime = 0.0f;
            float maxValue = 0.0f;
            for (const auto& k : curve.keys)
            {
                if (k.time > maxTime) maxTime = k.time;
                if (fabsf(k.value) > maxValue) maxValue = fabsf(k.value);
            }
            if (maxTime > 0.0f)
            {
                for (auto& k : curve.keys)
                    k.time /= maxTime;
            }
            if (maxValue > 0.0f)
            {
                for (auto& k : curve.keys)
                    k.value /= maxValue;
            }
        }
    }

    for (int i = 0; i < (int)curve.keys.size(); ++i)
    {
        auto& k = curve.keys[i];
        ImGui::PushID(i);

        ImGui::DragFloat("Time", &k.time, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("Value", &k.value, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("In", &k.inTangent, -1.0f, 1.0f);
        ImGui::DragFloat("Out", &k.outTangent, -1.0f, 1.0f);

        if (ImGui::Button("Delete"))
            curve.keys.erase(curve.keys.begin() + i);

        ImGui::Separator();
        ImGui::PopID();
    }

    if (ImGui::Button("Add Key"))
        curve.keys.push_back({ 0.0f, 0.0f, 0.0f, 0.0f });
}

ImVec2 MapToScreenNormalized(const CurveKey& key, ImVec2 origin, ImVec2 size)
{
    float x = origin.x + key.time * size.x;
    float y = origin.y + size.y - key.value * size.y;
    return ImVec2(x, y);
}

void CurveEditor::DrawGraph(CurveData& curve)
{
    // 折りたたみ可能ヘッダー
    if (ImGui::CollapsingHeader("Curve Graph"))
    {
        // 正規化ボタン
        if (ImGui::Button("Normalize AllKeys"))
        {
            if (!curve.keys.empty())
            {
                // 時間の最大値を取得
                float maxTime = 0.0f;
                float maxValue = 0.0f;
                for (const auto& k : curve.keys)
                {
                    if (k.time > maxTime) maxTime = k.time;
                    if (fabsf(k.value) > maxValue) maxValue = fabsf(k.value);
                }
                if (maxTime > 0.0f)
                {
                    for (auto& k : curve.keys)
                        k.time /= maxTime;
                }
                if (maxValue > 0.0f)
                {
                    for (auto& k : curve.keys)
                        k.value /= maxValue;
                }
            }
        }

        // 横幅をウィンドウの利用可能幅に合わせ、縦は同じ値で正方形
        ImVec2 avail = ImGui::GetContentRegionAvail();
        ImVec2 canvasSize(avail.x, avail.x); // 横幅に合わせて縦も同じに

        // ImCurveEdit による直接編集
        static CurveEditorDelegate delegate(&curve);
        ImCurveEdit::Edit(delegate, canvasSize, 12345);
    }
}

void CurveEditor::DrawBezierEditor(CurveData& curve)
{
    float P[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
    if (curve.keys.size() >= 2)
    {
        P[0] = curve.keys[1].time;
        P[1] = curve.keys[1].value;
    }
    if (curve.keys.size() >= 4)
    {
        P[2] = curve.keys[curve.keys.size() - 2].time;
        P[3] = curve.keys[curve.keys.size() - 2].value;
    }
    if (ImGui::BEZIER("Bezier Curve", P))
    {
        if (curve.keys.size() < 2)
            curve.keys.resize(2);
        curve.keys[0].time = 0.0f;
        curve.keys[0].value = 0.0f;
        curve.keys[1].time = P[0];
        curve.keys[1].value = P[1];

        if (curve.keys.size() < 4)
            curve.keys.resize(4);
        curve.keys[curve.keys.size() - 2].time = P[2];
        curve.keys[curve.keys.size() - 2].value = P[3];
    }
}

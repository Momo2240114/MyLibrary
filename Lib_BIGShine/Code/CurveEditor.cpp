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

void CurveEditor::DrawGraph(CurveData & curve)
{
    // ウィンドウの余白をゼロにして開始
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("CurveEditor", nullptr, ImGuiWindowFlags_NoScrollbar);

    ImVec2 avail = ImGui::GetContentRegionAvail();
    static CurveEditorDelegate delegate(&curve);

    // 1. まずエディターを描画（ここで内部のサイズが確定する）
    ImCurveEdit::Edit(delegate, avail, 12345);

    // 2. 「直前に描画されたアイテム（グラフ）」の実際の四角形範囲を取得
    ImVec2 graphMin = ImGui::GetItemRectMin();
    ImVec2 graphMax = ImGui::GetItemRectMax();
    ImVec2 graphSize = ImGui::GetItemRectSize();

    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // 3. 【赤い線】グラフの「描画領域としての右端」に引く
    // ImCurveEdit の仕様上、右端にマージンがある場合は graphMax.x より少し内側になります
    // Delegate の Max.x が 1.0 なら、以下の計算でピッタリ合います
    float endX = graphMax.x;

    draw_list->AddLine(
        ImVec2(endX, graphMin.y),
        ImVec2(endX, graphMax.y),
        IM_COL32(255, 0, 0, 255), 2.0f
    );

    // 4. 【白い線】中央（1.0倍速）
    float midY = graphMin.y + graphSize.y * 0.5f;
    draw_list->AddLine(
        ImVec2(graphMin.x, midY),
        ImVec2(graphMax.x, midY),
        IM_COL32(255, 255, 255, 100), 1.0f
    );

    ImGui::End();
    ImGui::PopStyleVar();
}

    //if (ImGui::Button("Fit to 1.0s Range")) {
    //    if (!curve.keys.empty()) {
    //        float maxT = curve.keys.back().time;
    //        if (maxT > 0) {
    //            for (auto& k : curve.keys) k.time /= maxT;
    //        }
    //    }
    //}

    //// 横幅をウィンドウの利用可能幅に合わせ、縦は同じ値で正方形
    //ImVec2 avail = ImGui::GetContentRegionAvail();
    //// 縦幅を 300~400 くらいに固定すると、非常に見やすくなります
    //ImVec2 canvasSize(avail.x, 350.0f);

    //// ImCurveEdit による直接編集
    //static CurveEditorDelegate delegate(&curve);
    //ImCurveEdit::Edit(delegate, canvasSize, 12345);
//}

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

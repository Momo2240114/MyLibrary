#pragma once
// ImGui Bezier widget. @r-lyeh, public domain
// v1.03: VSà¿ëSëŒâûÅAImGui åˆéÆ API Ç…ìùàÍ
// Usage:
// {  static float v[] = { 0.390f, 0.575f, 0.565f, 1.000f }; 
//    ImGui::Bezier( "easeOutSine", v );       // draw
//    float y = ImGui::BezierValue( 0.5f, v ); // x delta in [0..1] range
// }

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>
#include <cstdio>

namespace ImGui
{
    template<int steps>
    void bezier_table(ImVec2 P[4], ImVec2 results[steps + 1]) {
        static float C[(steps + 1) * 4], * K = 0;
        if (!K) {
            K = C;
            for (unsigned step = 0; step <= steps; ++step) {
                float t = (float)step / (float)steps;
                C[step * 4 + 0] = (1 - t) * (1 - t) * (1 - t);   // P0
                C[step * 4 + 1] = 3 * (1 - t) * (1 - t) * t;     // P1
                C[step * 4 + 2] = 3 * (1 - t) * t * t;           // P2
                C[step * 4 + 3] = t * t * t;                     // P3
            }
        }
        for (unsigned step = 0; step <= steps; ++step) {
            ImVec2 point = {
                K[step * 4 + 0] * P[0].x + K[step * 4 + 1] * P[1].x + K[step * 4 + 2] * P[2].x + K[step * 4 + 3] * P[3].x,
                K[step * 4 + 0] * P[0].y + K[step * 4 + 1] * P[1].y + K[step * 4 + 2] * P[2].y + K[step * 4 + 3] * P[3].y
            };
            results[step] = point;
        }
    }

    inline float BezierValue(float dt01, float P[4]) {
        enum { STEPS = 256 };
        ImVec2 Q[4] = { {0,0}, {P[0], P[1]}, {P[2], P[3]}, {1,1} };
        ImVec2 results[STEPS + 1];
        bezier_table<STEPS>(Q, results);
        int idx = (int)((dt01 < 0 ? 0 : dt01 > 1 ? 1 : dt01) * STEPS);
        return results[idx].y;
    }

    inline int Bezier(const char* label, float P[4])
    {
        enum { SMOOTHNESS = 64, CURVE_WIDTH = 4, LINE_WIDTH = 1, GRAB_RADIUS = 6, GRAB_BORDER = 2 };

        const ImGuiStyle& style = GetStyle();
        ImGuiIO& io = GetIO();
        ImDrawList* draw_list = GetWindowDrawList();
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems) return false;

        int changed = SliderFloat4(label, P, 0, 1, "%.3f", 1.0f);
        int hovered = IsItemActive() || IsItemHovered();
        Dummy(ImVec2(0, 3));

        // canvas
        float avail = GetContentRegionAvailWidth();
        float dim = ImMin(avail, 128.f);
        ImVec2 canvas(dim, dim);
        ImRect bb(window->DC.CursorPos, window->DC.CursorPos + canvas);
        ItemSize(bb);
        if (!ItemAdd(bb, 0)) return changed;

        RenderFrame(bb.Min, bb.Max, GetColorU32(ImGuiCol_FrameBg), true, style.FrameRounding);

        // grid
        for (int i = 0; i <= 4; ++i) {
            float fx = bb.Min.x + canvas.x * i / 4.0f;
            float fy = bb.Min.y + canvas.y * i / 4.0f;
            draw_list->AddLine(ImVec2(fx, bb.Min.y), ImVec2(fx, bb.Max.y), GetColorU32(ImGuiCol_TextDisabled));
            draw_list->AddLine(ImVec2(bb.Min.x, fy), ImVec2(bb.Max.x, fy), GetColorU32(ImGuiCol_TextDisabled));
        }

        // eval curve
        ImVec2 Q[4] = { {0,0}, {P[0], P[1]}, {P[2], P[3]}, {1,1} };
        ImVec2 results[SMOOTHNESS + 1];
        bezier_table<SMOOTHNESS>(Q, results);

        // control points
        char buf[128];
        sprintf_s(buf, sizeof(buf), "0##%s", label);

        for (int i = 0; i < 2; ++i) {
            ImVec2 pos(P[i * 2], 1 - P[i * 2 + 1]);
            pos.x = pos.x * canvas.x + bb.Min.x;
            pos.y = pos.y * canvas.y + bb.Min.y;
            SetCursorScreenPos(pos - ImVec2(GRAB_RADIUS, GRAB_RADIUS));
            InvisibleButton((buf[0]++, buf), ImVec2(2 * GRAB_RADIUS, 2 * GRAB_RADIUS));
            if (IsItemActive() || IsItemHovered()) SetTooltip("(%.3f, %.3f)", P[i * 2], P[i * 2 + 1]);
            if (IsItemActive() && IsMouseDragging(0)) {
                P[i * 2] += io.MouseDelta.x / canvas.x;
                P[i * 2 + 1] -= io.MouseDelta.y / canvas.y;
                changed = true;
            }
        }

        // draw curve
        ImColor curve_col(style.Colors[ImGuiCol_PlotLines]);
        for (int i = 0; i < SMOOTHNESS; ++i) {
            ImVec2 p = results[i], q = results[i + 1];
            ImVec2 r(p.x * canvas.x + bb.Min.x, (1 - p.y) * canvas.y + bb.Min.y);
            ImVec2 s(q.x * canvas.x + bb.Min.x, (1 - q.y) * canvas.y + bb.Min.y);
            draw_list->AddLine(r, s, curve_col, CURVE_WIDTH);
        }

        // lines and grabbers
        ImVec4 pink(1.f, 0.f, 0.75f, 1.f), cyan(0.f, 0.75f, 1.f, 1.f), white(style.Colors[ImGuiCol_Text]);
        ImVec2 p1(P[0], 1 - P[1]); p1.x = p1.x * canvas.x + bb.Min.x; p1.y = p1.y * canvas.y + bb.Min.y;
        ImVec2 p2(P[2], 1 - P[3]); p2.x = p2.x * canvas.x + bb.Min.x; p2.y = p2.y * canvas.y + bb.Min.y;
        draw_list->AddLine(ImVec2(bb.Min.x, bb.Max.y), p1, ImColor(white), LINE_WIDTH);
        draw_list->AddLine(ImVec2(bb.Max.x, bb.Min.y), p2, ImColor(white), LINE_WIDTH);
        draw_list->AddCircleFilled(p1, GRAB_RADIUS, ImColor(white));
        draw_list->AddCircleFilled(p1, GRAB_RADIUS - GRAB_BORDER, ImColor(pink));
        draw_list->AddCircleFilled(p2, GRAB_RADIUS, ImColor(white));
        draw_list->AddCircleFilled(p2, GRAB_RADIUS - GRAB_BORDER, ImColor(cyan));

        SetCursorScreenPos(ImVec2(bb.Min.x, bb.Max.y + GRAB_RADIUS));

        return changed;
    }
}

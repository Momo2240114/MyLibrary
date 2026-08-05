#pragma once

#include "imgui.h"

namespace ImGuiColorEditor
{
    inline ImGuiColorEditFlags GetRuntimeColorEditFlags()
    {
        // Project内の色編集UIで表示形式を統一するための共通フラグ。
        return
            ImGuiColorEditFlags_DisplayRGB |
            ImGuiColorEditFlags_InputRGB |
            ImGuiColorEditFlags_PickerHueBar |
            ImGuiColorEditFlags_AlphaBar |
            ImGuiColorEditFlags_AlphaPreviewHalf;
    }

    inline bool DrawColorEdit4(const char* label_, float* color_)
    {
        // Alpha付きRGBAを編集する共通ラッパー。
        return ImGui::ColorEdit4(label_, color_, GetRuntimeColorEditFlags());
    }

    inline bool DrawColorEdit3(const char* label_, float* color_)
    {
        // RGBだけを編集する共通ラッパー。
        return ImGui::ColorEdit3(label_, color_, GetRuntimeColorEditFlags());
    }
}

#pragma once

#include "imgui.h"

namespace ImGuiColorEditor
{
    inline ImGuiColorEditFlags GetRuntimeColorEditFlags()
    {
        return
            ImGuiColorEditFlags_DisplayRGB |
            ImGuiColorEditFlags_InputRGB |
            ImGuiColorEditFlags_PickerHueBar |
            ImGuiColorEditFlags_AlphaBar |
            ImGuiColorEditFlags_AlphaPreviewHalf;
    }

    inline bool DrawColorEdit4(const char* label_, float* color_)
    {
        return ImGui::ColorEdit4(label_, color_, GetRuntimeColorEditFlags());
    }

    inline bool DrawColorEdit3(const char* label_, float* color_)
    {
        return ImGui::ColorEdit3(label_, color_, GetRuntimeColorEditFlags());
    }
}

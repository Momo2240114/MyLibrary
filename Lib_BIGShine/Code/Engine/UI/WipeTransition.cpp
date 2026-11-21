#include "PrecompiledHeader.h"
#include "WipeTransition.h"
#include <imgui.h>

namespace {
const bool REGISTERED = TransitionEffectFactory::Register("WIPE", []()
    {
        return std::make_unique<WipeTransition>();
    });
}

WipeTransition::WipeTransition() : ShaderTransitionEffect("Data/Shader/WipeTransitionPS.cso")
{
}

// 固有パラメータだけを編集し、共通の時間と色はComponent側で管理する。
void WipeTransition::Gui(TransitionParameters& parameters_)
{
    DrawEdges(parameters_);
    DrawDirection(parameters_);
    ImGui::DragFloat("Boundary Curve", &parameters_.boundaryCurve,0.01f,-0.3f,0.3f);
}

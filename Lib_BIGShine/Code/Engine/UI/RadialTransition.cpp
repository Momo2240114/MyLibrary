#include "PrecompiledHeader.h"
#include "RadialTransition.h"
#include <imgui.h>
namespace {
const bool REGISTERED = TransitionEffectFactory::Register("RADIAL", []() { return std::make_unique<RadialTransition>(); });
}
RadialTransition::RadialTransition() : ShaderTransitionEffect("Data/Shader/RadialTransitionPS.cso") {}
// 固有パラメータだけを編集し、共通の時間と色はComponent側で管理する。
void RadialTransition::Gui(TransitionParameters& parameters_)
{
    DrawEdges(parameters_);
    ImGui::SliderFloat2("Center Position", &parameters_.centerPosition.x,0,1);
}

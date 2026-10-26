#include "PrecompiledHeader.h"
#include "DiamondTransition.h"
#include <imgui.h>
#include "Engine/Debug/TracyProfiler.h"
namespace { const bool REGISTERED = TransitionEffectFactory::Register("DIAMOND",[]() { return std::make_unique<DiamondTransition>(); }); }
DiamondTransition::DiamondTransition() : ShaderTransitionEffect("Data/Shader/DiamondTransitionPS.cso") {}
// 固有の形状設定を共通Inspectorへ公開する。
void DiamondTransition::Gui(TransitionParameters& parameters_)
{
    BS_PROFILE_SCOPE("DiamondTransition::Gui");
    DrawEdges(parameters_);
    DrawDirection(parameters_);
    ImGui::SliderFloat2("Center Position",&parameters_.centerPosition.x,0,1);
    DrawShapeMask(parameters_);
}

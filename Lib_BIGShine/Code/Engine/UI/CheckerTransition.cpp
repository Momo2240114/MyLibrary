#include "PrecompiledHeader.h"
#include "CheckerTransition.h"
#include <imgui.h>
#include "Engine/Debug/TracyProfiler.h"
namespace { const bool REGISTERED = TransitionEffectFactory::Register("CHECKER",[]() { return std::make_unique<CheckerTransition>(); }); }
CheckerTransition::CheckerTransition() : ShaderTransitionEffect("Data/Shader/CheckerTransitionPS.cso") {}
// 固有の形状設定を共通Inspectorへ公開する。
void CheckerTransition::Gui(TransitionParameters& parameters_)
{
    BS_PROFILE_SCOPE("CheckerTransition::Gui");
    DrawEdges(parameters_);
    ImGui::SliderInt("Tile Columns",&parameters_.panelCount,2,32);
    ImGui::DragFloat("Tile Delay",&parameters_.panelDelay,0.01f,0,0.5f);
    DrawShapeMask(parameters_);
}

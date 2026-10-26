#include "PrecompiledHeader.h"
#include "SpiralTransition.h"
#include <imgui.h>
#include "Engine/Debug/TracyProfiler.h"
namespace { const bool REGISTERED = TransitionEffectFactory::Register("SPIRAL",[]() { return std::make_unique<SpiralTransition>(); }); }
SpiralTransition::SpiralTransition() : ShaderTransitionEffect("Data/Shader/SpiralTransitionPS.cso") {}
// 固有の形状設定を共通Inspectorへ公開する。
void SpiralTransition::Gui(TransitionParameters& parameters_)
{
    BS_PROFILE_SCOPE("SpiralTransition::Gui");
    DrawEdges(parameters_);
    ImGui::DragFloat("Spiral Turns",&parameters_.noiseScale,0.1f,0.1f,30);
    ImGui::SliderFloat2("Center Position",&parameters_.centerPosition.x,0,1);
}

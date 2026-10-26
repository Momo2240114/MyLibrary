#include "PrecompiledHeader.h"
#include "ShutterTransition.h"
#include <imgui.h>
namespace {
const bool REGISTERED = TransitionEffectFactory::Register("SHUTTER", []() { return std::make_unique<ShutterTransition>(); });
}
ShutterTransition::ShutterTransition() : ShaderTransitionEffect("Data/Shader/ShutterTransitionPS.cso") {}

// 固有パラメータだけを編集し、共通の時間と色はComponent側で管理する。
void ShutterTransition::Gui(TransitionParameters& parameters_)
{
    DrawEdges(parameters_);
    ImGui::SliderInt("Panel Count", &parameters_.panelCount,2,32);
    ImGui::DragFloat("Panel Delay", &parameters_.panelDelay,0.005f,0,0.5f);
    DrawTexture(parameters_,"Texture","Panel Texture"); DrawMaskUV(parameters_);
}

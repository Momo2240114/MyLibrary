#include "PrecompiledHeader.h"
#include "DissolveTransition.h"
#include <imgui.h>

namespace {
const bool REGISTERED = TransitionEffectFactory::Register("DISSOLVE", []()
    {
        return std::make_unique<DissolveTransition>();
    });
}

DissolveTransition::DissolveTransition() : ShaderTransitionEffect("Data/Shader/DissolveTransitionPS.cso")
{
}

// 固有パラメータだけを編集し、共通の時間と色はComponent側で管理する。
void DissolveTransition::Gui(TransitionParameters& parameters_)
{
    DrawEdges(parameters_);
    DrawDirection(parameters_);
    DrawTexture(parameters_,"NoiseTexture","Noise Texture");
    ImGui::DragFloat("Noise Scale", &parameters_.noiseScale,0.1f,0.1f,50);
    ImGui::DragFloat("Noise Speed", &parameters_.noiseSpeed,0.01f,-5,5);
}

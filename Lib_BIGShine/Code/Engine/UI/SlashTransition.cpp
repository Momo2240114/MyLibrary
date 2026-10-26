#include "PrecompiledHeader.h"
#include "SlashTransition.h"
#include <imgui.h>
namespace {
const bool REGISTERED = TransitionEffectFactory::Register("SLASH", []() { return std::make_unique<SlashTransition>(); });
}
SlashTransition::SlashTransition() : ShaderTransitionEffect("Data/Shader/SlashTransitionPS.cso") {}
// 固有パラメータだけを編集し、共通の時間と色はComponent側で管理する。
void SlashTransition::Gui(TransitionParameters& parameters_)
{
    DrawEdges(parameters_); DrawDirection(parameters_);
    DrawTexture(parameters_,"Texture","Slash Texture");
    DrawTexture(parameters_,"GlowTexture","Glow Texture");
    DrawTexture(parameters_,"NoiseTexture","Noise Texture");
}

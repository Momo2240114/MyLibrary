#include "PrecompiledHeader.h"
#include "FadeTransition.h"
#include <imgui.h>
namespace {
const bool REGISTERED = TransitionEffectFactory::Register("FADE", []() { return std::make_unique<FadeTransition>(); });
}
FadeTransition::FadeTransition() : ShaderTransitionEffect("Data/Shader/FadeTransitionPS.cso") {}
// 固有パラメータだけを編集し、共通の時間と色はComponent側で管理する。
void FadeTransition::Gui(TransitionParameters& parameters_)
{
    static_cast<void>(parameters_);

}

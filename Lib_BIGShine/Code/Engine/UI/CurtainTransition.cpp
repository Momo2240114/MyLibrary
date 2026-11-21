#include "PrecompiledHeader.h"
#include "CurtainTransition.h"
#include <imgui.h>
#include "Engine/Debug/TracyProfiler.h"

namespace
{
    const bool REGISTERED = TransitionEffectFactory::Register("CURTAIN",[]()
    {
        return std::make_unique<CurtainTransition>();
    });
}

CurtainTransition::CurtainTransition() : ShaderTransitionEffect("Data/Shader/CurtainTransitionPS.cso")
{
}

// 固有の形状設定を共通Inspectorへ公開する。
void CurtainTransition::Gui(TransitionParameters& parameters_)
{
    BS_PROFILE_SCOPE("CurtainTransition::Gui");
    DrawEdges(parameters_);
    DrawDirection(parameters_);
    ImGui::SliderFloat2("Center Position",&parameters_.centerPosition.x,0,1);
}

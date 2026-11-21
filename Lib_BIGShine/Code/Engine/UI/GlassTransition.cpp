#include "PrecompiledHeader.h"
#include "GlassTransition.h"
#include "UIPresentationPresets.h"
#include <imgui.h>
#include "Engine/Debug/TracyProfiler.h"

namespace
{
    const bool REGISTERED = TransitionEffectFactory::Register("GLASS",[]()
    {
        return std::make_unique<GlassTransition>();
    });
}

GlassTransition::GlassTransition() : ShaderTransitionEffect("Data/Shader/GlassTransitionPS.cso")
{
}

// 破片数の上限を固定し、描画する頂点数の増加を抑える。
// 固有の形状設定を共通Inspectorへ公開する。
void GlassTransition::Gui(TransitionParameters& parameters_)
{
    BS_PROFILE_SCOPE("GlassTransition::Gui");
    DrawEdges(parameters_);
    ImGui::SliderInt("Shard Columns",&parameters_.panelCount,2,6);
    ImGui::DragFloat("Fall / Spread",&parameters_.noiseSpeed,.01f,.1f,3);
    float rotation_ = UIPresentationPresets::Number(parameters_.effectSettings,"ShardRotation",.8f,0,2);
    if (ImGui::DragFloat("Shard Rotation",&rotation_,.01f,0,2))
    {
        parameters_.effectSettings["ShardRotation"] = rotation_;
    }
}

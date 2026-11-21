#pragma once
#include "ShaderTransitionEffect.h"

// 現在の画面を三角形の破片へ分割して落下させるスクリーン演出。
class GlassTransition : public ShaderTransitionEffect
{
public:
    GlassTransition();
    void Gui(TransitionParameters& parameters_) override;
};

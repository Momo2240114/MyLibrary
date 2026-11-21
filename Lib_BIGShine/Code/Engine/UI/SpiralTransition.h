#pragma once
#include "ShaderTransitionEffect.h"

// 渦の境界で画面を覆うトランジション。
class SpiralTransition final : public ShaderTransitionEffect
{
public:
    SpiralTransition();
    // 固有の境界・方向・展開速度をInspectorから編集する。
    void Gui(TransitionParameters& parameters_) override;
};

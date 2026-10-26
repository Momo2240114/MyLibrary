#pragma once
#include "ShaderTransitionEffect.h"
// 菱形の開口部を閉じるトランジション。
class DiamondTransition final : public ShaderTransitionEffect
{
public:
    DiamondTransition();
    // 固有の境界・方向・展開速度をInspectorから編集する。
    void Gui(TransitionParameters& parameters_) override;
};

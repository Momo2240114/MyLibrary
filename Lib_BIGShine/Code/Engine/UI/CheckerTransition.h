#pragma once
#include "ShaderTransitionEffect.h"

// タイルを時間差で覆うトランジション。
class CheckerTransition final : public ShaderTransitionEffect
{
public:
    CheckerTransition();
    // 固有の境界・方向・展開速度をInspectorから編集する。
    void Gui(TransitionParameters& parameters_) override;
};

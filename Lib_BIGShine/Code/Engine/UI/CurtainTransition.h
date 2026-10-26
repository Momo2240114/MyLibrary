#pragma once
#include "ShaderTransitionEffect.h"
// 両側から幕を閉じるトランジション。
class CurtainTransition final : public ShaderTransitionEffect
{
public:
    CurtainTransition();
    // 固有の境界・方向・展開速度をInspectorから編集する。
    void Gui(TransitionParameters& parameters_) override;
};

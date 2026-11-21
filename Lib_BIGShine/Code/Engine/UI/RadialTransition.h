#pragma once
#include "ShaderTransitionEffect.h"

// Radial専用のShaderと設定UIを持つ画面演出。
class RadialTransition final : public ShaderTransitionEffect
{
public:
    RadialTransition();
    void Gui(TransitionParameters& parameters_) override;
};

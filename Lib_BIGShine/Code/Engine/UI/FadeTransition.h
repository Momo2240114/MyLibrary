#pragma once
#include "ShaderTransitionEffect.h"

// Fade専用のShaderと設定UIを持つ画面演出。
class FadeTransition final : public ShaderTransitionEffect
{
public:
    FadeTransition();
    void Gui(TransitionParameters& parameters_) override;
};

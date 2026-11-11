#pragma once
#include "ShaderTransitionEffect.h"
// Slash専用のShaderと設定UIを持つ画面演出。
class SlashTransition final : public ShaderTransitionEffect
{
public:
    SlashTransition();
    void Gui(TransitionParameters& parameters_) override;
};

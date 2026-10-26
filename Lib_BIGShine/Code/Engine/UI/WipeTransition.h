#pragma once
#include "ShaderTransitionEffect.h"
// Wipe専用のShaderと設定UIを持つ画面演出。
class WipeTransition final : public ShaderTransitionEffect
{
public:
    WipeTransition();
    void Gui(TransitionParameters& parameters_) override;
};

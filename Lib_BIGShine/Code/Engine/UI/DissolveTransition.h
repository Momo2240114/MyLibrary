#pragma once
#include "ShaderTransitionEffect.h"

// Dissolve専用のShaderと設定UIを持つ画面演出。
class DissolveTransition final : public ShaderTransitionEffect
{
public:
    DissolveTransition();
    void Gui(TransitionParameters& parameters_) override;
};

#pragma once
#include "ShaderTransitionEffect.h"

// Shutter専用のShaderと設定UIを持つ画面演出。
class ShutterTransition final : public ShaderTransitionEffect
{
public:
    ShutterTransition();
    void Gui(TransitionParameters& parameters_) override;
};

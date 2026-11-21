#pragma once
#include "ShaderTransitionEffect.h"

// TextureMask専用のShaderと設定UIを持つ画面演出。
class TextureMaskTransition final : public ShaderTransitionEffect
{
public:
    TextureMaskTransition();
    void Gui(TransitionParameters& parameters_) override;
};

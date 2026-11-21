#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include <wrl.h>

//=========================================
// SpriteShader (スプライト用シェーダー)
//=========================================
class SpriteShader : public Component
{
private:
    static inline const std::string componentName = "SpriteShader";

public:
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;

    const std::string& GetName() const override
    {
        return componentName;
    }

    SpriteShader() = default;

    SpriteShader(Microsoft::WRL::ComPtr<ID3D11VertexShader> vs_,
        Microsoft::WRL::ComPtr<ID3D11PixelShader> ps_,
        Microsoft::WRL::ComPtr<ID3D11InputLayout> layout_)
        : vertexShader(vs_), pixelShader(ps_), inputLayout(layout_) {
    }
};
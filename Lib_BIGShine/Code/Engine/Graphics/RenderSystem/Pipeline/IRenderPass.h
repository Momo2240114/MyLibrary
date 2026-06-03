#pragma once

#include "Engine/Graphics/Core/RenderContext.h"

// IRenderPass.h
class IRenderPass {
public:
    virtual ~IRenderPass() = default;
    virtual void Initialize(ID3D11Device* device, float width, float height) = 0;

    // Execute内でリソースを加工し、次のパスへと繋ぐ
    virtual void Execute(const RenderContext& rc,
        ID3D11ShaderResourceView* inputSRV,
        ID3D11RenderTargetView* outputRTV) = 0;
};
#pragma once

#include "MeshVFXShader.h"

#include <memory>
#include <vector>

class MeshVFXComponent;
struct RenderContext;

class MeshVFXRenderSystem
{
public:
    bool Initialize(ID3D11Device* device_);
    void PushSubmit(MeshVFXComponent* component_);
    void RenderAll(const RenderContext& renderContext_);

private:
    std::unique_ptr<MeshVFXShader> shader;
    std::vector<MeshVFXComponent*> components;
};

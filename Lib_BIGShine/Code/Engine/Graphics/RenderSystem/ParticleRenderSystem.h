#pragma once

#include <d3d11.h>
#include <wrl.h>
#include <vector>
#include <memory>
#include <DirectXMath.h>
#include "Engine/Graphics/Particle/ParticleComponent.h"

struct RenderContext;

class ParticleRenderSystem
{
public:
    void Initialize(ID3D11Device* device);
    void PushSubmit(ParticleComponent* component);
    void UpdateAll(ID3D11DeviceContext* context,
        float et,
        const DirectX::XMMATRIX& vpMatrix,
        const std::vector<std::weak_ptr<ParticleComponent>>& components);
    void ApplySceneConstants(const RenderContext& rc);
    void RenderAll(const RenderContext& rc);

private:
    struct DrawInfo
    {
        ParticleComponent* component = nullptr;
    };

    struct CbScene
    {
        DirectX::XMFLOAT4X4 viewProjection;
        DirectX::XMFLOAT4 lightDirection;
        DirectX::XMFLOAT4 lightColor;
        DirectX::XMFLOAT4 cameraPosition;
        DirectX::XMFLOAT4 zBufferParameteres;
        int numPointLights;
        int numSpotLights;
        float padding[2];
        DirectX::XMFLOAT4X4 invViewProjection;
    };

private:
    std::vector<DrawInfo> drawInfos;
    Microsoft::WRL::ComPtr<ID3D11Buffer> sceneConstantBuffer;
};

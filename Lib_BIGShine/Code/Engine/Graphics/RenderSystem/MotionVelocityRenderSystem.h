#pragma once

#include <memory>
#include <vector>

#include <d3d11.h>
#include <DirectXMath.h>
#include <wrl/client.h>

#include "Engine/Graphics/RenderSystem/MotionBlurSystem.h"

class MotionVelocityComponent;

class MotionVelocityRenderSystem : public MotionBlurSystem::ObjectVelocityRenderer
{
public:
    bool Initialize(ID3D11Device* device_);
    void Register(const std::shared_ptr<MotionVelocityComponent>& component_);
    void Unregister(MotionVelocityComponent* component_);

    void DrawObjectVelocity(
        const RenderContext& rc_,
        ID3D11RenderTargetView* velocityRTV_,
        int width_,
        int height_,
        const DirectX::XMFLOAT4X4& currentViewProjection_,
        const DirectX::XMFLOAT4X4& previousViewProjection_) override;
    void CommitHistory() override;

private:
    struct CbObjectMotionVelocity
    {
        DirectX::XMFLOAT4X4 currentViewProjection;
        DirectX::XMFLOAT4X4 previousViewProjection;
        DirectX::XMFLOAT4X4 currentOwnerWorld;
        DirectX::XMFLOAT4X4 previousOwnerWorld;
        DirectX::XMFLOAT4X4 inverseCurrentOwnerWorld;
        float velocityScale = 1.0f;
        DirectX::XMFLOAT3 padding{};
    };

    struct CbSkeleton
    {
        DirectX::XMFLOAT4X4 boneTransforms[256];
    };

    void RemoveExpiredComponents();
    void DrawModelVelocity(
        const RenderContext& rc_,
        const std::shared_ptr<MotionVelocityComponent>& component_,
        const DirectX::XMFLOAT4X4& currentViewProjection_,
        const DirectX::XMFLOAT4X4& previousViewProjection_);

    bool initialized = false;
    std::vector<std::weak_ptr<MotionVelocityComponent>> components;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
    Microsoft::WRL::ComPtr<ID3D11Buffer> objectConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> skeletonConstantBuffer;
};

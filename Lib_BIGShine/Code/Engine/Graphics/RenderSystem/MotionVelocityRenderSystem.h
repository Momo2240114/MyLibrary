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
    // Object Velocity用ShaderとConstantBufferを作成する。
    bool Initialize(ID3D11Device* device_);
    // MotionBlurへ速度を書きたいComponentを登録する。
    void Register(const std::shared_ptr<MotionVelocityComponent>& component_);
    // Component破棄や無効化時に登録を外す。
    void Unregister(MotionVelocityComponent* component_);

    // 登録Componentの前Frame/現Frame行列差分からVelocity RTへ速度を書き込む。
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
        // CameraとOwnerの現在/前Frame行列をShaderへ渡し、Screen上の移動量を計算する。
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
        // Skinning Mesh用のBone行列。Shader側の最大数に合わせて固定配列にする。
        DirectX::XMFLOAT4X4 boneTransforms[256];
    };

    // weak_ptr登録のうち、既に破棄されたComponentを取り除く。
    void RemoveExpiredComponents();
    // 1つのModelについてVelocity描画を行う。
    void DrawModelVelocity(
        const RenderContext& rc_,
        const std::shared_ptr<MotionVelocityComponent>& component_,
        const DirectX::XMFLOAT4X4& currentViewProjection_,
        const DirectX::XMFLOAT4X4& previousViewProjection_);

    // Shader/Buffer作成が成功したかどうか。
    bool initialized = false;
    // Velocity描画対象Component一覧。所有権はGameObject側に残す。
    std::vector<std::weak_ptr<MotionVelocityComponent>> components;
    // Object Velocity描画用GPU Resource。
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
    Microsoft::WRL::ComPtr<ID3D11Buffer> objectConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> skeletonConstantBuffer;
};

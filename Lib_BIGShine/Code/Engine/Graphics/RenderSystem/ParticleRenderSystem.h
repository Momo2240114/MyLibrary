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
    // Particle共通ShaderとScene ConstantBufferを初期化する。
    void Initialize(ID3D11Device* device);
    // 1Frame分の描画対象ParticleComponentを追加する。
    void PushSubmit(ParticleComponent* component);
    // 登録済みComponent一覧から有効なものを収集し、描画Queueを作り直す。
    void UpdateAll(ID3D11DeviceContext* context,
        float et,
        const DirectX::XMMATRIX& vpMatrix,
        const std::vector<std::weak_ptr<ParticleComponent>>& components);
    // CameraやLightなど、全Particleで共有するScene情報をShaderへ送る。
    void ApplySceneConstants(const RenderContext& rc);
    // Queueに積まれたParticle Trackをまとめて描画する。
    void RenderAll(const RenderContext& rc,bool isEditorWindow_ = false);

private:
    struct DrawInfo
    {
        // 描画対象Component。QueueはFrame末尾でClearされるため生ポインタで保持する。
        ParticleComponent* component = nullptr;
    };

    struct CbScene
    {
        // Particle Shaderで共有するCamera/Light/Depth復元情報。
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
    // このFrameで描画するParticleComponent一覧。
    std::vector<DrawInfo> drawInfos;
    // Scene単位のConstantBuffer。
    Microsoft::WRL::ComPtr<ID3D11Buffer> sceneConstantBuffer;
};

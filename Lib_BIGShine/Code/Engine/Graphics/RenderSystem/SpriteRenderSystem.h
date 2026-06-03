#pragma once

#include "Engine/Graphics/Resource/Sprite.h"
#include "Engine/Graphics/Shader/SpriteShader.h"


#include <vector>
#include <map>
#include <memory>
#include <d3d11.h>
#include "Engine/Graphics/Shader/SpriteShaderID.h"


class SpriteRenderSystem
{
public:
    struct DrawInfo
    {
        std::shared_ptr<SpriteRenderer> sprite; // SpriteRenderer 自身を保持
        SpriteShaderID shaderID;
    };

    struct ConstantBuffer
    {
        DirectX::XMMATRIX world;  // ワールド行列
        DirectX::XMFLOAT4 color;  // スプライト色
    };
public:
    void Finalize();

    void Initialize(ID3D11Device* device_, ID3D11DeviceContext* context_);

    void Submit(const std::shared_ptr<SpriteRenderer>& sprite_, SpriteShaderID shaderID_ = SpriteShaderID::Default)
    {
        drawList.push_back({ sprite_, shaderID_ });
    }
    void RenderAll(ID3D11DeviceContext* context_);
private:
    void CreateShader(ID3D11Device* device_, SpriteShaderID id_, const char* vsFile_, const char* psFile_,
        D3D11_INPUT_ELEMENT_DESC* inputElementDesc_,
        UINT numElements_);
private:
    // 描画用のスプライト頂点バッファとインデックスバッファを事前に作成しておく
    struct SpriteVertex
    {
        DirectX::XMFLOAT3 pos;
        DirectX::XMFLOAT4 color;
        DirectX::XMFLOAT2 texcoord;
    };

    std::vector<DrawInfo> drawList;
    std::map<SpriteShaderID, SpriteShader> shaders;
    Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerState;
    Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;
};

#pragma once

#include "Engine/Graphics/Resource/Sprite.h"
#include "Engine/Graphics/Shader/SpriteShader.h"
#include "Engine/Graphics/Shader/SpriteShaderID.h"

#include <array>
#include <cstdint>
#include <d3d11.h>
#include <map>
#include <memory>
#include <vector>

class UIWidgetComponent;
class UIVertexModifierComponent;
class WidgetBoardComponent;

class SpriteRenderSystem
{
public:
    struct SpriteRenderCommand
    {
        std::shared_ptr<SpriteRenderer> sprite;
        UIWidgetComponent* widget = nullptr;
        UIVertexModifierComponent* vertexModifier = nullptr;
        WidgetBoardComponent* widgetBoard = nullptr;
        SpriteShaderID shaderID = SpriteShaderID::Default;
        int boardLayer = 0;
        int boardZOrder = 0;
        int widgetZOrder = 0;
        int hierarchyOrder = 0;
        uint64_t submitOrder = 0;
    };

    struct ConstantBuffer
    {
        DirectX::XMMATRIX world;
        DirectX::XMFLOAT4 color;
    };

    void Finalize();
    void Initialize(ID3D11Device* device_, ID3D11DeviceContext* context_);

    void Submit(const std::shared_ptr<SpriteRenderer>& sprite_, SpriteShaderID shaderID_ = SpriteShaderID::Default)
    {
        SpriteRenderCommand command_;
        command_.sprite = sprite_;
        command_.shaderID = shaderID_;
        command_.submitOrder = nextSubmitOrder++;
        drawList.push_back(command_);
    }
    void Submit(const SpriteRenderCommand& command_);
    void RenderAll(ID3D11DeviceContext* context_);

private:
    struct SpriteVertex
    {
        DirectX::XMFLOAT3 pos;
        DirectX::XMFLOAT4 color;
        DirectX::XMFLOAT2 texcoord;
    };

    void SortDrawList();
    void BuildSpriteVertices(
        SpriteRenderCommand& renderCommand_,
        const DirectX::XMFLOAT2& screenSize_,
        std::array<SpriteVertex, 4>& vertices_);
    void BuildLegacySpriteVertices(
        const SpriteRenderCommand& renderCommand_,
        const DirectX::XMFLOAT2& screenSize_,
        std::array<SpriteVertex, 4>& vertices_);
    void ApplyWidgetLayout(
        SpriteRenderCommand& renderCommand_,
        const DirectX::XMFLOAT2& screenSize_,
        std::array<SpriteVertex, 4>& vertices_);
    void CalculateWidgetLayoutRecursive(
        UIWidgetComponent* widget_,
        WidgetBoardComponent* board_,
        const DirectX::XMFLOAT2& screenSize_,
        int depth_);
    void ApplyVertexModification(
        const SpriteRenderCommand& renderCommand_,
        const DirectX::XMFLOAT2& screenSize_,
        std::array<SpriteVertex, 4>& vertices_);
    void RenderSprite(
        SpriteRenderCommand& renderCommand_,
        ID3D11DeviceContext* context_,
        const DirectX::XMFLOAT2& screenSize_);
    void CreateShader(
        ID3D11Device* device_,
        SpriteShaderID id_,
        const char* vsFile_,
        const char* psFile_,
        D3D11_INPUT_ELEMENT_DESC* inputElementDesc_,
        UINT numElements_);

    std::vector<SpriteRenderCommand> drawList;
    uint64_t nextSubmitOrder = 0;
    std::map<SpriteShaderID, SpriteShader> shaders;
    Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerState;
    Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;
};

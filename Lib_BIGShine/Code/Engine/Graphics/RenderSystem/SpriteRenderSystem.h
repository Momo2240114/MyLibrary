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
        // 描画するSprite本体。
        std::shared_ptr<SpriteRenderer> sprite;

        // UI描画時に、Widgetレイアウトや頂点変形を反映するための参照。
        UIWidgetComponent* widget = nullptr;
        UIVertexModifierComponent* vertexModifier = nullptr;
        WidgetBoardComponent* widgetBoard = nullptr;
        SpriteShaderID shaderID = SpriteShaderID::Default;

        // WidgetBoard / Widget階層 / Submit順を組み合わせて安定した描画順にする。
        int boardLayer = 0;
        int boardZOrder = 0;
        int widgetZOrder = 0;
        int hierarchyOrder = 0;
        uint64_t submitOrder = 0;
    };

    struct ConstantBuffer
    {
        // SpriteShaderへ渡すTransformと色。
        DirectX::XMMATRIX world;
        DirectX::XMFLOAT4 color;
    };

    void Finalize();

    // SpriteShader、頂点/Index Buffer、Samplerを初期化する。
    void Initialize(ID3D11Device* device_, ID3D11DeviceContext* context_);

    // 旧式Sprite描画の簡易Submit。UI情報を持たないSpriteに使う。
    void Submit(const std::shared_ptr<SpriteRenderer>& sprite_, SpriteShaderID shaderID_ = SpriteShaderID::Default)
    {
        SpriteRenderCommand command_;
        command_.sprite = sprite_;
        command_.shaderID = shaderID_;
        command_.submitOrder = nextSubmitOrder++;
        drawList.push_back(command_);
    }

    // UIWidget情報込みの描画Commandを登録する。
    void Submit(const SpriteRenderCommand& command_);

    // 今フレーム登録されたSpriteを並べ替えて描画し、drawListを空にする。
    void RenderAll(ID3D11DeviceContext* context_);

private:
    struct SpriteVertex
    {
        // 画面空間に変換済みの頂点情報。
        DirectX::XMFLOAT3 pos;
        DirectX::XMFLOAT4 color;
        DirectX::XMFLOAT2 texcoord;
    };

    // HLSLのb1と16バイト単位で対応し、画像と文字に同じ合成効果を渡す。
    struct UIEffectConstants
    {
        DirectX::XMFLOAT4 glowColor, outlineColor, shadowColor, sweepColor, gradientTop, gradientBottom;
        DirectX::XMFLOAT4 tone, style, animation, sweep, sampling, shadow, uvRect, detail, burnColor;
        DirectX::XMFLOAT4 maskUV, maskOptions;
    };
    Microsoft::WRL::ComPtr<ID3D11PixelShader> uiEffectShader;
    Microsoft::WRL::ComPtr<ID3D11Buffer> uiEffectBuffer;
    // UI階層やSubmit順に基づいて描画順を確定する。
    void SortDrawList();

    // Widget付き/無しを判定して、最終的な4頂点を作る。
    void BuildSpriteVertices(
        SpriteRenderCommand& renderCommand_,
        const DirectX::XMFLOAT2& screenSize_,
        std::array<SpriteVertex, 4>& vertices_);
    void BuildLegacySpriteVertices(
        const SpriteRenderCommand& renderCommand_,
        const DirectX::XMFLOAT2& screenSize_,
            std::array<SpriteVertex, 4>& vertices_);

    // UIWidgetのAnchor/Size/Pivotを反映して頂点位置を作る。
    void ApplyWidgetLayout(
        SpriteRenderCommand& renderCommand_,
        const DirectX::XMFLOAT2& screenSize_,
        std::array<SpriteVertex, 4>& vertices_);
    void CalculateWidgetLayoutRecursive(
        UIWidgetComponent* widget_,
        WidgetBoardComponent* board_,
            const DirectX::XMFLOAT2& screenSize_,
            int depth_);

    // UIVertexModifierComponentの頂点別位置・色補正を反映する。
    void ApplyVertexModification(
        const SpriteRenderCommand& renderCommand_,
        const DirectX::XMFLOAT2& screenSize_,
        std::array<SpriteVertex, 4>& vertices_);
    void RenderSprite(
        SpriteRenderCommand& renderCommand_,
        ID3D11DeviceContext* context_,
            const DirectX::XMFLOAT2& screenSize_);

    // SpriteShaderIDごとのVS/PS/InputLayoutを作成する。
    void CreateShader(
        ID3D11Device* device_,
        SpriteShaderID id_,
        const char* vsFile_,
        const char* psFile_,
        D3D11_INPUT_ELEMENT_DESC* inputElementDesc_,
        UINT numElements_);

    // 今フレーム描画するSprite一覧。
    std::vector<SpriteRenderCommand> drawList;
    uint64_t nextSubmitOrder = 0;
    std::map<SpriteShaderID, SpriteShader> shaders;
    Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerState;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> linearSamplerState;
    Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;
};

#pragma once

#include <DirectXMath.h>
#include <d3d11.h>
#include <memory>
#include <vector>
#include <wrl.h>

class EditorPrimitiveRenderer
{
public:
    // エディタ専用の線描画Renderer。グリッドやプレビュー補助線を一時頂点として描く。
    bool Initialize(ID3D11Device* device_);

    void AddLine(
        const DirectX::XMFLOAT3& start_,
        const DirectX::XMFLOAT3& end_,
        const DirectX::XMFLOAT4& color_);

    void DrawGrid(
        int subdivisions_,
        float cellSize_,
        const DirectX::XMFLOAT4& gridColor_,
        bool drawAxis_);

    void DrawAxis(float length_);

    void Render(
        ID3D11DeviceContext* context_,
        const DirectX::XMFLOAT4X4& view_,
        const DirectX::XMFLOAT4X4& projection_);

    void Clear();

private:
    // 1回のMapで送る最大頂点数。超えた分はRender内で分割描画される。
    static constexpr UINT VERTEX_CAPACITY = 6 * 1024;

    struct Vertex
    {
        // ワールド座標の線頂点。シェーダではViewProjectionだけを掛ける。
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT4 color;
    };

    struct CbScene
    {
        DirectX::XMFLOAT4X4 viewProjection;
    };

private:
    std::vector<Vertex> vertices;

    // 線描画用の最小シェーダ一式と、毎フレーム更新するDynamic VertexBuffer。
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
    Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer;
};

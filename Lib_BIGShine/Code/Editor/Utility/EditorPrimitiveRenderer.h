#pragma once

#include <DirectXMath.h>
#include <d3d11.h>
#include <memory>
#include <vector>
#include <wrl.h>

class EditorPrimitiveRenderer
{
public:
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
    static constexpr UINT VERTEX_CAPACITY = 6 * 1024;

    struct Vertex
    {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT4 color;
    };

    struct CbScene
    {
        DirectX::XMFLOAT4X4 viewProjection;
    };

private:
    std::vector<Vertex> vertices;

    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
    Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer;
};
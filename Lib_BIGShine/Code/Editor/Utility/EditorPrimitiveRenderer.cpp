#include "EditorPrimitiveRenderer.h"

#include "Engine/Graphics/Core/CPUResourceLoader.h"

#include <algorithm>
#include <cassert>
#include <cstring>

bool EditorPrimitiveRenderer::Initialize(ID3D11Device* device_)
{
    if (!device_) return false;

    D3D11_INPUT_ELEMENT_DESC inputElementDesc_[]
    {
        {
            "POSITION",
            0,
            DXGI_FORMAT_R32G32B32_FLOAT,
            0,
            D3D11_APPEND_ALIGNED_ELEMENT,
            D3D11_INPUT_PER_VERTEX_DATA,
            0
        },
        {
            "COLOR",
            0,
            DXGI_FORMAT_R32G32B32A32_FLOAT,
            0,
            D3D11_APPEND_ALIGNED_ELEMENT,
            D3D11_INPUT_PER_VERTEX_DATA,
            0
        },
    };

    CPUResourceLoader::LoadVertexShader(
        device_,
        "Shader/PrimitiveRendererVS.cso",
        inputElementDesc_,
        _countof(inputElementDesc_),
        inputLayout.GetAddressOf(),
        vertexShader.GetAddressOf());

    CPUResourceLoader::LoadPixelShader(
        device_,
        "Shader/PrimitiveRendererPS.cso",
        pixelShader.GetAddressOf());

    CPUResourceLoader::CreateConstantBuffer(
        device_,
        sizeof(CbScene),
        constantBuffer.GetAddressOf());

    D3D11_BUFFER_DESC vertexBufferDesc_{};
    vertexBufferDesc_.ByteWidth = sizeof(Vertex) * VERTEX_CAPACITY;
    vertexBufferDesc_.Usage = D3D11_USAGE_DYNAMIC;
    vertexBufferDesc_.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vertexBufferDesc_.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    HRESULT hr_ = device_->CreateBuffer(
        &vertexBufferDesc_,
        nullptr,
        vertexBuffer.GetAddressOf());

    return SUCCEEDED(hr_);
}

void EditorPrimitiveRenderer::AddLine(
    const DirectX::XMFLOAT3& start_,
    const DirectX::XMFLOAT3& end_,
    const DirectX::XMFLOAT4& color_)
{
    vertices.push_back({ start_, color_ });
    vertices.push_back({ end_, color_ });
}

void EditorPrimitiveRenderer::DrawGrid(
    int subdivisions_,
    float cellSize_,
    const DirectX::XMFLOAT4& gridColor_,
    bool drawAxis_)
{
    subdivisions_ = (std::max)(1, subdivisions_);
    cellSize_ = (std::max)(0.01f, cellSize_);

    const int halfCount_ = subdivisions_ / 2;
    const float gridMin_ = -static_cast<float>(halfCount_) * cellSize_;
    const float gridMax_ = static_cast<float>(halfCount_) * cellSize_;

    for (int lineIndex_ = -halfCount_; lineIndex_ <= halfCount_; ++lineIndex_)
    {
        const float position_ = static_cast<float>(lineIndex_) * cellSize_;

        DirectX::XMFLOAT4 lineColor_ = gridColor_;

        if (drawAxis_ && lineIndex_ == 0)
        {
            continue;
        }

        AddLine(
            { position_, 0.0f, gridMin_ },
            { position_, 0.0f, gridMax_ },
            lineColor_);

        AddLine(
            { gridMin_, 0.0f, position_ },
            { gridMax_, 0.0f, position_ },
            lineColor_);
    }

    if (drawAxis_)
    {
        const DirectX::XMFLOAT4 xColor_{ 1.0f, 0.15f, 0.15f, 1.0f };
        const DirectX::XMFLOAT4 zColor_{ 0.15f, 0.35f, 1.0f, 1.0f };
        const DirectX::XMFLOAT4 yColor_{ 0.15f, 1.0f, 0.15f, 1.0f };

        AddLine(
            { gridMin_, 0.0f, 0.0f },
            { gridMax_, 0.0f, 0.0f },
            xColor_);

        AddLine(
            { 0.0f, 0.0f, gridMin_ },
            { 0.0f, 0.0f, gridMax_ },
            zColor_);

        AddLine(
            { 0.0f, 0.0f, 0.0f },
            { 0.0f, cellSize_ * 2.0f, 0.0f },
            yColor_);
    }
}

void EditorPrimitiveRenderer::DrawAxis(float length_)
{
    length_ = (std::max)(0.01f, length_);

    AddLine(
        { 0.0f, 0.0f, 0.0f },
        { length_, 0.0f, 0.0f },
        { 1.0f, 0.1f, 0.1f, 1.0f });

    AddLine(
        { 0.0f, 0.0f, 0.0f },
        { 0.0f, length_, 0.0f },
        { 0.1f, 1.0f, 0.1f, 1.0f });

    AddLine(
        { 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, length_ },
        { 0.1f, 0.3f, 1.0f, 1.0f });
}

void EditorPrimitiveRenderer::Render(
    ID3D11DeviceContext* context_,
    const DirectX::XMFLOAT4X4& view_,
    const DirectX::XMFLOAT4X4& projection_)
{
    if (!context_) return;
    if (vertices.empty()) return;
    if (!vertexShader || !pixelShader || !inputLayout || !vertexBuffer || !constantBuffer) return;

    DirectX::XMMATRIX viewMatrix_ = DirectX::XMLoadFloat4x4(&view_);
    DirectX::XMMATRIX projectionMatrix_ = DirectX::XMLoadFloat4x4(&projection_);
    DirectX::XMMATRIX viewProjectionMatrix_ = viewMatrix_ * projectionMatrix_;

    CbScene cbScene_{};
    DirectX::XMStoreFloat4x4(&cbScene_.viewProjection, viewProjectionMatrix_);

    context_->UpdateSubresource(
        constantBuffer.Get(),
        0,
        nullptr,
        &cbScene_,
        0,
        0);

    context_->VSSetShader(vertexShader.Get(), nullptr, 0);
    context_->PSSetShader(pixelShader.Get(), nullptr, 0);
    context_->IASetInputLayout(inputLayout.Get());
    context_->VSSetConstantBuffers(0, 1, constantBuffer.GetAddressOf());

    UINT stride_ = sizeof(Vertex);
    UINT offset_ = 0;

    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    context_->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
    context_->IASetVertexBuffers(
        0,
        1,
        vertexBuffer.GetAddressOf(),
        &stride_,
        &offset_);

    UINT totalVertexCount_ = static_cast<UINT>(vertices.size());
    UINT startVertex_ = 0;

    while (startVertex_ < totalVertexCount_)
    {
        const UINT copyCount_ = (std::min)(
            VERTEX_CAPACITY,
            totalVertexCount_ - startVertex_);

        D3D11_MAPPED_SUBRESOURCE mappedResource_{};
        HRESULT hr_ = context_->Map(
            vertexBuffer.Get(),
            0,
            D3D11_MAP_WRITE_DISCARD,
            0,
            &mappedResource_);

        if (FAILED(hr_)) break;

        std::memcpy(
            mappedResource_.pData,
            vertices.data() + startVertex_,
            sizeof(Vertex) * copyCount_);

        context_->Unmap(vertexBuffer.Get(), 0);
        context_->Draw(copyCount_, 0);

        startVertex_ += copyCount_;
    }

    Clear();
}

void EditorPrimitiveRenderer::Clear()
{
    vertices.clear();
}
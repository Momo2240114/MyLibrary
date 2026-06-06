#include "CascadeShadowMap.h"
#include "Engine/GamePlay/Camera/CameraComponent.h"
#include <algorithm>
#include <cfloat>
#include <cmath>

bool CascadeShadowMap::Initialize(ID3D11Device* device)
{
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = ShadowmapSize;
    desc.Height = ShadowmapSize;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R32_TYPELESS;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Texture2D.MipSlice = 0;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;

    for (int i = 0; i < CascadeCount; ++i)
    {
        HRESULT hr = device->CreateTexture2D(&desc, nullptr, shadowTextures[i].GetAddressOf());
        if (FAILED(hr)) return false;

        hr = device->CreateDepthStencilView(shadowTextures[i].Get(), &dsvDesc, shadowDSVs[i].GetAddressOf());
        if (FAILED(hr)) return false;

        hr = device->CreateShaderResourceView(shadowTextures[i].Get(), &srvDesc, shadowSRVs[i].GetAddressOf());
        if (FAILED(hr)) return false;
    }

    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(ShadowmapSize);
    viewport.Height = static_cast<float>(ShadowmapSize);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    return true;
}


void CascadeShadowMap::UpdateMatrices(const CameraComponent& camera, const DirectX::XMFLOAT3& lightDirection)
{
    using namespace DirectX;

    const float splitTable[CascadeCount + 1] =
    {
        camera.GetNear(),
        (std::min)(cascadeSplitDistances[0], camera.GetFar()),
        (std::min)(cascadeSplitDistances[1], camera.GetFar()),
        (std::min)(cascadeSplitDistances[2], camera.GetFar()),
        (std::min)(cascadeSplitDistances[3], camera.GetFar()),
    };
    cascadeSplits = { splitTable[1], splitTable[2], splitTable[3], splitTable[4] };

    XMVECTOR cameraPosition = XMLoadFloat3(&camera.GetEye());
    XMVECTOR cameraRight = XMVector3Normalize(XMLoadFloat3(&camera.GetRight()));
    XMVECTOR cameraUp = XMVector3Normalize(XMLoadFloat3(&camera.GetUp()));
    XMVECTOR cameraFront = XMVector3Normalize(XMLoadFloat3(&camera.GetFront()));

    XMVECTOR lightDir = XMLoadFloat3(&lightDirection);
    if (XMVectorGetX(XMVector3LengthSq(lightDir)) < 0.0001f)
    {
        lightDir = XMVectorSet(0.0f, -1.0f, 1.0f, 0.0f);
    }
    lightDir = XMVector3Normalize(lightDir);

    XMVECTOR lightPosition = XMVectorScale(lightDir, -250.0f);
    XMMATRIX lightView = XMMatrixLookAtLH(
        lightPosition,
        XMVectorZero(),
        XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    XMMATRIX lightProjection = XMMatrixOrthographicLH(10000.0f, 10000.0f, 0.1f, 1000.0f);
    XMMATRIX lightViewProjection = lightView * lightProjection;

    const float fovY = XMConvertToRadians(camera.GetFovY());
    const float aspect = camera.GetAspect();

    for (int index = 0; index < CascadeCount; ++index)
    {
        float nearDepth = splitTable[index];
        float farDepth = splitTable[index + 1];
        if (farDepth <= nearDepth)
        {
            farDepth = nearDepth + 0.1f;
        }

        float nearY = tanf(fovY * 0.5f) * nearDepth;
        float nearX = nearY * aspect;
        float farY = tanf(fovY * 0.5f) * farDepth;
        float farX = farY * aspect;

        XMVECTOR nearPosition = XMVectorAdd(cameraPosition, XMVectorScale(cameraFront, nearDepth));
        XMVECTOR farPosition = XMVectorAdd(cameraPosition, XMVectorScale(cameraFront, farDepth));

        XMVECTOR vertex[8];
        vertex[0] = XMVectorAdd(nearPosition, XMVectorAdd(XMVectorScale(cameraUp, nearY), XMVectorScale(cameraRight, nearX)));
        vertex[1] = XMVectorAdd(nearPosition, XMVectorAdd(XMVectorScale(cameraUp, nearY), XMVectorScale(cameraRight, -nearX)));
        vertex[2] = XMVectorAdd(nearPosition, XMVectorAdd(XMVectorScale(cameraUp, -nearY), XMVectorScale(cameraRight, nearX)));
        vertex[3] = XMVectorAdd(nearPosition, XMVectorAdd(XMVectorScale(cameraUp, -nearY), XMVectorScale(cameraRight, -nearX)));
        vertex[4] = XMVectorAdd(farPosition, XMVectorAdd(XMVectorScale(cameraUp, farY), XMVectorScale(cameraRight, farX)));
        vertex[5] = XMVectorAdd(farPosition, XMVectorAdd(XMVectorScale(cameraUp, farY), XMVectorScale(cameraRight, -farX)));
        vertex[6] = XMVectorAdd(farPosition, XMVectorAdd(XMVectorScale(cameraUp, -farY), XMVectorScale(cameraRight, farX)));
        vertex[7] = XMVectorAdd(farPosition, XMVectorAdd(XMVectorScale(cameraUp, -farY), XMVectorScale(cameraRight, -farX)));

        XMFLOAT2 vertexMin(FLT_MAX, FLT_MAX);
        XMFLOAT2 vertexMax(-FLT_MAX, -FLT_MAX);
        for (auto& v : vertex)
        {
            XMFLOAT3 p;
            XMStoreFloat3(&p, XMVector3TransformCoord(v, lightViewProjection));
            vertexMin.x = (std::min)(vertexMin.x, p.x);
            vertexMin.y = (std::min)(vertexMin.y, p.y);
            vertexMax.x = (std::max)(vertexMax.x, p.x);
            vertexMax.y = (std::max)(vertexMax.y, p.y);
        }

        XMMATRIX cropMatrix = XMMatrixIdentity();
        XMFLOAT4X4 crop{};
        XMStoreFloat4x4(&crop, cropMatrix);
        const float xScale = 2.0f / (vertexMax.x - vertexMin.x);
        const float yScale = 2.0f / (vertexMax.y - vertexMin.y);
        crop._11 = xScale;
        crop._22 = yScale;
        crop._41 = -0.5f * (vertexMax.x + vertexMin.x) * xScale;
        crop._42 = -0.5f * (vertexMax.y + vertexMin.y) * yScale;

        XMStoreFloat4x4(&lightViewProjections[index], lightViewProjection * XMLoadFloat4x4(&crop));
    }
}

void CascadeShadowMap::SetViewport(ID3D11DeviceContext* dc) const
{
    dc->RSSetViewports(1, &viewport);
}

void CascadeShadowMap::SetCascadeSplitDistance(int index_, float distance_)
{
    if (index_ < 0 || index_ >= CascadeCount) return;
    cascadeSplitDistances[index_] = distance_;
}

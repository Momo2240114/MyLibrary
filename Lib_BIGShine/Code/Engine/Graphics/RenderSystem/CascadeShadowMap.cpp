#include "PrecompiledHeader.h"

#include "CascadeShadowMap.h"
#include "Engine/GamePlay/Camera/CameraComponent.h"
#include "Engine/Debug/TracyProfiler.h"
#include <algorithm>
#include <cfloat>
#include <cmath>

bool CascadeShadowMap::Initialize(ID3D11Device* device)
{
    // 各CascadeごとにDepthStencilとして書き込み、ShaderResourceとして読むTextureを作る。
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
        // Cascadeごとに別Textureを持ち、描画時は近距離から遠距離まで順に使う。
        HRESULT hr = device->CreateTexture2D(&desc, nullptr, shadowTextures[i].GetAddressOf());
        if (FAILED(hr))
        {
            return false;
        }

        hr = device->CreateDepthStencilView(shadowTextures[i].Get(), &dsvDesc, shadowDSVs[i].GetAddressOf());
        if (FAILED(hr))
        {
            return false;
        }

        hr = device->CreateShaderResourceView(shadowTextures[i].Get(), &srvDesc, shadowSRVs[i].GetAddressOf());
        if (FAILED(hr))
        {
            return false;
        }
    }

    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(ShadowmapSize);
    viewport.Height = static_cast<float>(ShadowmapSize);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    return true;
}

void CascadeShadowMap::Shutdown()
{
    BS_PROFILE_SCOPE("CascadeShadowMap::Shutdown");

    for (int index_ = 0; index_ < CascadeCount; ++index_)
    {
        shadowSRVs[index_].Reset();
        shadowDSVs[index_].Reset();
        shadowTextures[index_].Reset();
    }
}


void CascadeShadowMap::UpdateMatrices(const CameraComponent& camera, const DirectX::XMFLOAT3& lightDirection)
{
    BS_PROFILE_SCOPE("CascadeShadowMap::UpdateMatrices");

    using namespace DirectX;

    const float splitTable[CascadeCount + 1] =
    {
        // カメラのNearから各分割距離までを、Cascadeごとの視錐台範囲として扱う。
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
        // ライト方向が壊れている場合の保険。ゼロ方向だとLookAt行列を作れない。
        lightDir = XMVectorSet(0.0f, -1.0f, 1.0f, 0.0f);
    }
    lightDir = XMVector3Normalize(lightDir);

    XMVECTOR lightUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    const float lightUpDot_ = std::abs(XMVectorGetX(XMVector3Dot(lightDir, lightUp)));
    if (lightUpDot_ > 0.98f)
    {
        // Light方向とUpがほぼ平行だとView行列が不安定になるため、別のUpへ切り替える。
        lightUp = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
    }

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
        // 対象CascadeのCamera視錐台8頂点をワールド空間で求める。
        vertex[0] = XMVectorAdd(nearPosition, XMVectorAdd(XMVectorScale(cameraUp, nearY), XMVectorScale(cameraRight, nearX)));
        vertex[1] = XMVectorAdd(nearPosition, XMVectorAdd(XMVectorScale(cameraUp, nearY), XMVectorScale(cameraRight, -nearX)));
        vertex[2] = XMVectorAdd(nearPosition, XMVectorAdd(XMVectorScale(cameraUp, -nearY), XMVectorScale(cameraRight, nearX)));
        vertex[3] = XMVectorAdd(nearPosition, XMVectorAdd(XMVectorScale(cameraUp, -nearY), XMVectorScale(cameraRight, -nearX)));
        vertex[4] = XMVectorAdd(farPosition, XMVectorAdd(XMVectorScale(cameraUp, farY), XMVectorScale(cameraRight, farX)));
        vertex[5] = XMVectorAdd(farPosition, XMVectorAdd(XMVectorScale(cameraUp, farY), XMVectorScale(cameraRight, -farX)));
        vertex[6] = XMVectorAdd(farPosition, XMVectorAdd(XMVectorScale(cameraUp, -farY), XMVectorScale(cameraRight, farX)));
        vertex[7] = XMVectorAdd(farPosition, XMVectorAdd(XMVectorScale(cameraUp, -farY), XMVectorScale(cameraRight, -farX)));

        XMVECTOR cascadeCenter = XMVectorZero();
        for (const XMVECTOR& vertex_ : vertex)
        {
            cascadeCenter = XMVectorAdd(cascadeCenter, vertex_);
        }
        cascadeCenter = XMVectorScale(cascadeCenter, 1.0f / 8.0f);

        const float safeLightDistance_ = (std::max)(lightDistance, 1.0f);
        // 視錐台中心をライト方向から見下ろす位置にShadow Cameraを置く。
        XMVECTOR lightPosition = XMVectorSubtract(cascadeCenter, XMVectorScale(lightDir, safeLightDistance_));
        XMMATRIX lightView = XMMatrixLookAtLH(lightPosition, cascadeCenter, lightUp);

        XMFLOAT3 vertexMin(FLT_MAX, FLT_MAX, FLT_MAX);
        XMFLOAT3 vertexMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
        for (const XMVECTOR& vertex_ : vertex)
        {
            // Light空間へ変換し、このCascadeを覆う最小のOrtho範囲を作る。
            XMFLOAT3 transformedVertex_;
            XMStoreFloat3(&transformedVertex_, XMVector3TransformCoord(vertex_, lightView));
            vertexMin.x = (std::min)(vertexMin.x, transformedVertex_.x);
            vertexMin.y = (std::min)(vertexMin.y, transformedVertex_.y);
            vertexMin.z = (std::min)(vertexMin.z, transformedVertex_.z);
            vertexMax.x = (std::max)(vertexMax.x, transformedVertex_.x);
            vertexMax.y = (std::max)(vertexMax.y, transformedVertex_.y);
            vertexMax.z = (std::max)(vertexMax.z, transformedVertex_.z);
        }

        const float safeXyPadding_ = (std::max)(cascadeXyPadding, 0.0f);
        const float safeDepthPadding_ = (std::max)(cascadeDepthPadding, 0.0f);
        float left_ = vertexMin.x - safeXyPadding_;
        float right_ = vertexMax.x + safeXyPadding_;
        float bottom_ = vertexMin.y - safeXyPadding_;
        float top_ = vertexMax.y + safeXyPadding_;
        float nearZ_ = (std::max)(0.1f, vertexMin.z - safeDepthPadding_);
        float farZ_ = (std::max)(nearZ_ + 1.0f, vertexMax.z + safeDepthPadding_);

        if (texelSnapEnabled)
        {
            // ShadowMapのテクセル単位へ中心を丸め、カメラ移動時の影の揺れを抑える。
            const float width_ = (std::max)(right_ - left_, 1.0f);
            const float height_ = (std::max)(top_ - bottom_, 1.0f);
            const float texelWidth_ = width_ / static_cast<float>(ShadowmapSize);
            const float texelHeight_ = height_ / static_cast<float>(ShadowmapSize);
            float centerX_ = (left_ + right_) * 0.5f;
            float centerY_ = (bottom_ + top_) * 0.5f;
            centerX_ = std::floor(centerX_ / texelWidth_) * texelWidth_;
            centerY_ = std::floor(centerY_ / texelHeight_) * texelHeight_;
            left_ = centerX_ - width_ * 0.5f;
            right_ = centerX_ + width_ * 0.5f;
            bottom_ = centerY_ - height_ * 0.5f;
            top_ = centerY_ + height_ * 0.5f;
        }

        XMMATRIX lightProjection = XMMatrixOrthographicOffCenterLH(left_, right_, bottom_, top_, nearZ_, farZ_);
        XMStoreFloat4x4(&lightViewProjections[index], lightView * lightProjection);
    }
}

void CascadeShadowMap::SetViewport(ID3D11DeviceContext* dc) const
{
    // ShadowMap描画時はCascade用の固定解像度Viewportへ切り替える。
    dc->RSSetViewports(1, &viewport);
}

void CascadeShadowMap::SetCascadeSplitDistance(int index_, float distance_)
{
    if (index_ < 0 || index_ >= CascadeCount)
    {
        return;
    }
    cascadeSplitDistances[index_] = distance_;
}

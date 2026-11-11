#include "PrecompiledHeader.h"

#include "MotionBlurSystem.h"

#include <algorithm>
#include <cstring>

#include "Engine/Graphics/Core/CPUResourceLoader.h"
#include "Engine/Debug/TracyProfiler.h"

bool MotionBlurSystem::Initialize(ID3D11Device* device_, int width_, int height_)
{
    // Scene解像度と、Velocity最大値をまとめるTile解像度を保持する。
    width = width_;
    height = height_;
    tileWidth = (width + TILE_SIZE - 1) / TILE_SIZE;
    tileHeight = (height + TILE_SIZE - 1) / TILE_SIZE;
    hasPreviousViewProjection = false;
    DirectX::XMStoreFloat4x4(&previousViewProjection, DirectX::XMMatrixIdentity());

    // FullScreenVSは全Passで共有する。
    if (FAILED(CPUResourceLoader::LoadVertexShader(
        device_, "Data/Shader/FullScreenVS.cso", nullptr, 0, nullptr, fullScreenVs.GetAddressOf())))
    {
        return false;
    }

    struct PixelShaderSource
    {
        const char* path_;
        Microsoft::WRL::ComPtr<ID3D11PixelShader>* shader_;
    };
    PixelShaderSource pixelShaderSources_[] =
    {
        // Velocity生成、Tile最大値、近傍最大値、最終Blurの4Pass。
        { "Data/Shader/MotionVelocityPS.cso", &velocityPixelShader },
        { "Data/Shader/MotionVelocityTileMaxPS.cso", &tileMaxPixelShader },
        { "Data/Shader/MotionVelocityNeighborMaxPS.cso", &neighborMaxPixelShader },
        { "Data/Shader/MotionBlurPS.cso", &pixelShader },
    };
    for (PixelShaderSource& source_ : pixelShaderSources_)
    {
        if (FAILED(CPUResourceLoader::LoadPixelShader(
            device_, source_.path_, source_.shader_->GetAddressOf())))
        {
            return false;
        }
    }

    // Full resolution Velocityと、縮小したTile/Neighbor Max Bufferを作る。
    if (!CreateRenderTarget(device_, width, height, DXGI_FORMAT_R16G16_FLOAT, velocityBuffer) ||
        !CreateRenderTarget(device_, tileWidth, tileHeight, DXGI_FORMAT_R16G16_FLOAT, tileMaxBuffer) ||
        !CreateRenderTarget(device_, tileWidth, tileHeight, DXGI_FORMAT_R16G16_FLOAT, neighborMaxBuffer))
    {
        return false;
    }

    // MotionBlur設定は各Passで共有し、毎Frame更新する。
    D3D11_BUFFER_DESC desc_{};
    desc_.ByteWidth = sizeof(CbMotionBlur);
    desc_.Usage = D3D11_USAGE_DYNAMIC;
    desc_.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc_.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    return SUCCEEDED(device_->CreateBuffer(&desc_, nullptr, constantBuffer.GetAddressOf()));
}

void MotionBlurSystem::Shutdown()
{
    BS_PROFILE_SCOPE("MotionBlurSystem::Shutdown");

    // 内部RTとShader参照を全て解放し、外部Renderer参照も切る。
    velocityBuffer.srv.Reset();
    velocityBuffer.rtv.Reset();
    velocityBuffer.texture.Reset();
    tileMaxBuffer.srv.Reset();
    tileMaxBuffer.rtv.Reset();
    tileMaxBuffer.texture.Reset();
    neighborMaxBuffer.srv.Reset();
    neighborMaxBuffer.rtv.Reset();
    neighborMaxBuffer.texture.Reset();
    constantBuffer.Reset();
    velocityPixelShader.Reset();
    tileMaxPixelShader.Reset();
    neighborMaxPixelShader.Reset();
    pixelShader.Reset();
    fullScreenVs.Reset();
    objectVelocityRenderer = nullptr;
    hasPreviousViewProjection = false;
}

void MotionBlurSystem::SynchronizeCameraHistory(const RenderContext& rc_)
{
    // Scene切り替えやEditor操作後に、現在Cameraを履歴基準として揃える。
    UpdatePreviousViewProjection(rc_);
}

void MotionBlurSystem::ResetCameraHistory()
{
    DirectX::XMStoreFloat4x4(&previousViewProjection, DirectX::XMMatrixIdentity());
    hasPreviousViewProjection = false;
}

void MotionBlurSystem::SetObjectVelocityRenderer(ObjectVelocityRenderer* objectVelocityRenderer_)
{
    objectVelocityRenderer = objectVelocityRenderer_;
}

void MotionBlurSystem::ClearVelocityBuffer(ID3D11DeviceContext* dc_)
{
    if (dc_ == nullptr || velocityBuffer.rtv == nullptr) return;
    // Velocity 0は「Blurなし」を意味する。
    const float clearVelocity_[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    dc_->ClearRenderTargetView(velocityBuffer.rtv.Get(), clearVelocity_);
}

void MotionBlurSystem::Execute(
    const RenderContext& rc_,
    ID3D11ShaderResourceView* sceneSRV_,
    ID3D11ShaderResourceView* depthSRV_,
    ID3D11RenderTargetView* outputRTV_,
    const Parameters& parameters_)
{
    ID3D11DeviceContext* dc_ = rc_.deviceContext;
    const RenderState* rs_ = rc_.renderState;
    if (dc_ == nullptr || rs_ == nullptr)
    {
        return;
    }

    Parameters safeParameters_ = parameters_;
    if (depthSRV_ == nullptr || rc_.camera == nullptr)
    {
        // DepthやCameraが無い場合はVelocity復元できないため、Blurを無効化して通常合成へ落とす。
        safeParameters_.enabled = false;
    }

    ClearVelocityBuffer(dc_);

    // 以降のPassはFullScreen描画なので共通のRenderStateを先に設定する。
    ID3D11SamplerState* sampler_ = rs_->GetSamplerState(SamplerState::LinearClamp);
    dc_->RSSetState(rs_->GetRasterizerState(RasterizerState::SolidCullNone));
    dc_->OMSetDepthStencilState(rs_->GetDepthStencilState(DepthState::NoTestNoWrite), 0);
    dc_->VSSetShader(fullScreenVs.Get(), nullptr, 0);
    dc_->PSSetSamplers(0, 1, &sampler_);
    dc_->PSSetConstantBuffers(0, 1, constantBuffer.GetAddressOf());

    const bool useCameraVelocityPass_ = safeParameters_.useCameraVelocity || objectVelocityRenderer == nullptr;
    if (safeParameters_.enabled &&
        useCameraVelocityPass_ &&
        depthSRV_ != nullptr &&
        hasPreviousViewProjection)
    {
        // Depthと前FrameのViewProjectionからCamera移動分のScreen Velocityを生成する。
        UpdateConstantBuffer(rc_, safeParameters_);
        ID3D11ShaderResourceView* velocityResources_[] = { depthSRV_ };
        DrawPass(dc_, velocityBuffer.rtv.Get(), velocityPixelShader.Get(), velocityResources_, 1, width, height);
    }

    if (safeParameters_.enabled && objectVelocityRenderer != nullptr)
    {
        // Object Velocity Rendererがあれば、Model単位の移動/Skinned速度をVelocity Bufferへ追加する。
        const DirectX::XMFLOAT4X4 currentViewProjection_ = BuildViewProjection(rc_);
        objectVelocityRenderer->DrawObjectVelocity(
            rc_,
            velocityBuffer.rtv.Get(),
            width,
            height,
            currentViewProjection_,
            previousViewProjection);

        // Object描画Passで変更されたRenderStateを、PostEffect用へ戻す。
        dc_->RSSetState(rs_->GetRasterizerState(RasterizerState::SolidCullNone));
        dc_->OMSetDepthStencilState(rs_->GetDepthStencilState(DepthState::NoTestNoWrite), 0);
        dc_->VSSetShader(fullScreenVs.Get(), nullptr, 0);
        dc_->PSSetSamplers(0, 1, &sampler_);
        dc_->PSSetConstantBuffers(0, 1, constantBuffer.GetAddressOf());
    }

    ExecuteBlurFromCurrentVelocity(rc_, sceneSRV_, depthSRV_, outputRTV_, safeParameters_);
    if (safeParameters_.enabled && objectVelocityRenderer != nullptr)
    {
        // Velocity描画に使った現在Transformを次Frame履歴へ進める。
        objectVelocityRenderer->CommitHistory();
    }
    // Camera履歴も最後に現在Frameへ更新する。
    UpdatePreviousViewProjection(rc_);
}

void MotionBlurSystem::ExecuteBlurFromCurrentVelocity(
    const RenderContext& rc_,
    ID3D11ShaderResourceView* sceneSRV_,
    ID3D11ShaderResourceView* depthSRV_,
    ID3D11RenderTargetView* outputRTV_,
    const Parameters& parameters_)
{
    ID3D11DeviceContext* dc_ = rc_.deviceContext;
    const RenderState* rs_ = rc_.renderState;
    if (dc_ == nullptr || rs_ == nullptr || sceneSRV_ == nullptr || outputRTV_ == nullptr)
    {
        return;
    }

    Parameters safeParameters_ = parameters_;
    if (rc_.camera == nullptr)
    {
        // Cameraが無い場合、Blur ShaderへIdentity情報だけ渡して無効扱いにする。
        safeParameters_.enabled = false;
    }
    UpdateConstantBuffer(rc_, safeParameters_);

    dc_->RSSetState(rs_->GetRasterizerState(RasterizerState::SolidCullNone));
    dc_->OMSetDepthStencilState(rs_->GetDepthStencilState(DepthState::NoTestNoWrite), 0);
    ID3D11SamplerState* sampler_ = rs_->GetSamplerState(SamplerState::LinearClamp);
    dc_->VSSetShader(fullScreenVs.Get(), nullptr, 0);
    dc_->PSSetSamplers(0, 1, &sampler_);
    dc_->PSSetConstantBuffers(0, 1, constantBuffer.GetAddressOf());

    // Velocity BufferをTile単位で最大化し、長いBlurの方向を低解像度でまとめる。
    ID3D11ShaderResourceView* tileResources_[] = { velocityBuffer.srv.Get() };
    DrawPass(dc_, tileMaxBuffer.rtv.Get(), tileMaxPixelShader.Get(), tileResources_, 1, tileWidth, tileHeight);

    // 周囲Tileの最大Velocityも拾い、Blur範囲の境界で途切れにくくする。
    ID3D11ShaderResourceView* neighborResources_[] = { tileMaxBuffer.srv.Get() };
    DrawPass(dc_, neighborMaxBuffer.rtv.Get(), neighborMaxPixelShader.Get(), neighborResources_, 1, tileWidth, tileHeight);

    ID3D11ShaderResourceView* blurResources_[] =
    {
        // Scene, Depth, Full Velocity, NeighborMax Velocityを使って最終Blurを合成する。
        sceneSRV_, depthSRV_, velocityBuffer.srv.Get(), neighborMaxBuffer.srv.Get()
    };
    DrawPass(dc_, outputRTV_, pixelShader.Get(), blurResources_, 4, width, height);

    ID3D11ShaderResourceView* nullSRVs_[4] = {};
    dc_->PSSetShaderResources(0, _countof(nullSRVs_), nullSRVs_);
    dc_->OMSetDepthStencilState(rs_->GetDepthStencilState(DepthState::TestOnly), 0);
}

bool MotionBlurSystem::CreateRenderTarget(
    ID3D11Device* device_, int width_, int height_, DXGI_FORMAT format_, RenderTarget& target_)
{
    // 内部PassでRTVとして書き、次PassでSRVとして読むTextureを作る。
    D3D11_TEXTURE2D_DESC desc_{};
    desc_.Width = static_cast<UINT>(width_);
    desc_.Height = static_cast<UINT>(height_);
    desc_.MipLevels = 1;
    desc_.ArraySize = 1;
    desc_.Format = format_;
    desc_.SampleDesc.Count = 1;
    desc_.Usage = D3D11_USAGE_DEFAULT;
    desc_.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(device_->CreateTexture2D(&desc_, nullptr, target_.texture.GetAddressOf()))) return false;
    if (FAILED(device_->CreateRenderTargetView(target_.texture.Get(), nullptr, target_.rtv.GetAddressOf()))) return false;
    return SUCCEEDED(device_->CreateShaderResourceView(target_.texture.Get(), nullptr, target_.srv.GetAddressOf()));
}

void MotionBlurSystem::UpdateConstantBuffer(const RenderContext& rc_, const Parameters& parameters_)
{
    // Camera Velocity復元とBlur合成に必要な共通値を詰める。
    CbMotionBlur cb_{};
    cb_.screenSize = { width, height };
    cb_.screenPixelSize = { 1.0f / static_cast<float>(width), 1.0f / static_cast<float>(height) };
    cb_.previousViewProjection = previousViewProjection;
    cb_.enabled = parameters_.enabled ? 1 : 0;
    cb_.sampleCount = (std::max)(2, (std::min)(parameters_.sampleCount, 16));
    cb_.strength = parameters_.strength;
    cb_.maxVelocity = parameters_.maxVelocity;
    DirectX::XMStoreFloat4x4(&cb_.invViewProjection, DirectX::XMMatrixIdentity());
    if (rc_.camera != nullptr)
    {
        // DepthからWorld位置を復元するため、現在ViewProjectionの逆行列を渡す。
        const DirectX::XMMATRIX view_ = DirectX::XMLoadFloat4x4(&rc_.camera->GetView());
        const DirectX::XMMATRIX projection_ = DirectX::XMLoadFloat4x4(&rc_.camera->GetProjection());
        DirectX::XMStoreFloat4x4(&cb_.invViewProjection, DirectX::XMMatrixInverse(nullptr, view_ * projection_));
    }
    D3D11_MAPPED_SUBRESOURCE mapped_{};
    if (SUCCEEDED(rc_.deviceContext->Map(constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_)))
    {
        std::memcpy(mapped_.pData, &cb_, sizeof(cb_));
        rc_.deviceContext->Unmap(constantBuffer.Get(), 0);
    }
}

void MotionBlurSystem::UpdatePreviousViewProjection(const RenderContext& rc_)
{
    if (rc_.camera == nullptr)
    {
        // Cameraが無いFrameの次はCamera Velocityを使わない。
        hasPreviousViewProjection = false;
        return;
    }
    const DirectX::XMMATRIX view_ = DirectX::XMLoadFloat4x4(&rc_.camera->GetView());
    const DirectX::XMMATRIX projection_ = DirectX::XMLoadFloat4x4(&rc_.camera->GetProjection());
    DirectX::XMStoreFloat4x4(&previousViewProjection, view_ * projection_);
    hasPreviousViewProjection = true;
}

DirectX::XMFLOAT4X4 MotionBlurSystem::BuildViewProjection(const RenderContext& rc_) const
{
    DirectX::XMFLOAT4X4 viewProjection_{};
    DirectX::XMStoreFloat4x4(&viewProjection_, DirectX::XMMatrixIdentity());
    if (rc_.camera == nullptr)
    {
        return viewProjection_;
    }

    const DirectX::XMMATRIX view_ = DirectX::XMLoadFloat4x4(&rc_.camera->GetView());
    const DirectX::XMMATRIX projection_ = DirectX::XMLoadFloat4x4(&rc_.camera->GetProjection());
    DirectX::XMStoreFloat4x4(&viewProjection_, view_ * projection_);
    return viewProjection_;
}

void MotionBlurSystem::DrawPass(
    ID3D11DeviceContext* dc_, ID3D11RenderTargetView* outputRTV_, ID3D11PixelShader* pixelShader_,
    ID3D11ShaderResourceView* const* shaderResources_, UINT shaderResourceCount_, int width_, int height_)
{
    // 指定サイズのViewportへ切り替え、Pixel Shaderだけ差し替えてFullScreen Passを実行する。
    D3D11_VIEWPORT viewport_{};
    viewport_.Width = static_cast<float>(width_);
    viewport_.Height = static_cast<float>(height_);
    viewport_.MinDepth = 0.0f;
    viewport_.MaxDepth = 1.0f;
    dc_->RSSetViewports(1, &viewport_);
    dc_->OMSetRenderTargets(1, &outputRTV_, nullptr);
    dc_->PSSetShader(pixelShader_, nullptr, 0);
    dc_->PSSetShaderResources(0, shaderResourceCount_, shaderResources_);
    DrawFullScreenQuad(dc_);
    // 次PassでRTVとして使う可能性があるため、読んだSRVを解除する。
    ID3D11ShaderResourceView* nullSRVs_[4] = {};
    dc_->PSSetShaderResources(0, shaderResourceCount_, nullSRVs_);
}

void MotionBlurSystem::DrawFullScreenQuad(ID3D11DeviceContext* dc_)
{
    // FullScreenVSがSV_VertexIDから三角形を作るため、頂点入力は不要。
    dc_->IASetInputLayout(nullptr);
    dc_->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
    dc_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    dc_->Draw(3, 0);
}

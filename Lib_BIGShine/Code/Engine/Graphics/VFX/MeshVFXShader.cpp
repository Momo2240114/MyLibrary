#include "MeshVFXShader.h"

#include "Engine/Graphics/Core/CPUResourceLoader.h"
#include "Engine/Graphics/Core/RenderContext.h"
#include "Engine/Graphics/Resource/GLTFModel.h"

bool MeshVFXShader::Initialize(ID3D11Device* device_)
{
    if (!device_) return false;

    HRESULT result_ = CPUResourceLoader::LoadVertexShader(
        device_,
        "Shader/MeshVFXVS.cso",
        GltfModelCommponennt::InputElementDescs.data(),
        static_cast<UINT>(GltfModelCommponennt::InputElementDescs.size()),
        inputLayout.GetAddressOf(),
        vertexShader.GetAddressOf());
    if (FAILED(result_)) return false;

    result_ = CPUResourceLoader::LoadPixelShader(
        device_, "Shader/MeshVFXPS.cso", pixelShader.GetAddressOf());
    if (FAILED(result_)) return false;

    result_ = CPUResourceLoader::CreateConstantBuffer(
        device_, sizeof(CbScene), sceneConstantBuffer.GetAddressOf());
    if (FAILED(result_)) return false;

    result_ = CPUResourceLoader::CreateConstantBuffer(
        device_, sizeof(CbMeshVFX), partConstantBuffer.GetAddressOf());
    return SUCCEEDED(result_);
}

void MeshVFXShader::Begin(const RenderContext& renderContext_)
{
    ID3D11DeviceContext* deviceContext_ = renderContext_.deviceContext;
    deviceContext_->IASetInputLayout(inputLayout.Get());
    deviceContext_->VSSetShader(vertexShader.Get(), nullptr, 0);
    deviceContext_->GSSetShader(nullptr, nullptr, 0);
    deviceContext_->PSSetShader(pixelShader.Get(), nullptr, 0);

    ID3D11Buffer* sceneBuffer_ = sceneConstantBuffer.Get();
    ID3D11Buffer* partBuffer_ = partConstantBuffer.Get();
    deviceContext_->VSSetConstantBuffers(7, 1, &sceneBuffer_);
    deviceContext_->PSSetConstantBuffers(7, 1, &sceneBuffer_);
    deviceContext_->VSSetConstantBuffers(0, 1, &partBuffer_);
    deviceContext_->PSSetConstantBuffers(0, 1, &partBuffer_);
}

void MeshVFXShader::UpdateScene(const RenderContext& renderContext_)
{
    if (!renderContext_.camera) return;

    const DirectX::XMMATRIX view_ = DirectX::XMLoadFloat4x4(&renderContext_.camera->GetView());
    const DirectX::XMMATRIX projection_ = DirectX::XMLoadFloat4x4(&renderContext_.camera->GetProjection());
    const DirectX::XMMATRIX viewProjection_ = view_ * projection_;

    CbScene scene_{};
    DirectX::XMStoreFloat4x4(&scene_.viewProjection, viewProjection_);
    DirectX::XMStoreFloat4x4(
        &scene_.invViewProjection,
        DirectX::XMMatrixInverse(nullptr, viewProjection_));
    const DirectX::XMFLOAT3& eye_ = renderContext_.camera->GetEye();
    scene_.cameraPosition = { eye_.x, eye_.y, eye_.z, 1.0f };
    renderContext_.deviceContext->UpdateSubresource(
        sceneConstantBuffer.Get(), 0, nullptr, &scene_, 0, 0);
}

void MeshVFXShader::UpdatePart(
    const RenderContext& renderContext_,
    const MeshVFXPart& part_,
    const MeshVFXRuntimePart& runtimePart_,
    ID3D11ShaderResourceView* baseTexture_,
    ID3D11ShaderResourceView* normalTexture_,
    const DirectX::XMMATRIX& world_,
    float partTime_,
    float alpha_)
{
    CbMeshVFX constants_{};
    DirectX::XMStoreFloat4x4(&constants_.world, world_);
    constants_.color = part_.color;
    constants_.baseUVSpeed = part_.baseUVSpeed;
    constants_.maskUVSpeed = part_.maskUVSpeed;
    constants_.noiseUVSpeed = part_.noiseUVSpeed;
    constants_.uvRotationCenter = part_.uvRotationCenter;
    constants_.time = partTime_;
    constants_.alpha = alpha_;
    constants_.normalStrength = part_.normalStrength;
    constants_.noiseStrength = part_.noiseStrength;
    constants_.fresnelPower = part_.fresnelPower;
    constants_.fresnelStrength = part_.fresnelStrength;
    constants_.dissolveThreshold = part_.dissolveThreshold;
    constants_.baseUVRotation = DirectX::XMConvertToRadians(part_.baseUVRotationSpeed * partTime_);
    constants_.maskUVRotation = DirectX::XMConvertToRadians(part_.maskUVRotationSpeed * partTime_);
    constants_.noiseUVRotation = DirectX::XMConvertToRadians(part_.noiseUVRotationSpeed * partTime_);
    constants_.useMask = part_.useMask ? 1u : 0u;
    constants_.useNoise = part_.useNoise ? 1u : 0u;
    constants_.useNormal = part_.useNormal ? 1u : 0u;
    constants_.useFresnel = part_.useFresnel ? 1u : 0u;

    ID3D11DeviceContext* deviceContext_ = renderContext_.deviceContext;
    deviceContext_->UpdateSubresource(partConstantBuffer.Get(), 0, nullptr, &constants_, 0, 0);
    ID3D11ShaderResourceView* textures_[] =
    {
        baseTexture_ ? baseTexture_ : runtimePart_.baseTexture.Get(),
        runtimePart_.maskTexture.Get(),
        runtimePart_.noiseTexture.Get(),
        normalTexture_ ? normalTexture_ : runtimePart_.normalTexture.Get(),
    };
    deviceContext_->PSSetShaderResources(0, _countof(textures_), textures_);
}

void MeshVFXShader::End(const RenderContext& renderContext_)
{
    ID3D11DeviceContext* deviceContext_ = renderContext_.deviceContext;
    ID3D11ShaderResourceView* nullTextures_[] = { nullptr, nullptr, nullptr, nullptr };
    ID3D11Buffer* nullBuffer_ = nullptr;
    deviceContext_->PSSetShaderResources(0, _countof(nullTextures_), nullTextures_);
    deviceContext_->VSSetConstantBuffers(0, 1, &nullBuffer_);
    deviceContext_->PSSetConstantBuffers(0, 1, &nullBuffer_);
    deviceContext_->VSSetConstantBuffers(7, 1, &nullBuffer_);
    deviceContext_->PSSetConstantBuffers(7, 1, &nullBuffer_);
    deviceContext_->VSSetShader(nullptr, nullptr, 0);
    deviceContext_->PSSetShader(nullptr, nullptr, 0);
    deviceContext_->IASetInputLayout(nullptr);
}

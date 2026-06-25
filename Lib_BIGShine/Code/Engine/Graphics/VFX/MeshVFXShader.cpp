#include "MeshVFXShader.h"

#include "Engine/Graphics/Core/CPUResourceLoader.h"
#include "Engine/Graphics/Core/RenderContext.h"
#include "Engine/Graphics/Resource/GLTFModel.h"

#include <algorithm>
#include <cmath>
#include <numbers>

bool MeshVFXShader::Initialize(ID3D11Device* device_)
{
    // MeshVFXで使うShaderとConstantBufferをまとめて作成する。
    if (!device_) return false;

    HRESULT result_ = CPUResourceLoader::LoadVertexShader(
        device_,
        "Shader/MeshVFXVS.cso",
        GltfModelCommponennt::InputElementDescs.data(),
        static_cast<UINT>(GltfModelCommponennt::InputElementDescs.size()),
        inputLayout.GetAddressOf(),
        vertexShader.GetAddressOf());
    if (FAILED(result_)) return false;

    result_ = CPUResourceLoader::LoadVertexShader(
        device_,
        "Shader/MaterialOverlayVS.cso",
        GltfModelCommponennt::InputElementDescs.data(),
        static_cast<UINT>(GltfModelCommponennt::InputElementDescs.size()),
        nullptr,
        materialOverlayVertexShader.GetAddressOf());
    if (FAILED(result_)) return false;

    result_ = CPUResourceLoader::LoadVertexShader(
        device_,
        "Shader/AfterimageInstancedVS.cso",
        GltfModelCommponennt::InputElementDescs.data(),
        static_cast<UINT>(GltfModelCommponennt::InputElementDescs.size()),
        nullptr,
        afterimageInstancedVertexShader.GetAddressOf());
    if (FAILED(result_)) return false;

    result_ = CPUResourceLoader::LoadPixelShader(
        device_, "Shader/MeshVFXPS.cso", pixelShader.GetAddressOf());
    if (FAILED(result_)) return false;

    result_ = CPUResourceLoader::CreateConstantBuffer(
        device_, sizeof(CbScene), sceneConstantBuffer.GetAddressOf());
    if (FAILED(result_)) return false;

    result_ = CPUResourceLoader::CreateConstantBuffer(
        device_, sizeof(CbMeshVFX), partConstantBuffer.GetAddressOf());
    if (FAILED(result_)) return false;
    return SUCCEEDED(CPUResourceLoader::CreateConstantBuffer(
        device_, sizeof(DirectX::XMFLOAT4X4) * 256, skeletonConstantBuffer.GetAddressOf()));
}

void MeshVFXShader::Begin(
    const RenderContext& renderContext_,
    ID3D11ShaderResourceView* sceneColor_,
    ID3D11ShaderResourceView* sceneDepth_,
    bool useMaterialOverlay_,
    bool useInstancedPose_)
{
    // 描画モードに応じてVertexShaderを選び、Scene情報と背景TextureをPixelShaderへ渡す。
    ID3D11DeviceContext* deviceContext_ = renderContext_.deviceContext;
    deviceContext_->IASetInputLayout(inputLayout.Get());
    ID3D11VertexShader* selectedVertexShader_ = vertexShader.Get();
    if (useInstancedPose_)
    {
        selectedVertexShader_ = afterimageInstancedVertexShader.Get();
    }
    else if (useMaterialOverlay_)
    {
        selectedVertexShader_ = materialOverlayVertexShader.Get();
    }
    deviceContext_->VSSetShader(selectedVertexShader_, nullptr, 0);
    deviceContext_->GSSetShader(nullptr, nullptr, 0);
    deviceContext_->PSSetShader(pixelShader.Get(), nullptr, 0);
    ID3D11SamplerState* vertexSampler_ =
        renderContext_.renderState->GetSamplerState(SamplerState::LinearWrap);
    deviceContext_->VSSetSamplers(0, 1, &vertexSampler_);

    ID3D11Buffer* sceneBuffer_ = sceneConstantBuffer.Get();
    ID3D11Buffer* partBuffer_ = partConstantBuffer.Get();
    deviceContext_->VSSetConstantBuffers(7, 1, &sceneBuffer_);
    ID3D11Buffer* skeletonBuffer_ = skeletonConstantBuffer.Get();
    deviceContext_->VSSetConstantBuffers(6, 1, &skeletonBuffer_);
    deviceContext_->PSSetConstantBuffers(7, 1, &sceneBuffer_);
    deviceContext_->VSSetConstantBuffers(0, 1, &partBuffer_);
    deviceContext_->PSSetConstantBuffers(0, 1, &partBuffer_);
    deviceContext_->PSSetShaderResources(7, 1, &sceneColor_);
    deviceContext_->PSSetShaderResources(8, 1, &sceneDepth_);
}

void MeshVFXShader::UpdateSkeleton(
    const RenderContext& renderContext_,
    const GltfModelCommponennt::Mesh& mesh_)
{
    // 通常モデル描画用に、現在Meshのボーン行列をSkinning用CBへ転送する。
    DirectX::XMFLOAT4X4 transforms_[256]{};
    if (!mesh_.bones.empty())
    {
        const size_t boneCount_ = (std::min)(mesh_.bones.size(), static_cast<size_t>(256));
        for (size_t boneIndex_ = 0; boneIndex_ < boneCount_; ++boneIndex_)
        {
            const GltfModelCommponennt::Bone& bone_ = mesh_.bones[boneIndex_];
            const DirectX::XMMATRIX offset_ = DirectX::XMLoadFloat4x4(&bone_.offsetTransform);
            const DirectX::XMMATRIX world_ = bone_.node
                ? DirectX::XMLoadFloat4x4(&bone_.node->worldTransform)
                : DirectX::XMMatrixIdentity();
            DirectX::XMStoreFloat4x4(&transforms_[boneIndex_], offset_ * world_);
        }
    }
    else if (mesh_.node)
    {
        // Boneを持たないMeshはNodeのWorldTransformだけを0番へ入れる。
        transforms_[0] = mesh_.node->worldTransform;
    }
    renderContext_.deviceContext->UpdateSubresource(
        skeletonConstantBuffer.Get(), 0, nullptr, transforms_, 0, 0);
}

void MeshVFXShader::UpdateSkeleton(
    const RenderContext& renderContext_,
    const std::vector<DirectX::XMFLOAT4X4>& transforms_)
{
    // Afterimageなど、外部で保存したPose行列をそのままSkinning用CBへ転送する。
    DirectX::XMFLOAT4X4 bufferTransforms_[256]{};
    const size_t transformCount_ = (std::min)(transforms_.size(), static_cast<size_t>(256));
    for (size_t transformIndex_ = 0; transformIndex_ < transformCount_; ++transformIndex_)
    {
        bufferTransforms_[transformIndex_] = transforms_[transformIndex_];
    }
    renderContext_.deviceContext->UpdateSubresource(
        skeletonConstantBuffer.Get(), 0, nullptr, bufferTransforms_, 0, 0);
}

void MeshVFXShader::UpdateScene(const RenderContext& renderContext_)
{
    // Camera/Depth復元情報をHLSLのScene定数へ合わせて更新する。
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
    ID3D11ShaderResourceView* maskTexture_,
    ID3D11ShaderResourceView* noiseTexture_,
    const DirectX::XMMATRIX& world_,
    float partTime_,
    float alpha_)
{
    // 1Part分の編集値とRuntime TextureをShaderへ送る。ここがC++設定とHLSL表現の接続点。
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
    const auto calculateRotation_ = [partTime_](float initialRotation_, float rotationSpeed_)
        {
            // GUIは度数法で扱い、Shaderへはラジアンで渡す。
            const float rotation_ = DirectX::XMConvertToRadians(
                initialRotation_ + rotationSpeed_ * partTime_);
            return std::fmod(rotation_, 2.0f * std::numbers::pi_v<float>);
        };
    constants_.baseUVRotation = calculateRotation_(part_.baseUVRotation, part_.baseUVRotationSpeed);
    constants_.maskUVRotation = calculateRotation_(part_.maskUVRotation, part_.maskUVRotationSpeed);
    constants_.noiseUVRotation = calculateRotation_(part_.noiseUVRotation, part_.noiseUVRotationSpeed);
    constants_.uvRadialScale = part_.uvRadialScale;
    constants_.uvRadialScaleSpeed = part_.uvRadialScaleSpeed;
    constants_.vertexNoiseHorizontalStrength = part_.vertexNoiseHorizontalStrength;
    constants_.vertexNoiseVerticalStrength = part_.vertexNoiseVerticalStrength;
    constants_.vertexNoiseHorizontalScale = part_.vertexNoiseHorizontalScale;
    constants_.vertexNoiseVerticalScale = part_.vertexNoiseVerticalScale;
    constants_.vertexNoiseSpeed = part_.vertexNoiseSpeed;
    constants_.useVertexNoise = part_.useVertexNoise ? 1u : 0u;
    constants_.useMask = part_.useMask ? 1u : 0u;
    constants_.useNoise = part_.useNoise ? 1u : 0u;
    constants_.useNormal = part_.useNormal ? 1u : 0u;
    constants_.useFresnel = part_.useFresnel ? 1u : 0u;
    constants_.edgeColor = part_.edgeColor;
    constants_.maskDistortionStrength = part_.maskDistortionStrength;
    constants_.maskDistortionOpacity = part_.maskDistortionOpacity;
    constants_.silhouetteEdgeStrength = part_.silhouetteEdgeStrength;
    constants_.silhouetteEdgePower = part_.silhouetteEdgePower;
    constants_.intersectionEdgeStrength = part_.intersectionEdgeStrength;
    constants_.intersectionEdgeWidth = part_.intersectionEdgeWidth;
    constants_.useMaskDistortion = part_.useMaskDistortion ? 1u : 0u;
    constants_.useEdgeHighlight = part_.useEdgeHighlight ? 1u : 0u;
    constants_.hasDistortionNoiseTexture = runtimePart_.distortionNoiseTexture ? 1u : 0u;
    constants_.hasVertexNoiseTexture = runtimePart_.vertexNoiseTexture ? 1u : 0u;
    constants_.edgePatternMode = static_cast<uint32_t>(part_.edgePatternMode);
    constants_.edgePatternStrength = part_.edgePatternStrength;

    ID3D11DeviceContext* deviceContext_ = renderContext_.deviceContext;
    deviceContext_->UpdateSubresource(partConstantBuffer.Get(), 0, nullptr, &constants_, 0, 0);
    ID3D11ShaderResourceView* baseTextureView_ =
        baseTexture_ ? baseTexture_ : runtimePart_.baseTexture.Get();
    ID3D11ShaderResourceView* normalTextureView_ =
        normalTexture_ ? normalTexture_ : runtimePart_.normalTexture.Get();
    deviceContext_->PSSetShaderResources(0, 1, &baseTextureView_);
    deviceContext_->PSSetShaderResources(1, 1, &maskTexture_);
    deviceContext_->PSSetShaderResources(2, 1, &noiseTexture_);
    deviceContext_->PSSetShaderResources(3, 1, &normalTextureView_);
    ID3D11ShaderResourceView* distortionNoiseTextureView_ = runtimePart_.distortionNoiseTexture.Get();
    deviceContext_->PSSetShaderResources(4, 1, &distortionNoiseTextureView_);
    ID3D11ShaderResourceView* vertexNoiseTextureView_ = runtimePart_.vertexNoiseTexture.Get();
    deviceContext_->VSSetShaderResources(0, 1, &vertexNoiseTextureView_);
}

void MeshVFXShader::End(const RenderContext& renderContext_)
{
    // 次の描画へTexture/CB/Shader状態を持ち越さないよう、MeshVFXで使ったSlotを解除する。
    ID3D11DeviceContext* deviceContext_ = renderContext_.deviceContext;
    ID3D11ShaderResourceView* nullTextures_[9] = {};
    ID3D11Buffer* nullBuffer_ = nullptr;
    deviceContext_->PSSetShaderResources(0, _countof(nullTextures_), nullTextures_);
    ID3D11ShaderResourceView* nullVertexTexture_ = nullptr;
    deviceContext_->VSSetShaderResources(0, 1, &nullVertexTexture_);
    deviceContext_->VSSetConstantBuffers(0, 1, &nullBuffer_);
    deviceContext_->PSSetConstantBuffers(0, 1, &nullBuffer_);
    deviceContext_->VSSetConstantBuffers(7, 1, &nullBuffer_);
    deviceContext_->PSSetConstantBuffers(7, 1, &nullBuffer_);
    deviceContext_->VSSetShader(nullptr, nullptr, 0);
    deviceContext_->PSSetShader(nullptr, nullptr, 0);
    deviceContext_->IASetInputLayout(nullptr);
}

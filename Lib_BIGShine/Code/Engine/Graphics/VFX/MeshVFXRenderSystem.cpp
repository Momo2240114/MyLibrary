#include "MeshVFXRenderSystem.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/Graphics/Core/CPUResourceLoader.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Graphics/Core/RenderContext.h"
#include "Engine/Graphics/Resource/ModelResource.h"
#include "Engine/Debug/TracyProfiler.h"
#include "MeshVFXComponent.h"

#include <algorithm>
#include <cstring>
#include <unordered_map>
#include <cmath>

namespace
{
    struct PartAnimationState
    {
        DirectX::XMFLOAT3 rotation;
        DirectX::XMFLOAT3 scale;
        float time = 0.0f;
        float normalizedTime = 0.0f;
        float alpha = 1.0f;
        bool active = true;
    };

    float Lerp(float start_, float end_, float amount_)
    {
        return start_ + (end_ - start_) * amount_;
    }

    DirectX::XMFLOAT3 Lerp(
        const DirectX::XMFLOAT3& start_,
        const DirectX::XMFLOAT3& end_,
        float amount_)
    {
        return
        {
            Lerp(start_.x, end_.x, amount_),
            Lerp(start_.y, end_.y, amount_),
            Lerp(start_.z, end_.z, amount_),
        };
    }

    PartAnimationState CalculatePartAnimation(
        const MeshVFXPart& part_,
        const MeshVFXComponent& component_,
        float componentTime_)
    {
        PartAnimationState state_{};
        state_.rotation = part_.rotation;
        state_.scale = part_.scale;
        state_.alpha = part_.alpha;

        float normalizedTime_ = 0.0f;
        if (part_.duration > 0.0f)
        {
            if (!part_.loop && componentTime_ >= part_.duration)
            {
                state_.active = false;
                return state_;
            }
            state_.time = part_.loop
                ? std::fmod((std::max)(componentTime_, 0.0f), part_.duration)
                : std::clamp(componentTime_, 0.0f, part_.duration);
            normalizedTime_ = state_.time / part_.duration;
            state_.normalizedTime = normalizedTime_;
        }
        else
        {
            state_.time = (std::max)(componentTime_, 0.0f);
        }

        state_.rotation.x += part_.meshRotationSpeed.x * state_.time;
        state_.rotation.y += part_.meshRotationSpeed.y * state_.time;
        state_.rotation.z += part_.meshRotationSpeed.z * state_.time;
        if (part_.animateScale) state_.scale = Lerp(part_.scaleStart, part_.scaleEnd, normalizedTime_);
        if (part_.animateAlpha) state_.alpha *= Lerp(part_.alphaStart, part_.alphaEnd, normalizedTime_);
        const float scaleCurveValue_ = component_.EvaluateCurveValue(part_.scaleCurveName, normalizedTime_, 1.0f);
        state_.scale.x *= scaleCurveValue_;
        state_.scale.y *= scaleCurveValue_;
        state_.scale.z *= scaleCurveValue_;
        state_.alpha *= component_.EvaluateCurveValue(part_.alphaCurveName, normalizedTime_, 1.0f);
        return state_;
    }

    MeshVFXPart ApplyPartTimeCurves(
        const MeshVFXPart& part_,
        const MeshVFXComponent& component_,
        float normalizedTime_)
    {
        MeshVFXPart result_ = part_;
        result_.dissolveThreshold = component_.EvaluateCurveValue(
            part_.dissolveCurveName,
            normalizedTime_,
            part_.dissolveThreshold);
        result_.emissiveStrength *= component_.EvaluateCurveValue(
            part_.emissiveCurveName,
            normalizedTime_,
            1.0f);

        const float uvScrollCurveValue_ = component_.EvaluateCurveValue(
            part_.uvScrollCurveName,
            normalizedTime_,
            1.0f);
        result_.baseUVSpeed.x *= uvScrollCurveValue_;
        result_.baseUVSpeed.y *= uvScrollCurveValue_;
        result_.maskUVSpeed.x *= uvScrollCurveValue_;
        result_.maskUVSpeed.y *= uvScrollCurveValue_;
        result_.noiseUVSpeed.x *= uvScrollCurveValue_;
        result_.noiseUVSpeed.y *= uvScrollCurveValue_;

        const float uvRotationCurveValue_ = component_.EvaluateCurveValue(
            part_.uvRotationCurveName,
            normalizedTime_,
            1.0f);
        result_.baseUVRotationSpeed *= uvRotationCurveValue_;
        result_.maskUVRotationSpeed *= uvRotationCurveValue_;
        result_.noiseUVRotationSpeed *= uvRotationCurveValue_;
        return result_;
    }

    DirectX::XMMATRIX CreatePartTransform(
        const MeshVFXPart& part_,
        const PartAnimationState& animation_)
    {
        const DirectX::XMVECTOR rotation_ = DirectX::XMQuaternionRotationRollPitchYaw(
            DirectX::XMConvertToRadians(animation_.rotation.x),
            DirectX::XMConvertToRadians(animation_.rotation.y),
            DirectX::XMConvertToRadians(animation_.rotation.z));
        return DirectX::XMMatrixScaling(animation_.scale.x, animation_.scale.y, animation_.scale.z) *
            DirectX::XMMatrixRotationQuaternion(rotation_) *
            DirectX::XMMatrixTranslation(part_.position.x, part_.position.y, part_.position.z);
    }

    DirectX::XMMATRIX CreateNodeTransform(const GltfModelCommponennt::Node* node_)
    {
        DirectX::XMMATRIX transform_ = DirectX::XMMatrixIdentity();
        const GltfModelCommponennt::Node* currentNode_ = node_;
        while (currentNode_)
        {
            const DirectX::XMMATRIX local_ =
                DirectX::XMMatrixScaling(currentNode_->scale.x, currentNode_->scale.y, currentNode_->scale.z) *
                DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&currentNode_->rotation)) *
                DirectX::XMMatrixTranslation(
                    currentNode_->position.x, currentNode_->position.y, currentNode_->position.z);
            transform_ *= local_;
            currentNode_ = currentNode_->parent;
        }
        return transform_;
    }
}

bool MeshVFXTextureMixer::Initialize(ID3D11Device* device_)
{
    // 隍・焚Texture繧竪PU荳翫〒蜷域・縺吶ｋ縺溘ａ縺ｮ蜈ｨ逕ｻ髱｢Shader縺ｨ荳譎３T繧呈ｺ門ｙ縺吶ｋ縲・
    if (!device_) return false;
    if (FAILED(CPUResourceLoader::LoadVertexShader(
        device_,
        "Shader/FullScreenVS.cso",
        nullptr,
        0,
        nullptr,
        fullScreenVertexShader.ReleaseAndGetAddressOf()))) return false;
    if (FAILED(CPUResourceLoader::LoadPixelShader(
        device_,
        "Shader/MeshVFXTextureMixPS.cso",
        mixPixelShader.ReleaseAndGetAddressOf()))) return false;
    if (FAILED(CPUResourceLoader::CreateConstantBuffer(
        device_,
        sizeof(MixConstants),
        constantBuffer.ReleaseAndGetAddressOf()))) return false;
    return CreateTarget(device_, targets[0]) && CreateTarget(device_, targets[1]);
}

bool MeshVFXTextureMixer::CreateTarget(ID3D11Device* device_, Target& target_)
{
    // Mask/Noise蜷域・邨先棡繧剃ｿ晄戟縺吶ｋ1024x1024縺ｮ荳譎５exture繧剃ｽ懊ｋ縲・
    D3D11_TEXTURE2D_DESC description_{};
    description_.Width = 1024;
    description_.Height = 1024;
    description_.MipLevels = 1;
    description_.ArraySize = 1;
    description_.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    description_.SampleDesc.Count = 1;
    description_.Usage = D3D11_USAGE_DEFAULT;
    description_.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(device_->CreateTexture2D(&description_, nullptr, target_.texture.ReleaseAndGetAddressOf()))) return false;
    if (FAILED(device_->CreateRenderTargetView(
        target_.texture.Get(),
        nullptr,
        target_.renderTargetView.ReleaseAndGetAddressOf()))) return false;
    return SUCCEEDED(device_->CreateShaderResourceView(
        target_.texture.Get(),
        nullptr,
        target_.shaderResourceView.ReleaseAndGetAddressOf()));
}

ID3D11ShaderResourceView* MeshVFXTextureMixer::Mix(
    const RenderContext& renderContext_,
    const std::vector<ID3D11ShaderResourceView*>& textures_,
    const std::vector<MeshVFXMaskBlendMode>* blendModes_,
    const std::vector<bool>* invertLayers_,
    bool average_)
{
    // Texture縺・譫壹□縺代↑繧牙粋謌舌○縺壹◎縺ｮ縺ｾ縺ｾ霑斐＠縲∬､・焚譫壹・蝣ｴ蜷医□縺善ing-Pong蜷域・縺吶ｋ縲・
    if (textures_.empty() || !renderContext_.deviceContext || !renderContext_.renderState) return nullptr;
    if (textures_.size() == 1 && (!invertLayers_ || invertLayers_->empty() || !(*invertLayers_)[0]))
    {
        return textures_[0];
    }

    ID3D11DeviceContext* deviceContext_ = renderContext_.deviceContext;
    D3D11_VIEWPORT previousViewport_{};
    UINT previousViewportCount_ = 1;
    deviceContext_->RSGetViewports(&previousViewportCount_, &previousViewport_);
    ID3D11SamplerState* sampler_ = renderContext_.renderState->GetSamplerState(SamplerState::LinearClamp);
    deviceContext_->PSSetSamplers(0, 1, &sampler_);
    deviceContext_->OMSetDepthStencilState(
        renderContext_.renderState->GetDepthStencilState(DepthState::NoTestNoWrite),
        0);
    deviceContext_->OMSetBlendState(
        renderContext_.renderState->GetBlendState(BlendState::Opaque),
        nullptr,
        0xFFFFFFFF);
    deviceContext_->RSSetState(
        renderContext_.renderState->GetRasterizerState(RasterizerState::SolidCullNone));
    deviceContext_->IASetInputLayout(nullptr);
    deviceContext_->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
    deviceContext_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    deviceContext_->VSSetShader(fullScreenVertexShader.Get(), nullptr, 0);
    deviceContext_->PSSetShader(mixPixelShader.Get(), nullptr, 0);
    deviceContext_->PSSetConstantBuffers(0, 1, constantBuffer.GetAddressOf());

    D3D11_VIEWPORT viewport_{};
    viewport_.Width = 1024.0f;
    viewport_.Height = 1024.0f;
    viewport_.MaxDepth = 1.0f;
    deviceContext_->RSSetViewports(1, &viewport_);

    int outputIndex_ = 0;
    ID3D11ShaderResourceView* currentTexture_ = textures_[0];
    for (size_t layerIndex_ = 0; layerIndex_ < textures_.size(); ++layerIndex_)
    {
        // 1Layer縺壹▽迴ｾ蝨ｨ邨先棡縺ｨ谺｡Texture繧貞粋謌舌＠縲∝・蜉帛・繧剃ｺ､莠偵↓蛻・ｊ譖ｿ縺医ｋ縲・
        MixConstants constants_{};
        constants_.blendMode = layerIndex_ == 0
            ? 2u
            : (average_ ? 2u : static_cast<uint32_t>((*blendModes_)[layerIndex_]));
        constants_.invertLayer =
            invertLayers_ && layerIndex_ < invertLayers_->size() && (*invertLayers_)[layerIndex_] ? 1u : 0u;
        constants_.blendWeight = average_ ? 1.0f / static_cast<float>(layerIndex_ + 1) : 1.0f;
        deviceContext_->UpdateSubresource(constantBuffer.Get(), 0, nullptr, &constants_, 0, 0);

        ID3D11RenderTargetView* renderTargetView_ = targets[outputIndex_].renderTargetView.Get();
        deviceContext_->OMSetRenderTargets(1, &renderTargetView_, nullptr);
        ID3D11ShaderResourceView* resources_[] = { currentTexture_, textures_[layerIndex_] };
        deviceContext_->PSSetShaderResources(0, _countof(resources_), resources_);
        deviceContext_->Draw(3, 0);
        ID3D11ShaderResourceView* nullResources_[] = { nullptr, nullptr };
        deviceContext_->PSSetShaderResources(0, _countof(nullResources_), nullResources_);
        currentTexture_ = targets[outputIndex_].shaderResourceView.Get();
        outputIndex_ = 1 - outputIndex_;
    }
    if (previousViewportCount_ > 0)
    {
        deviceContext_->RSSetViewports(1, &previousViewport_);
    }
    return currentTexture_;
}

bool MeshVFXRenderSystem::Initialize(ID3D11Device* device_)
{
    // Shader譛ｬ菴薙→縲｀ask/Noise逕ｨTextureMixer繧偵∪縺ｨ繧√※蛻晄悄蛹悶☆繧九・
    shader = std::make_unique<MeshVFXShader>();
    isInitialized =
        shader->Initialize(device_) &&
        maskTextureMixer.Initialize(device_) &&
        noiseTextureMixer.Initialize(device_);
    if (!isInitialized)
    {
        shader.reset();
    }
    return isInitialized;
}

bool MeshVFXRenderSystem::EnsureAfterimageBuffers(
    ID3D11Device* device_,
    UINT instanceCapacity_)
{
    // Afterimage縺ｮ謨ｰ縺悟｢励∴縺溘→縺阪□縺代∝ｿ・ｦ∵焚莉･荳翫・StructuredBuffer縺ｸ諡｡蠑ｵ縺吶ｋ縲・
    if (!device_ || instanceCapacity_ == 0) return false;
    if (afterimageInstanceCapacity >= instanceCapacity_ &&
        afterimageBoneBuffer && afterimageInstanceBuffer)
    {
        return true;
    }

    UINT newCapacity_ = 1;
    while (newCapacity_ < instanceCapacity_) newCapacity_ *= 2;

    auto createStructuredBuffer_ = [device_](
        UINT elementCount_, UINT stride_,
        Microsoft::WRL::ComPtr<ID3D11Buffer>& buffer_,
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& shaderResourceView_)
        {
            D3D11_BUFFER_DESC bufferDesc_{};
            bufferDesc_.ByteWidth = elementCount_ * stride_;
            bufferDesc_.Usage = D3D11_USAGE_DYNAMIC;
            bufferDesc_.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            bufferDesc_.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            bufferDesc_.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
            bufferDesc_.StructureByteStride = stride_;
            if (FAILED(device_->CreateBuffer(&bufferDesc_, nullptr, buffer_.ReleaseAndGetAddressOf())))
            {
                return false;
            }

            D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc_{};
            viewDesc_.Format = DXGI_FORMAT_UNKNOWN;
            viewDesc_.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
            viewDesc_.Buffer.FirstElement = 0;
            viewDesc_.Buffer.NumElements = elementCount_;
            return SUCCEEDED(device_->CreateShaderResourceView(
                buffer_.Get(), &viewDesc_, shaderResourceView_.ReleaseAndGetAddressOf()));
        };

    if (!createStructuredBuffer_(
        newCapacity_ * 256,
        sizeof(DirectX::XMFLOAT4X4),
        afterimageBoneBuffer,
        afterimageBoneSRV))
    {
        return false;
    }
    if (!createStructuredBuffer_(
        newCapacity_,
        sizeof(MeshVFXTextureMixer::AfterimageInstanceData),
        afterimageInstanceBuffer,
        afterimageInstanceSRV))
    {
        return false;
    }

    afterimageInstanceCapacity = newCapacity_;
    return true;
}

UINT MeshVFXRenderSystem::UpdateAfterimageBuffers(
    const RenderContext& renderContext_,
    const std::vector<const MeshVFXModelPoseView*>& poses_,
    size_t meshIndex_)
{
    // 隍・焚Pose縺ｮ繝懊・繝ｳ陦悟・縺ｨAlpha/Time繧棚nstancing逕ｨBuffer縺ｸ隧ｰ繧√ｋ縲・
    if (poses_.empty() || !renderContext_.deviceContext)
    {
        return 0;
    }
    ID3D11Device* device_ = renderContext_.device
        ? renderContext_.device
        : GraphicsDeviceManager::Instance().GetDevice();
    if (!EnsureAfterimageBuffers(
        device_,
        static_cast<UINT>(poses_.size())))
    {
        return 0;
    }

    D3D11_MAPPED_SUBRESOURCE mappedBones_{};
    if (FAILED(renderContext_.deviceContext->Map(
        afterimageBoneBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedBones_)))
    {
        return 0;
    }
    auto* destinationBones_ = static_cast<DirectX::XMFLOAT4X4*>(mappedBones_.pData);
    const size_t activeBoneCapacity_ = poses_.size() * 256;
    std::memset(
        destinationBones_, 0,
        sizeof(DirectX::XMFLOAT4X4) * activeBoneCapacity_);
    for (size_t poseIndex_ = 0; poseIndex_ < poses_.size(); ++poseIndex_)
    {
        const MeshVFXModelPoseView* pose_ = poses_[poseIndex_];
        if (!pose_ || !pose_->meshTransforms ||
            meshIndex_ >= pose_->meshTransforms->size()) continue;
        const auto& transforms_ = (*pose_->meshTransforms)[meshIndex_];
        const size_t transformCount_ = (std::min)(transforms_.size(), static_cast<size_t>(256));
        std::memcpy(
            destinationBones_ + poseIndex_ * 256,
            transforms_.data(),
            sizeof(DirectX::XMFLOAT4X4) * transformCount_);
    }
    renderContext_.deviceContext->Unmap(afterimageBoneBuffer.Get(), 0);

    D3D11_MAPPED_SUBRESOURCE mappedInstances_{};
    if (FAILED(renderContext_.deviceContext->Map(
        afterimageInstanceBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedInstances_)))
    {
        return 0;
    }
    auto* destinationInstances_ = static_cast<MeshVFXTextureMixer::AfterimageInstanceData*>(mappedInstances_.pData);
    for (size_t poseIndex_ = 0; poseIndex_ < poses_.size(); ++poseIndex_)
    {
        const MeshVFXModelPoseView* pose_ = poses_[poseIndex_];
        destinationInstances_[poseIndex_].time = pose_ ? pose_->time : 0.0f;
        destinationInstances_[poseIndex_].alpha = pose_ ? pose_->alpha : 0.0f;
    }
    renderContext_.deviceContext->Unmap(afterimageInstanceBuffer.Get(), 0);

    ID3D11ShaderResourceView* resources_[] =
    {
        afterimageBoneSRV.Get(),
        afterimageInstanceSRV.Get(),
    };
    renderContext_.deviceContext->VSSetShaderResources(5, _countof(resources_), resources_);
    return static_cast<UINT>(poses_.size());
}

void MeshVFXRenderSystem::PushSubmit(MeshVFXComponent* component_)
{
    // 謠冗判蜿ｯ閭ｽ縺ｪComponent縺縺代ｒ莉翫ヵ繝ｬ繝ｼ繝縺ｮRenderAll蟇ｾ雎｡縺ｸ遨阪・縲・
    if (!isInitialized || !component_ || !component_->IsEnabled()) return;

    if (component_->UsesOwnerModel())
    {
        const std::shared_ptr<GameObject> owner_ = component_->GetGameObject();
        if (component_->UsesModelPoses())
        {
            if (component_->HasModelPoses()) components.push_back(component_);
            return;
        }
        if (owner_ && owner_->GetComponent<GltfModelCommponennt>())
        {
            components.push_back(component_);
        }
        return;
    }

    MeshVFXDynamicMesh dynamicMesh_{};
    if (component_->GetDynamicMesh(dynamicMesh_))
    {
        components.push_back(component_);
        return;
    }

    const auto& runtimeParts_ = component_->GetRuntimeParts();
    const bool hasRenderablePart_ = std::any_of(
        runtimeParts_.begin(),
        runtimeParts_.end(),
        [](const MeshVFXRuntimePart& runtimePart_)
        {
            return runtimePart_.modelResource != nullptr;
        });
    if (!hasRenderablePart_) return;

    components.push_back(component_);
}

bool MeshVFXRenderSystem::EnsureCopyTexture(
    ID3D11Device* device_,
    ID3D11ShaderResourceView* source_,
    Microsoft::WRL::ComPtr<ID3D11Texture2D>& texture_,
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& shaderResourceView_)
{
    // Scene Color/Depth繧担hader縺九ｉSample縺吶ｋ縺溘ａ縲√し繧､繧ｺ縺ｨ蠖｢蠑上′蜷医≧繧ｳ繝斐・Texture繧堤｢ｺ菫昴☆繧九・
    if (!device_ || !source_) return false;

    Microsoft::WRL::ComPtr<ID3D11Resource> sourceResource_;
    source_->GetResource(sourceResource_.GetAddressOf());
    Microsoft::WRL::ComPtr<ID3D11Texture2D> sourceTexture_;
    if (FAILED(sourceResource_.As(&sourceTexture_))) return false;

    D3D11_TEXTURE2D_DESC sourceDescription_{};
    sourceTexture_->GetDesc(&sourceDescription_);

    bool recreate_ = !texture_ || !shaderResourceView_;
    if (texture_)
    {
        D3D11_TEXTURE2D_DESC currentDescription_{};
        texture_->GetDesc(&currentDescription_);
        recreate_ =
            currentDescription_.Width != sourceDescription_.Width ||
            currentDescription_.Height != sourceDescription_.Height ||
            currentDescription_.Format != sourceDescription_.Format ||
            currentDescription_.SampleDesc.Count != sourceDescription_.SampleDesc.Count;
    }

    if (recreate_)
    {
        texture_.Reset();
        shaderResourceView_.Reset();
        sourceDescription_.Usage = D3D11_USAGE_DEFAULT;
        sourceDescription_.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        sourceDescription_.CPUAccessFlags = 0;
        sourceDescription_.MiscFlags = 0;
        if (FAILED(device_->CreateTexture2D(
            &sourceDescription_,
            nullptr,
            texture_.ReleaseAndGetAddressOf())))
        {
            return false;
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC viewDescription_{};
        source_->GetDesc(&viewDescription_);
        if (FAILED(device_->CreateShaderResourceView(
            texture_.Get(),
            &viewDescription_,
            shaderResourceView_.ReleaseAndGetAddressOf())))
        {
            return false;
        }
    }

    return true;
}

bool MeshVFXRenderSystem::PrepareSceneSampling(
    const RenderContext& renderContext_,
    bool isEditorWindow_,
    ID3D11RenderTargetView* activeRenderTarget_,
    ID3D11DepthStencilView* activeDepthStencil_)
{
    // 閭梧勹豁ｪ縺ｿ繧・ｷｱ蠎ｦ莠､蟾ｮEdge縺ｧ菴ｿ縺・◆繧√∫樟蝨ｨ縺ｮScene Color/Depth繧呈緒逕ｻ蜑阪↓騾驕ｿ縺吶ｋ縲・
    ID3D11Device* device_ = renderContext_.device
        ? renderContext_.device
        : GraphicsDeviceManager::Instance().GetDevice();
    ID3D11DeviceContext* deviceContext_ = renderContext_.deviceContext;
    if (!device_ || !deviceContext_ || !renderContext_.depthSRV)
    {
        return false;
    }
    if (!EnsureCopyTexture(device_, renderContext_.depthSRV, sceneDepthCopy, sceneDepthCopySRV))
    {
        return false;
    }

    deviceContext_->OMSetRenderTargets(0, nullptr, nullptr);

    Microsoft::WRL::ComPtr<ID3D11Resource> depthSource_;
    renderContext_.depthSRV->GetResource(depthSource_.GetAddressOf());
    deviceContext_->CopyResource(sceneDepthCopy.Get(), depthSource_.Get());

    if (isEditorWindow_ && activeRenderTarget_)
    {
        deviceContext_->OMSetRenderTargets(1, &activeRenderTarget_, activeDepthStencil_);
    }
    else
    {
        GraphicsDeviceManager::Instance().SetParticleRenderTarget();
    }
    return true;
}

void MeshVFXRenderSystem::RenderAll(
    const RenderContext& renderContext_,
    bool isEditorWindow_,
    ID3D11RenderTargetView* activeRenderTarget_,
    ID3D11DepthStencilView* activeDepthStencil_)
{
    BS_PROFILE_SCOPE("Render::VFX");
    // PushSubmit縺輔ｌ縺溷・MeshVFX繧偵・壼ｸｸMesh繝ｻ蜍慕噪Mesh繝ｻAfterimage Pose縺ｮ3邉ｻ邨ｱ縺ｫ蛻・￠縺ｦ謠冗判縺吶ｋ縲・
    if (!isInitialized || !shader || !renderContext_.deviceContext || !renderContext_.camera ||
        !renderContext_.renderState || components.empty())
    {
        components.clear();
        return;
    }

    ID3D11DeviceContext* deviceContext_ = renderContext_.deviceContext;
    deviceContext_->OMSetDepthStencilState(
        renderContext_.renderState->GetDepthStencilState(DepthState::TestOnly), 0);
    deviceContext_->RSSetState(
        renderContext_.renderState->GetRasterizerState(RasterizerState::SolidCullNone));
    ID3D11SamplerState* samplers_[] =
    {
        renderContext_.renderState->GetSamplerState(SamplerState::LinearWrap),
        renderContext_.renderState->GetSamplerState(SamplerState::LinearBorder),
        renderContext_.renderState->GetSamplerState(SamplerState::LinearClamp),
    };
    deviceContext_->PSSetSamplers(0, _countof(samplers_), samplers_);

    const bool needsSceneSampling_ = std::any_of(
        components.begin(),
        components.end(),
        [](const MeshVFXComponent* component_)
        {
            if (!component_) return false;
            const auto& parts_ = component_->GetAsset().parts;
            return std::any_of(
                parts_.begin(),
                parts_.end(),
                [](const MeshVFXPart& part_)
                {
                    return part_.renderEnabled &&
                        part_.useEdgeHighlight &&
                        part_.intersectionEdgeStrength > 0.0f;
                });
        });
    // 蠢・ｦ√↑Part縺後≠繧句ｴ蜷医□縺全cene Color/Depth繧偵さ繝斐・縺励∽ｸ崎ｦ√↑繝輔Ξ繝ｼ繝縺ｮ繧ｳ繧ｹ繝医ｒ驕ｿ縺代ｋ縲・
    const bool sceneSamplingReady_ = needsSceneSampling_ && PrepareSceneSampling(
        renderContext_,
        isEditorWindow_,
        activeRenderTarget_,
        activeDepthStencil_);
    const auto restoreMeshTarget_ = [&](bool useDistortion_)
        {
            if (isEditorWindow_ && activeRenderTarget_)
            {
                deviceContext_->OMSetRenderTargets(1, &activeRenderTarget_, activeDepthStencil_);
                return;
            }
            if (useDistortion_)
            {
                GraphicsDeviceManager::Instance().SetParticleRenderTargetWithDistortion();
            }
            else
            {
                GraphicsDeviceManager::Instance().SetParticleRenderTarget();
            }
        };
    for (MeshVFXComponent* component_ : components)
    {
        const std::shared_ptr<GameObject> owner_ = component_->GetGameObject();
        if (!owner_ || !owner_->transform) continue;

        const MeshVFXAsset& asset_ = component_->GetAsset();
        const std::vector<MeshVFXRuntimePart>& runtimeParts_ = component_->GetRuntimeParts();
        const size_t partCount_ = (std::min)(
            (std::min)(asset_.parts.size(), runtimeParts_.size()),
            component_->GetMaxRenderParts());
        std::vector<MeshVFXModelPoseView> modelPoses_;
        const bool hasModelPoses_ =
            component_->GetModelPoseViews(modelPoses_) && !modelPoses_.empty();

        for (size_t partIndex_ = 0; partIndex_ < partCount_; ++partIndex_)
        {
            const MeshVFXPart& part_ = asset_.parts[partIndex_];
            const MeshVFXRuntimePart& runtimePart_ = runtimeParts_[partIndex_];
            const bool useOwnerModel_ = component_->UsesOwnerModel();
            const std::shared_ptr<GltfModelCommponennt> ownerModel_ =
                useOwnerModel_ && !component_->UsesModelPoses()
                ? owner_->GetComponent<GltfModelCommponennt>()
                : nullptr;
            MeshVFXDynamicMesh dynamicMesh_{};
            const bool useDynamicMesh_ = component_->GetDynamicMesh(dynamicMesh_);
            if (!part_.renderEnabled ||
                (useOwnerModel_ ? (!ownerModel_ && !hasModelPoses_) :
                    (!useDynamicMesh_ && !runtimePart_.modelResource))) continue;

            const PartAnimationState animation_ =
                CalculatePartAnimation(part_, *component_, component_->GetTime());
            if (!animation_.active) continue;
            const MeshVFXPart evaluatedPart_ = ApplyPartTimeCurves(
                part_,
                *component_,
                animation_.normalizedTime);

            const bool partUsesDistortion_ =
                !isEditorWindow_ &&
                evaluatedPart_.useMaskDistortion &&
                runtimePart_.distortionNoiseTexture;
            restoreMeshTarget_(false);

            const BlendState blendState_ = part_.blendMode == MeshVFXBlendMode::ADDITIVE
                ? BlendState::AdditiveKeepAlpha
                : (isEditorWindow_ ? BlendState::TransparencyKeepAlpha : BlendState::Transparency);
            deviceContext_->OMSetBlendState(
                renderContext_.renderState->GetBlendState(blendState_), nullptr, 0xFFFFFFFF);

            const auto renderMesh_ = [&](ID3D11Buffer* vertexBuffer_, ID3D11Buffer* indexBuffer_,
                UINT indexCount_, UINT stride_, const DirectX::XMMATRIX& world_,
                const GltfModelCommponennt::Mesh* sourceMesh_,
                const std::vector<DirectX::XMFLOAT4X4>* poseTransforms_,
                float poseTime_, float poseAlpha_,
                UINT instanceCount_, bool useInstancedPose_)
            {
                // Mesh1縺､蛻・・Texture隗｣豎ｺ縲ヾhader譖ｴ譁ｰ縲〃ertex/Index險ｭ螳壹．raw蜻ｼ縺ｳ蜃ｺ縺励ｒ蜈ｱ騾壼喧縺吶ｋ縲・
                if (!vertexBuffer_ || !indexBuffer_ || indexCount_ == 0) return;

                ID3D11ShaderResourceView* baseTexture_ = runtimePart_.baseTexture.Get();
                if (part_.useMaterialBaseTexture && part_.baseTexturePath.empty() &&
                    sourceMesh_ && sourceMesh_->material && sourceMesh_->material->baseMap)
                {
                    baseTexture_ = sourceMesh_->material->baseMap.Get();
                }
                ID3D11ShaderResourceView* normalTexture_ = runtimePart_.normalTexture.Get();
                if (part_.useMaterialNormalTexture && part_.normalTexturePath.empty() &&
                    sourceMesh_ && sourceMesh_->material && sourceMesh_->material->normalMap)
                {
                    normalTexture_ = sourceMesh_->material->normalMap.Get();
                }

                std::vector<ID3D11ShaderResourceView*> maskTextures_;
                std::vector<MeshVFXMaskBlendMode> maskBlendModes_;
                std::vector<bool> invertMaskLayers_;
                for (size_t layerIndex_ = 0;
                    layerIndex_ < runtimePart_.maskTextures.size() && layerIndex_ < part_.maskLayers.size();
                    ++layerIndex_)
                {
                    maskTextures_.push_back(runtimePart_.maskTextures[layerIndex_].Get());
                    maskBlendModes_.push_back(part_.maskLayers[layerIndex_].blendMode);
                    invertMaskLayers_.push_back(part_.maskLayers[layerIndex_].invert);
                }
                if (maskTextures_.empty() && !runtimePart_.maskTextures.empty())
                {
                    maskTextures_.push_back(runtimePart_.maskTextures.front().Get());
                    maskBlendModes_.push_back(MeshVFXMaskBlendMode::MULTIPLY);
                    invertMaskLayers_.push_back(false);
                }
                std::vector<ID3D11ShaderResourceView*> noiseTextures_;
                for (const auto& texture_ : runtimePart_.noiseTextures)
                {
                    noiseTextures_.push_back(texture_.Get());
                }


                ID3D11ShaderResourceView* maskTexture_ = maskTextureMixer.Mix(
                    renderContext_, maskTextures_, &maskBlendModes_, &invertMaskLayers_, false);
                ID3D11ShaderResourceView* noiseTexture_ = noiseTextureMixer.Mix(
                    renderContext_, noiseTextures_, nullptr, nullptr, true);

                const MeshVFXPart* shaderPart_ = &evaluatedPart_;
                MeshVFXPart depthSafePart_;
                if (!sceneSamplingReady_ && evaluatedPart_.intersectionEdgeStrength > 0.0f)
                {
                    depthSafePart_ = evaluatedPart_;
                    depthSafePart_.intersectionEdgeStrength = 0.0f;
                    shaderPart_ = &depthSafePart_;
                }

                restoreMeshTarget_(false);
                deviceContext_->OMSetBlendState(
                    renderContext_.renderState->GetBlendState(blendState_), nullptr, 0xFFFFFFFF);
                deviceContext_->OMSetDepthStencilState(
                    renderContext_.renderState->GetDepthStencilState(DepthState::TestOnly), 0);
                deviceContext_->RSSetState(
                    renderContext_.renderState->GetRasterizerState(RasterizerState::SolidCullNone));
                deviceContext_->PSSetSamplers(0, _countof(samplers_), samplers_);
                shader->Begin(
                    renderContext_,
                    nullptr,
                    sceneSamplingReady_ ? sceneDepthCopySRV.Get() : nullptr,
                    useOwnerModel_, useInstancedPose_);
                shader->UpdateScene(renderContext_);
                if (!useInstancedPose_ && useOwnerModel_ && poseTransforms_)
                {
                    shader->UpdateSkeleton(renderContext_, *poseTransforms_);
                }
                else if (!useInstancedPose_ && useOwnerModel_ && sourceMesh_)
                {
                    shader->UpdateSkeleton(renderContext_, *sourceMesh_);
                }
                shader->UpdatePart(
                    renderContext_, *shaderPart_, runtimePart_, baseTexture_, normalTexture_,
                    maskTexture_, noiseTexture_,
                    world_, poseTime_, animation_.alpha * poseAlpha_);

                UINT offset_ = 0;
                deviceContext_->IASetVertexBuffers(0, 1, &vertexBuffer_, &stride_, &offset_);
                deviceContext_->IASetIndexBuffer(indexBuffer_, DXGI_FORMAT_R32_UINT, 0);
                deviceContext_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                if (useInstancedPose_)
                {
                    deviceContext_->DrawIndexedInstanced(indexCount_, instanceCount_, 0, 0, 0);
                }
                else
                {
                    deviceContext_->DrawIndexed(indexCount_, 0, 0);
                }

                if (partUsesDistortion_)
                {
                    GraphicsDeviceManager::Instance().SetSceneDistortionRenderTarget();
                    deviceContext_->OMSetBlendState(
                        renderContext_.renderState->GetBlendState(BlendState::Transparency), nullptr, 0xFFFFFFFF);
                    deviceContext_->OMSetDepthStencilState(
                        renderContext_.renderState->GetDepthStencilState(DepthState::TestOnly), 0);
                    deviceContext_->RSSetState(
                        renderContext_.renderState->GetRasterizerState(RasterizerState::SolidCullNone));
                    deviceContext_->PSSetSamplers(0, _countof(samplers_), samplers_);
                    shader->BeginDistortion(
                        renderContext_,
                        useOwnerModel_, useInstancedPose_);
                    shader->UpdateScene(renderContext_);
                    if (!useInstancedPose_ && useOwnerModel_ && poseTransforms_)
                    {
                        shader->UpdateSkeleton(renderContext_, *poseTransforms_);
                    }
                    else if (!useInstancedPose_ && useOwnerModel_ && sourceMesh_)
                    {
                        shader->UpdateSkeleton(renderContext_, *sourceMesh_);
                    }
                    shader->UpdatePart(
                        renderContext_, *shaderPart_, runtimePart_, baseTexture_, normalTexture_,
                        maskTexture_, noiseTexture_,
                        world_, poseTime_, animation_.alpha * poseAlpha_);

                    deviceContext_->IASetVertexBuffers(0, 1, &vertexBuffer_, &stride_, &offset_);
                    deviceContext_->IASetIndexBuffer(indexBuffer_, DXGI_FORMAT_R32_UINT, 0);
                    deviceContext_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                    if (useInstancedPose_)
                    {
                        deviceContext_->DrawIndexedInstanced(indexCount_, instanceCount_, 0, 0, 0);
                    }
                    else
                    {
                        deviceContext_->DrawIndexed(indexCount_, 0, 0);
                    }
                }
            };

            if (useDynamicMesh_)
            {
                // Trail縺ｪ縺ｩ縺ｮ螳溯｡梧凾逕滓・Mesh縺ｯ縲，omponent蛛ｴ縺梧戟縺､Buffer繧偵◎縺ｮ縺ｾ縺ｾ謠冗判縺吶ｋ縲・
                renderMesh_(dynamicMesh_.vertexBuffer, dynamicMesh_.indexBuffer,
                    dynamicMesh_.indexCount, dynamicMesh_.vertexStride,
                    DirectX::XMMatrixIdentity(), nullptr, nullptr,
                    animation_.time, 1.0f, 1, false);
                continue;
            }

            if (hasModelPoses_)
            {
                // Afterimage縺ｯ蜷後§Mesh繧定､・焚Pose縺ｧInstancing謠冗判縺励．rawCall繧呈椛縺医ｋ縲・
                std::unordered_map<
                    GltfModelCommponennt*,
                    std::vector<const MeshVFXModelPoseView*>> poseGroups_;
                for (const MeshVFXModelPoseView& pose_ : modelPoses_)
                {
                    if (!pose_.model) continue;
                    poseGroups_[pose_.model.get()].push_back(&pose_);
                }

                for (const auto& [model_, poses_] : poseGroups_)
                {
                    if (!model_ || poses_.empty()) continue;
                    const auto& meshes_ = model_->GetMeshes();
                    for (size_t meshIndex_ = 0; meshIndex_ < meshes_.size(); ++meshIndex_)
                    {
                        if (part_.meshIndex >= 0 && part_.meshIndex != static_cast<int>(meshIndex_)) continue;
                        const GltfModelCommponennt::Mesh& mesh_ = meshes_[meshIndex_];
                        const UINT instanceCount_ = UpdateAfterimageBuffers(
                            renderContext_, poses_, meshIndex_);
                        if (instanceCount_ == 0) continue;
                        renderMesh_(mesh_.vertexBuffer.Get(), mesh_.indexBuffer.Get(),
                            static_cast<UINT>(mesh_.indices.size()),
                            sizeof(GltfModelCommponennt::Vertex),
                            DirectX::XMMatrixIdentity(), &mesh_, nullptr,
                            0.0f, 1.0f, instanceCount_, true);
                    }
                }
                continue;
            }

            const auto& meshes_ = useOwnerModel_ ? ownerModel_->GetMeshes() : runtimePart_.modelResource->meshes;
            for (size_t meshIndex_ = 0; meshIndex_ < meshes_.size(); ++meshIndex_)
            {
                // 騾壼ｸｸMeshVFX縺ｯPartTransform縺ｨOwnerTransform繧貞粋謌舌＠縺ｦ謠冗判縺吶ｋ縲・
                if (part_.meshIndex >= 0 && part_.meshIndex != static_cast<int>(meshIndex_)) continue;

                const GltfModelCommponennt::Mesh& mesh_ = meshes_[meshIndex_];
                const DirectX::XMMATRIX world_ = useOwnerModel_
                    ? DirectX::XMMatrixIdentity()
                    : CreateNodeTransform(mesh_.node) *
                        CreatePartTransform(part_, animation_) *
                        owner_->transform->GetWorldMatrix();
                renderMesh_(mesh_.vertexBuffer.Get(), mesh_.indexBuffer.Get(),
                    static_cast<UINT>(mesh_.indices.size()),
                    sizeof(GltfModelCommponennt::Vertex), world_, &mesh_, nullptr,
                    animation_.time, 1.0f, 1, false);
            }
        }
    }

    ID3D11ShaderResourceView* nullAfterimageResources_[2] = {};
    deviceContext_->VSSetShaderResources(5, _countof(nullAfterimageResources_), nullAfterimageResources_);
    shader->End(renderContext_);
    deviceContext_->OMSetBlendState(
        renderContext_.renderState->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);
    deviceContext_->OMSetDepthStencilState(
        renderContext_.renderState->GetDepthStencilState(DepthState::TestAndWrite), 0);
    deviceContext_->RSSetState(
        renderContext_.renderState->GetRasterizerState(RasterizerState::SolidCullBack));
    components.clear();
}

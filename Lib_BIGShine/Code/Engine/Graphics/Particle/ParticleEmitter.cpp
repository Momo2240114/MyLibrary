#include "PrecompiledHeader.h"

#include "ParticleEmitter.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include "Engine/Graphics/Core/CPUResourceLoader.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Debug/TracyProfiler.h"

Microsoft::WRL::ComPtr<ID3D11ComputeShader> ParticleEmitter::initShader;
Microsoft::WRL::ComPtr<ID3D11ComputeShader> ParticleEmitter::beginFrameShader;
Microsoft::WRL::ComPtr<ID3D11ComputeShader> ParticleEmitter::emitShader;
Microsoft::WRL::ComPtr<ID3D11ComputeShader> ParticleEmitter::updateShader;
Microsoft::WRL::ComPtr<ID3D11ComputeShader> ParticleEmitter::endFrameShader;
Microsoft::WRL::ComPtr<ID3D11ComputeShader> ParticleEmitter::sortB2Shader;
Microsoft::WRL::ComPtr<ID3D11ComputeShader> ParticleEmitter::sortC2Shader;
Microsoft::WRL::ComPtr<ID3D11VertexShader> ParticleEmitter::vertexShader;
Microsoft::WRL::ComPtr<ID3D11GeometryShader> ParticleEmitter::geometryShader;
Microsoft::WRL::ComPtr<ID3D11PixelShader> ParticleEmitter::pixelShader;

ParticleEmitter::ParticleEmitter(ID3D11Device* device, const ParticleSystemConstants& initialSettings, int maxParticles)
    : settings(initialSettings)
{
    // GPUのDispatch効率とBitonicSortの都合に合わせ、Particle数は安全な単位へ丸める。
    settings.maxParticleCount = static_cast<uint32_t>((std::max)(1, maxParticles));
    numParticles = RoundParticleCount(settings.maxParticleCount);
    numEmitParticles = (std::min<UINT>)(numParticles, 10000);
    emitParticles.reserve(numEmitParticles);

    InitializeSharedShaders(device);
    CreateGPUResources(device);
}

void ParticleEmitter::InitializeSharedShaders(ID3D11Device* device)
{
    // Shaderは全Emitterで同じものを使うため、初回だけ読み込む。
    if (!device || initShader) return;

    CPUResourceLoader::LoadComputeShader(device, "Shader/ParticleInit_CS.cso", initShader.GetAddressOf());
    CPUResourceLoader::LoadComputeShader(device, "Shader/ParticleBeginFrame_CS.cso", beginFrameShader.GetAddressOf());
    CPUResourceLoader::LoadComputeShader(device, "Shader/ParticleEmit_CS.cso", emitShader.GetAddressOf());
    CPUResourceLoader::LoadComputeShader(device, "Shader/ParticleUpdate_CS.cso", updateShader.GetAddressOf());
    CPUResourceLoader::LoadComputeShader(device, "Shader/ParticleEndFrame_CS.cso", endFrameShader.GetAddressOf());
    CPUResourceLoader::LoadComputeShader(device, "Shader/ParticleBitonicSortB2_CS.cso", sortB2Shader.GetAddressOf());
    CPUResourceLoader::LoadComputeShader(device, "Shader/ParticleBitonicSortC2_CS.cso", sortC2Shader.GetAddressOf());

    CPUResourceLoader::LoadVertexShader(device, "Shader/ParticleDraw_VS.cso", nullptr, 0, nullptr, vertexShader.GetAddressOf());
    CPUResourceLoader::LoadGeometryShader(device, "Shader/ParticleDraw_GS.cso", geometryShader.GetAddressOf());
    CPUResourceLoader::LoadPixelShader(device, "Shader/ParticleDraw_PS.cso", pixelShader.GetAddressOf());
}

void ParticleEmitter::BindSharedRenderShaders(ID3D11DeviceContext* context)
{
    context->VSSetShader(vertexShader.Get(), nullptr, 0);
    context->GSSetShader(geometryShader.Get(), nullptr, 0);
    context->PSSetShader(pixelShader.Get(), nullptr, 0);
}

void ParticleEmitter::CreateGPUResources(ID3D11Device* device)
{
    if (!device) return;

    // 各Compute Shader / Draw Shaderへ渡すConstantBufferを作る。
    D3D11_BUFFER_DESC cbDesc{};
    cbDesc.Usage = D3D11_USAGE_DEFAULT;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    cbDesc.ByteWidth = sizeof(ParticleSystemConstants);
    device->CreateBuffer(&cbDesc, nullptr, emitterConstantBuffer.GetAddressOf());

    cbDesc.ByteWidth = sizeof(CommonConstants);
    device->CreateBuffer(&cbDesc, nullptr, commonConstantBuffer.GetAddressOf());

    cbDesc.ByteWidth = sizeof(SortConstants);
    device->CreateBuffer(&cbDesc, nullptr, sortConstantBuffer.GetAddressOf());

    // Particle本体はCompute Shaderから読み書きするため、SRV/UAV両方を持つStructuredBufferにする。
    D3D11_BUFFER_DESC bufferDesc{};
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    bufferDesc.ByteWidth = sizeof(Particle) * numParticles;
    bufferDesc.StructureByteStride = sizeof(Particle);
    device->CreateBuffer(&bufferDesc, nullptr, particleDataBuffer.GetAddressOf());
    device->CreateShaderResourceView(particleDataBuffer.Get(), nullptr, particleDataSRV.GetAddressOf());
    device->CreateUnorderedAccessView(particleDataBuffer.Get(), nullptr, particleDataUAV.GetAddressOf());

    // 未使用Particle番号をAppend/Consumeすることで、死んだParticle枠を再利用する。
    bufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
    bufferDesc.ByteWidth = sizeof(UINT) * numParticles;
    bufferDesc.StructureByteStride = sizeof(UINT);
    device->CreateBuffer(&bufferDesc, nullptr, unusedIndexBuffer.GetAddressOf());

    D3D11_UNORDERED_ACCESS_VIEW_DESC appendUavDesc{};
    appendUavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    appendUavDesc.Format = DXGI_FORMAT_UNKNOWN;
    appendUavDesc.Buffer.NumElements = numParticles;
    appendUavDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_APPEND;
    device->CreateUnorderedAccessView(unusedIndexBuffer.Get(), &appendUavDesc, unusedIndexUAV.GetAddressOf());

    // CPUで決めた発生位置・速度をEmit_CSへまとめて渡す一時バッファ。
    bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    bufferDesc.ByteWidth = sizeof(EmitParticleData) * numEmitParticles;
    bufferDesc.StructureByteStride = sizeof(EmitParticleData);
    device->CreateBuffer(&bufferDesc, nullptr, emitBuffer.GetAddressOf());
    device->CreateShaderResourceView(emitBuffer.Get(), nullptr, emitSRV.GetAddressOf());

    // Headerは生存状態とDepthSortのための参照情報を持つ。
    bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    bufferDesc.ByteWidth = sizeof(ParticleHeader) * numParticles;
    bufferDesc.StructureByteStride = sizeof(ParticleHeader);
    device->CreateBuffer(&bufferDesc, nullptr, particleHeaderBuffer.GetAddressOf());
    device->CreateShaderResourceView(particleHeaderBuffer.Get(), nullptr, particleHeaderSRV.GetAddressOf());
    device->CreateUnorderedAccessView(particleHeaderBuffer.Get(), nullptr, particleHeaderUAV.GetAddressOf());

    // GPU側でDispatch/Draw数を決めるため、Indirect引数用バッファを用意する。
    D3D11_BUFFER_DESC indirectDesc{};
    indirectDesc.Usage = D3D11_USAGE_DEFAULT;
    indirectDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
    indirectDesc.MiscFlags = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS | D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
    indirectDesc.ByteWidth = DrawIndirectSize;

    std::vector<UINT> initialIndirect(indirectDesc.ByteWidth / sizeof(UINT), 0);
    auto* draw = reinterpret_cast<DrawIndirect*>(initialIndirect.data() + DrawIndirectOffset / sizeof(UINT));
    draw->vertexCountPerInstance = 0;
    draw->instanceCount = 1;
    draw->startVertexLocation = 0;
    draw->startInstanceLocation = 0;

    D3D11_SUBRESOURCE_DATA indirectData{};
    indirectData.pSysMem = initialIndirect.data();
    indirectData.SysMemPitch = indirectDesc.ByteWidth;
    device->CreateBuffer(&indirectDesc, &indirectData, indirectDataBuffer.GetAddressOf());

    D3D11_UNORDERED_ACCESS_VIEW_DESC rawUavDesc{};
    rawUavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    rawUavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    rawUavDesc.Buffer.NumElements = indirectDesc.ByteWidth / sizeof(UINT);
    rawUavDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
    device->CreateUnorderedAccessView(indirectDataBuffer.Get(), &rawUavDesc, indirectDataUAV.GetAddressOf());

    // Texture未設定でもShaderが安全にSampleできるよう、白Textureを初期値にする。
    CPUResourceLoader::CreateDummyTexture(device, 0xFFFFFFFFu, textureSRV.GetAddressOf());
    CPUResourceLoader::CreateDummyTexture(device, 0xFFFFFFFFu, maskTextureSRV.GetAddressOf());
    CPUResourceLoader::CreateDummyTexture(device, 0xFFFFFFFFu, gradientTextureSRV.GetAddressOf());
    CPUResourceLoader::CreateDummyTexture(device, 0xFFFFFFFFu, shadeGradientTextureSRV.GetAddressOf());
}

void ParticleEmitter::ApplySettings(const ParticleSystemConstants& nextSettings)
{
    // Editorから設定を差し替えても、再生中のEmitter時間は維持する。
    const float oldTime = settings.time;
    settings = nextSettings;
    settings.time = oldTime;
    settings.maxParticleCount = numParticles;
}

void ParticleEmitter::Emit(const EmitParticleData& data)
{
    if (emitParticles.size() >= numEmitParticles) return;
    emitParticles.emplace_back(data);
}

void ParticleEmitter::Update(ID3D11DeviceContext* context, float dt)
{
    if (!context) return;
    auto tracyGpuContext_ = GraphicsDeviceManager::Instance().GetTracyGpuContext();
    BS_PROFILE_GPU_SCOPE(tracyGpuContext_, "GPU::Particle Update CS");

    // CPU側で今フレーム発生するParticleリストを作ってからGPU更新へ渡す。
    settings.time += dt;
    settings.deltaTime = dt;
    BuildEmitList(dt);

    CommonConstants common{};
    common.elapsedTime = dt;
    common.textureSplitCount.x = (std::max)(1u, settings.textureSplitCount.x);
    common.textureSplitCount.y = (std::max)(1u, settings.textureSplitCount.y);
    common.systemNumParticles = numParticles;
    common.totalEmitCount = static_cast<UINT>(emitParticles.size());

    context->UpdateSubresource(emitterConstantBuffer.Get(), 0, nullptr, &settings, 0, 0);
    context->UpdateSubresource(commonConstantBuffer.Get(), 0, nullptr, &common, 0, 0);

    ID3D11Buffer* commonCB = commonConstantBuffer.Get();
    ID3D11Buffer* emitterCB = emitterConstantBuffer.Get();
    ID3D11Buffer* sortCB = sortConstantBuffer.Get();
    context->CSSetConstantBuffers(9, 1, &emitterCB);
    context->CSSetConstantBuffers(10, 1, &commonCB);
    context->CSSetConstantBuffers(11, 1, &sortCB);

    ID3D11ShaderResourceView* emitSrv = emitSRV.Get();
    context->CSSetShaderResources(0, 1, &emitSrv);

    ID3D11UnorderedAccessView* uavs[] = {
        particleDataUAV.Get(),
        unusedIndexUAV.Get(),
        indirectDataUAV.Get(),
        particleHeaderUAV.Get(),
    };
    UINT preserve = 0xFFFFFFFF;
    UINT initialCounts[] = { preserve, initialized ? preserve : 0, preserve, preserve };
    context->CSSetUnorderedAccessViews(0, _countof(uavs), uavs, initialCounts);

    if (!initialized)
    {
        // 初回だけGPU上のParticle/Index状態を初期化する。
        initialized = true;
        context->CSSetShader(initShader.Get(), nullptr, 0);
        BS_PROFILE_GPU_DETAILED_SCOPE(tracyGpuContext_, "GPU::Particle Init CS");
        context->Dispatch(numParticles / NumParticleThread, 1, 1);
    }

    context->CSSetShader(beginFrameShader.Get(), nullptr, 0);
    {
        BS_PROFILE_GPU_DETAILED_SCOPE(tracyGpuContext_, "GPU::Particle BeginFrame CS");
        context->Dispatch(1, 1, 1);
    }

    if (!emitParticles.empty())
    {
        // CPUで作ったEmitParticleDataをGPUへ転送し、Emit_CSで空きParticleへ割り当てる。
        D3D11_BOX box{};
        box.right = static_cast<UINT>(emitParticles.size() * sizeof(EmitParticleData));
        box.bottom = 1;
        box.back = 1;
        context->UpdateSubresource(emitBuffer.Get(), 0, &box, emitParticles.data(), box.right, 0);

        context->CSSetShader(emitShader.Get(), nullptr, 0);
        {
            BS_PROFILE_GPU_SCOPE(tracyGpuContext_, "GPU::Particle Emit CS");
            context->DispatchIndirect(indirectDataBuffer.Get(), EmitDispatchIndirectOffset);
        }
        emitParticles.clear();
    }

    // 生存Particleの寿命、速度、位置、色、深度を更新する。
    context->CSSetShader(updateShader.Get(), nullptr, 0);
    {
        BS_PROFILE_GPU_SCOPE(tracyGpuContext_, "GPU::Particle Simulate CS");
        context->DispatchIndirect(indirectDataBuffer.Get(), UpdateDispatchIndirectOffset);
    }

    // DrawInstancedIndirect draws only the leading live-count range.
    // Sorting also compacts live headers into that range, so it cannot be skipped.
    RunBitonicSort(context);

    context->CSSetShader(endFrameShader.Get(), nullptr, 0);
    {
        BS_PROFILE_GPU_DETAILED_SCOPE(tracyGpuContext_, "GPU::Particle EndFrame CS");
        context->Dispatch(1, 1, 1);
    }

    ID3D11UnorderedAccessView* nullUAVs[] = { nullptr, nullptr, nullptr, nullptr };
    context->CSSetUnorderedAccessViews(0, _countof(nullUAVs), nullUAVs, nullptr);
    ID3D11ShaderResourceView* nullSRV = nullptr;
    context->CSSetShaderResources(0, 1, &nullSRV);
    ID3D11Buffer* nullCB = nullptr;
    context->CSSetConstantBuffers(9, 1, &nullCB);
    context->CSSetConstantBuffers(10, 1, &nullCB);
    context->CSSetConstantBuffers(11, 1, &nullCB);
    context->CSSetShader(nullptr, nullptr, 0);
}

void ParticleEmitter::ResetRuntime()
{
    initialized = false;
    continuousAccumulator = 0.0f;
    depthSortFrame = 0u;
    settings.time = 0.0f;
    settings.deltaTime = 0.0f;
    settings.burstTrigger = 0.0f;
    emitParticles.clear();
}

bool ParticleEmitter::HasGPUResources() const
{
    return emitterConstantBuffer &&
        commonConstantBuffer &&
        particleDataBuffer &&
        particleDataSRV &&
        particleDataUAV &&
        particleHeaderBuffer &&
        particleHeaderSRV &&
        particleHeaderUAV &&
        indirectDataBuffer &&
        indirectDataUAV;
}

void ParticleEmitter::Render(ID3D11DeviceContext* context)
{
    if (!context) return;
    auto tracyGpuContext_ = GraphicsDeviceManager::Instance().GetTracyGpuContext();
    BS_PROFILE_GPU_SCOPE(tracyGpuContext_, "GPU::Particle Draw");

    ID3D11ShaderResourceView* dataSRV = particleDataSRV.Get();
    ID3D11ShaderResourceView* headerSRV = particleHeaderSRV.Get();
    ID3D11ShaderResourceView* texSRV = textureSRV.Get();
    ID3D11ShaderResourceView* maskSRV = maskTextureSRV.Get();
    ID3D11ShaderResourceView* gradientSRV = gradientTextureSRV.Get();
    ID3D11ShaderResourceView* shadeGradientSRV = shadeGradientTextureSRV.Get();
    ID3D11Buffer* emitterCB = emitterConstantBuffer.Get();

    context->GSSetShaderResources(9, 1, &dataSRV);
    context->GSSetShaderResources(10, 1, &headerSRV);
    context->VSSetConstantBuffers(9, 1, &emitterCB);
    context->GSSetConstantBuffers(9, 1, &emitterCB);
    context->PSSetConstantBuffers(9, 1, &emitterCB);
    ID3D11ShaderResourceView* particleTextures[] = { texSRV, maskSRV, gradientSRV, shadeGradientSRV };
    context->PSSetShaderResources(0, _countof(particleTextures), particleTextures);

    context->DrawInstancedIndirect(indirectDataBuffer.Get(), DrawIndirectOffset);
}

void ParticleEmitter::PrepareRender(ID3D11DeviceContext* context)
{
    Render(context);
}

void ParticleEmitter::BuildEmitList(float dt)
{
    if (!emissionEnabled) return;

    if (settings.emissionMode == 1)
    {
        // BurstはGUI/Actionからtriggerが立った瞬間だけ指定数を発生させる。
        if (settings.burstTrigger > 0.5f)
        {
            UINT count = settings.burstCount > 0 ? settings.burstCount : numEmitParticles;
            count = (std::min)(count, numEmitParticles);
            for (UINT i = 0; i < count; ++i) Emit(MakeEmitData(i));
        }
        return;
    }

    // Continuousは「秒間発生数 * dt」を蓄積し、小数分を次フレームへ持ち越す。
    float rate = settings.burstCount > 0 ? static_cast<float>(settings.burstCount) : 60.0f;
    continuousAccumulator += rate * dt;
    UINT count = static_cast<UINT>(continuousAccumulator);
    if (count == 0) return;

    continuousAccumulator -= static_cast<float>(count);
    count = (std::min)(count, numEmitParticles);
    for (UINT i = 0; i < count; ++i) Emit(MakeEmitData(static_cast<UINT>(settings.time * 1000.0f) + i));
}

EmitParticleData ParticleEmitter::MakeEmitData(UINT seed) const
{
    EmitParticleData data{};

    // seedから複数の乱数を作り、位置・速度・寿命・サイズへ別々に使う。
    float r0 = Random01(seed * 17u + 1u);
    float r1 = Random01(seed * 17u + 2u);
    float r2 = Random01(seed * 17u + 3u);
    float r3 = Random01(seed * 17u + 4u);
    float r4 = Random01(seed * 17u + 5u);
    float r5 = Random01(seed * 17u + 6u);
    float r6 = Random01(seed * 17u + 7u);
    float r7 = Random01(seed * 17u + 8u);
    float r8 = Random01(seed * 17u + 9u);
    float r9 = Random01(seed * 17u + 10u);
    float r10_ = Random01(seed * 17u + 11u);

    constexpr float pi = 3.1415926535f;

    const UINT splitX_ = (std::max)(1u, settings.textureSplitCount.x);
    const UINT splitY_ = (std::max)(1u, settings.textureSplitCount.y);
    const UINT spriteX_ = (std::min)(settings.spriteSheetIndex.x, splitX_ - 1u);
    const UINT spriteY_ = (std::min)(settings.spriteSheetIndex.y, splitY_ - 1u);
    data.parameter.x = static_cast<float>(spriteY_ * splitX_ + spriteX_); // sprite linear index
    data.parameter.y = (std::max)(0.01f, Lerp(settings.lifespan.x, settings.lifespan.y, r2)); // remaining life
    data.parameter.z = 0.0f; // total life is filled in Emit_CS
    data.parameter.w = static_cast<float>(settings.type); // render type: 0=billboard, 1=stretched

    // Spawn position. GUIのSpawn Shape設定をここで実際の発生座標へ変換する。
    DirectX::XMFLOAT3 localPosition{ 0.0f, 0.0f, 0.0f };
    switch (settings.spawnShape)
    {
    case 1: // Box
        localPosition.x = (r0 - 0.5f) * settings.shapeSize.x;
        localPosition.y = (r1 - 0.5f) * settings.shapeSize.y;
        localPosition.z = (r3 - 0.5f) * settings.shapeSize.z;
        break;

    case 2: // Circle / Disk on XZ plane
    {
        const float angle_ = r0 * pi * 2.0f;
        const float radius_ = std::sqrt(r1) * settings.shapeSize.x * 0.5f;
        localPosition.x = std::cos(angle_) * radius_;
        localPosition.y = 0.0f;
        localPosition.z = std::sin(angle_) * radius_;
        break;
    }

    case 3: // Sphere
    {
        const float z_ = r0 * 2.0f - 1.0f;
        const float a_ = r1 * pi * 2.0f;
        const float len_ = std::sqrt((std::max)(0.0f, 1.0f - z_ * z_));
        const float radius_ = std::cbrt(r3) * settings.shapeSize.x * 0.5f;
        localPosition.x = std::cos(a_) * len_ * radius_;
        localPosition.y = z_ * radius_;
        localPosition.z = std::sin(a_) * len_ * radius_;
        break;
    }

    case 4: // Line
        localPosition.x = Lerp(settings.shapeStart.x, settings.shapeEnd.x, r0);
        localPosition.y = Lerp(settings.shapeStart.y, settings.shapeEnd.y, r0);
        localPosition.z = Lerp(settings.shapeStart.z, settings.shapeEnd.z, r0);
        break;

    case 5: // Mesh Vertices
        if (!meshSpawnPositions.empty())
        {
            const size_t meshIndex_ = (std::min)(
                static_cast<size_t>(r0 * static_cast<float>(meshSpawnPositions.size())),
                meshSpawnPositions.size() - 1);
            localPosition = meshSpawnPositions[meshIndex_];
        }
        break;

    default: // Point + legacy radius ring support
    {
        const float angle_ = r0 * pi * 2.0f;
        const float radius_ = Lerp(settings.emissionOffset.x, settings.emissionOffset.y, r1);
        localPosition.x = std::cos(angle_) * radius_;
        localPosition.y = 0.0f;
        localPosition.z = std::sin(angle_) * radius_;
        break;
    }
    }

    // Point/Circle/Meshは平面に寄りやすいので、必要な時だけY方向の厚みを追加する。
    if ((settings.spawnShape == 0 || settings.spawnShape == 2 || settings.spawnShape == 5) &&
        settings.verticalSpawnRange > 0.0001f)
    {
        localPosition.y += (r10_ - 0.5f) * settings.verticalSpawnRange;
    }

    if (settings.spawnShape == 5 && !meshSpawnPositions.empty())
    {
        data.position.x = localPosition.x;
        data.position.y = localPosition.y;
        data.position.z = localPosition.z;
    }
    else
    {
        data.position.x = settings.emissionPosition.x + localPosition.x;
        data.position.y = settings.emissionPosition.y + localPosition.y;
        data.position.z = settings.emissionPosition.z + localPosition.z;
    }
    data.position.w = 1.0f;

    // 初速。Initial Speed が0の時は完全に停止開始にする。
    // Cone Angle 0 は「上向き方向」だが、速度0なら勝手に上へ飛ばない。
    float speed = Lerp(settings.emissionSpeed.x, settings.emissionSpeed.y, r5);
    if (std::abs(speed) <= 0.000001f)
    {
        data.velocity = { 0.0f, 0.0f, 0.0f, 0.0f };
    }
    else
    {
        float theta = Lerp(settings.emissionConeAngle.x, settings.emissionConeAngle.y, r3);
        float phi = r4 * pi * 2.0f;
        DirectX::XMFLOAT3 dir{
            std::sin(theta) * std::cos(phi),
            std::cos(theta),
            std::sin(theta) * std::sin(phi)
        };

        // Cone以外が選ばれている場合は、発生位置と中心の関係から外向き/内向きの初速へ置き換える。
        if (settings.spawnDirectionMode ==
            static_cast<uint32_t>(PARTICLE_SPAWN_DIRECTION_MODE::OUTWARD) ||
            settings.spawnDirectionMode ==
            static_cast<uint32_t>(PARTICLE_SPAWN_DIRECTION_MODE::INWARD))
        {
            DirectX::XMFLOAT3 radialVector_{
                localPosition.x,
                localPosition.y,
                localPosition.z
            };

            if (settings.spawnShape == 5 && !meshSpawnPositions.empty())
            {
                radialVector_.x = localPosition.x - settings.emissionPosition.x;
                radialVector_.y = localPosition.y - settings.emissionPosition.y;
                radialVector_.z = localPosition.z - settings.emissionPosition.z;
            }

            const float radialLengthSq_ =
                radialVector_.x * radialVector_.x +
                radialVector_.y * radialVector_.y +
                radialVector_.z * radialVector_.z;

            if (radialLengthSq_ > 0.000001f)
            {
                const float invRadialLength_ = 1.0f / std::sqrt(radialLengthSq_);
                const float directionSign_ =
                    settings.spawnDirectionMode ==
                    static_cast<uint32_t>(PARTICLE_SPAWN_DIRECTION_MODE::INWARD)
                    ? -1.0f
                    : 1.0f;
                dir.x = radialVector_.x * invRadialLength_ * directionSign_;
                dir.y = radialVector_.y * invRadialLength_ * directionSign_;
                dir.z = radialVector_.z * invRadialLength_ * directionSign_;
            }
        }
        else if (settings.spawnDirectionMode ==
            static_cast<uint32_t>(PARTICLE_SPAWN_DIRECTION_MODE::AXIS))
        {
            DirectX::XMFLOAT3 axis_
            {
                settings.emissionAxis.x,
                settings.emissionAxis.y,
                settings.emissionAxis.z
            };
            const float axisLengthSq_ =
                axis_.x * axis_.x + axis_.y * axis_.y + axis_.z * axis_.z;
            if (axisLengthSq_ > 0.000001f)
            {
                const float invAxisLength_ = 1.0f / std::sqrt(axisLengthSq_);
                axis_.x *= invAxisLength_;
                axis_.y *= invAxisLength_;
                axis_.z *= invAxisLength_;

                const float coneAngle_ = std::max(theta, 0.0f);
                DirectX::XMFLOAT3 reference_
                {
                    std::abs(axis_.y) < 0.98f ? 0.0f : 1.0f,
                    std::abs(axis_.y) < 0.98f ? 1.0f : 0.0f,
                    0.0f
                };
                DirectX::XMVECTOR axisVector_ = DirectX::XMLoadFloat3(&axis_);
                DirectX::XMVECTOR referenceVector_ = DirectX::XMLoadFloat3(&reference_);
                DirectX::XMVECTOR tangentVector_ = DirectX::XMVector3Normalize(
                    DirectX::XMVector3Cross(referenceVector_, axisVector_));
                DirectX::XMVECTOR bitangentVector_ = DirectX::XMVector3Normalize(
                    DirectX::XMVector3Cross(axisVector_, tangentVector_));
                DirectX::XMVECTOR coneVector_ = DirectX::XMVectorAdd(
                    DirectX::XMVectorScale(axisVector_, std::cos(coneAngle_)),
                    DirectX::XMVectorScale(
                        DirectX::XMVectorAdd(
                            DirectX::XMVectorScale(tangentVector_, std::cos(phi)),
                            DirectX::XMVectorScale(bitangentVector_, std::sin(phi))),
                        std::sin(coneAngle_)));
                DirectX::XMStoreFloat3(&dir, DirectX::XMVector3Normalize(coneVector_));
            }
        }

        data.velocity = { dir.x * speed, dir.y * speed, dir.z * speed, 0.0f };
    }

    // 基本加速度。追加加速度XYZはUpdate_CS側で windVelocity.xyz として加算される。
    data.acceleration = { 0.0f, settings.gravity, 0.0f, 0.0f };

    data.rotation.z = r6 * pi * 2.0f;
    data.rotation.w = Lerp(settings.emissionAngularSpeed.x, settings.emissionAngularSpeed.y, r7);

    float startSize_ = settings.emissionSize.x;
    float endSize_ = settings.emissionSize.y;
    if (settings.useRandomSize != 0)
    {
        startSize_ *= Lerp(settings.sizeRandomStart.x, settings.sizeRandomStart.y, r8);
        endSize_ *= Lerp(settings.sizeRandomEnd.x, settings.sizeRandomEnd.y, r9);
    }

    data.scale.x = startSize_;
    data.scale.y = startSize_;
    data.scale.z = endSize_;
    data.scale.w = 0.0f;
    data.color = settings.useColorOverLife != 0 ? settings.colorStart : settings.manualColor;

    return data;
}

void ParticleEmitter::RunBitonicSort(ID3D11DeviceContext* context)
{
    auto tracyGpuContext_ = GraphicsDeviceManager::Instance().GetTracyGpuContext();
    BS_PROFILE_GPU_DETAILED_SCOPE(tracyGpuContext_, "GPU::Particle Sort CS");

    // 半透明Particleを奥から描けるように、GPU上でHeaderを深度順に並べる。
    float exponentF = std::log2(static_cast<float>(numParticles));
    UINT exponent = static_cast<UINT>(std::ceil(exponentF) + 0.5f);

    for (UINT i = 0; i < exponent; ++i)
    {
        UINT increment = 1u << i;
        for (UINT j = 0; j < i + 1; ++j)
        {
            SortConstants constants{};
            constants.increment = increment;
            constants.direction = 2u << i;
            context->UpdateSubresource(sortConstantBuffer.Get(), 0, nullptr, &constants, 0, 0);

            if (increment <= BitonicSortC2Thread)
            {
                context->CSSetShader(sortC2Shader.Get(), nullptr, 0);
                context->Dispatch(std::max<UINT>(1, numParticles / 2 / BitonicSortC2Thread), 1, 1);
                break;
            }

            context->CSSetShader(sortB2Shader.Get(), nullptr, 0);
            context->Dispatch(std::max<UINT>(1, numParticles / 2 / BitonicSortB2Thread), 1, 1);
            increment /= 2;
        }
    }
}

UINT ParticleEmitter::RoundParticleCount(UINT count)
{
    float exponentF = std::log2(static_cast<float>((std::max<UINT>)(count, 1)));
    UINT exponent = static_cast<UINT>(std::ceil(exponentF) + 0.5f);
    UINT rounded = 1u << exponent;
    rounded = (std::max<UINT>)(rounded, NumParticleThread);
    rounded = (std::min<UINT>)(rounded, 1u << 20);
    return ((rounded + (NumParticleThread - 1)) / NumParticleThread) * NumParticleThread;
}

float ParticleEmitter::Random01(UINT seed)
{
    seed = (seed ^ 61u) ^ (seed >> 16u);
    seed *= 9u;
    seed = seed ^ (seed >> 4u);
    seed *= 0x27d4eb2du;
    seed = seed ^ (seed >> 15u);
    return static_cast<float>(seed & 0x00FFFFFFu) / 16777216.0f;
}

float ParticleEmitter::Lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}




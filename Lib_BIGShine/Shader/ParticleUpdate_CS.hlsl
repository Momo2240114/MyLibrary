#include "ParticleCommon.hlsli"

RWStructuredBuffer<Particle> particleDataBuffer : register(u0);
AppendStructuredBuffer<uint> unusedIndexBuffer : register(u1);
RWByteAddressBuffer indirectDataBuffer : register(u2);
RWStructuredBuffer<ParticleHeader> particleHeaderBuffer : register(u3);

[numthreads(NumParticleThread, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint headerIndex = dispatchThreadID.x;
    if (headerIndex >= systemNumParticles) return;

    ParticleHeader header = particleHeaderBuffer[headerIndex];
    if (header.alive == 0) return;

    uint dataIndex = header.particleIndex;
    Particle data = particleDataBuffer[dataIndex];

    data.parameter.y -= elapsedTime;
    if (data.parameter.y <= 0.0f)
    {
        header.alive = 0;
        particleHeaderBuffer[headerIndex] = header;
        unusedIndexBuffer.Append(dataIndex);

        uint originalDeadCount;
        indirectDataBuffer.InterlockedAdd(IndirectArgumentsNumDeadParticle, 1u, originalDeadCount);
        return;
    }

    data.velocity.xyz += data.acceleration.xyz * elapsedTime;
    data.position.xyz += data.velocity.xyz * elapsedTime;
    data.rotation.z += data.rotation.w * elapsedTime;

    uint type = (uint)(data.parameter.x + 0.5f);
    if (useSpriteAnimation != 0)
    {
        uint animationStart = min(spriteAnimationStartIndex, spriteAnimationEndIndex);
        uint animationEnd = max(spriteAnimationStartIndex, spriteAnimationEndIndex);
        uint animationLength = max(1u, animationEnd - animationStart + 1u);
        float age = max(0.0f, data.parameter.z - data.parameter.y);
        uint animationFrame = (uint)floor(age * max(spriteAnimationFps, 0.0f));
        if (spriteAnimationLoop != 0)
        {
            animationFrame %= animationLength;
        }
        else
        {
            animationFrame = min(animationFrame, animationLength - 1u);
        }
        type = animationStart + animationFrame;
    }
    type = min(type, max(1u, textureSplitCount.x * textureSplitCount.y) - 1u);
    float w = 1.0f / max(1u, textureSplitCount.x);
    float h = 1.0f / max(1u, textureSplitCount.y);
    data.texcoord.xy = float2((type % textureSplitCount.x) * w, (type / textureSplitCount.x) * h);
    data.texcoord.zw = float2(w, h);

    float lifeRate = saturate(data.parameter.y / max(data.parameter.z, 0.0001f));
    if (useColorOverLife != 0)
    {
        float colorRate = 1.0f - lifeRate;
        float4 lifeColor = lerp(colorStart, colorEnd, colorRate);
    
    // RGBはグラデーションの色をそのまま維持
        data.color.rgb = lifeColor.rgb;
    //アルファもグラデーションのアルファ、または純粋なlifeRateによる減衰のみにする
        data.color.a = lifeColor.a * lifeRate;
    }
    else
    {
        data.color.rgb = manualColor.rgb;
        data.color.a = manualColor.a * lifeRate;
    }

    float4 clip = mul(float4(data.position.xyz, 1.0f), viewProjection);
    header.depth = clip.w;

    particleDataBuffer[dataIndex] = data;
    particleHeaderBuffer[headerIndex] = header;
}



#include "ParticleCommon.hlsli"

RWStructuredBuffer<Particle> particleDataBuffer : register(u0);
AppendStructuredBuffer<uint> unusedIndexBuffer : register(u1);
RWByteAddressBuffer indirectDataBuffer : register(u2);
RWStructuredBuffer<ParticleHeader> particleHeaderBuffer : register(u3);

float Hash11(float n)
{
    return frac(sin(n) * 43758.5453123f);
}

float3 Hash33(float3 p)
{
    // 必ず -1.0〜+1.0 に中心化する。0〜1をそのまま使うと片方向へ流れる。
    return float3(
        Hash11(dot(p, float3(127.1f, 311.7f, 74.7f))),
        Hash11(dot(p, float3(269.5f, 183.3f, 246.1f))),
        Hash11(dot(p, float3(113.5f, 271.9f, 124.6f)))
    ) * 2.0f - 1.0f;
}

float3 SmoothNoise3(float3 p)
{
    // 軽量value noise。格子間を補間するのでHash直当てより暴れにくい。
    float3 i = floor(p);
    float3 f = frac(p);
    f = f * f * (3.0f - 2.0f * f);

    float3 n000 = Hash33(i + float3(0, 0, 0));
    float3 n100 = Hash33(i + float3(1, 0, 0));
    float3 n010 = Hash33(i + float3(0, 1, 0));
    float3 n110 = Hash33(i + float3(1, 1, 0));
    float3 n001 = Hash33(i + float3(0, 0, 1));
    float3 n101 = Hash33(i + float3(1, 0, 1));
    float3 n011 = Hash33(i + float3(0, 1, 1));
    float3 n111 = Hash33(i + float3(1, 1, 1));

    float3 nx00 = lerp(n000, n100, f.x);
    float3 nx10 = lerp(n010, n110, f.x);
    float3 nx01 = lerp(n001, n101, f.x);
    float3 nx11 = lerp(n011, n111, f.x);
    float3 nxy0 = lerp(nx00, nx10, f.y);
    float3 nxy1 = lerp(nx01, nx11, f.y);
    return lerp(nxy0, nxy1, f.z);
}

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
        data.parameter.y = 0.0f;
        data.color.a = 0.0f;

        header.alive = 0;
        header.depth = -3.402823466e+38f;
        header.dummy = 0;

        particleDataBuffer[dataIndex] = data;
        particleHeaderBuffer[headerIndex] = header;

        unusedIndexBuffer.Append(dataIndex);

        uint originalDeadCount;
        indirectDataBuffer.InterlockedAdd(
        IndirectArgumentsNumDeadParticle,
        1u,
        originalDeadCount
    );

        return;
    }

    float totalLife = max(data.parameter.z, 0.0001f);
    float age = max(0.0f, totalLife - data.parameter.y);
    float life01 = saturate(age / totalLife);

    // 基本物理。data.acceleration.yにはGravity/Acceleration Y、windVelocity.xyzにはAdditional Acceleration XYZが入る。
    data.velocity.xyz += data.acceleration.xyz * elapsedTime;
    data.velocity.xyz += windVelocity.xyz * elapsedTime;

    // 中心から外へ押し出す Radial Velocity。
    if (abs(radialVelocity) > 0.0001f)
    {
        float3 radialDir = data.position.xyz - emissionPosition.xyz;
        float radialLenSq = dot(radialDir, radialDir);
        if (radialLenSq > 0.000001f)
        {
            data.velocity.xyz += normalize(radialDir) * radialVelocity * elapsedTime;
        }
    }

    // Y軸周りのOrbit。Effekseer/NiagaraのOrbit系の簡易版。
    if (abs(orbitVelocity) > 0.0001f)
    {
        float3 fromCenter = data.position.xyz - emissionPosition.xyz;
        float3 tangent = float3(-fromCenter.z, 0.0f, fromCenter.x);
        if (dot(tangent, tangent) > 0.000001f)
        {
            data.velocity.xyz += normalize(tangent) * orbitVelocity * elapsedTime;
        }
    }

    // Noise/Turbulence。旧noiseScaleも0以外なら使えるように互換維持。
    float finalNoiseStrength = max(noiseStrength, noiseScale);
    if (finalNoiseStrength > 0.0f)
    {
        float freq = max(noiseFrequency, 0.0001f);
        float scroll = time * noiseSpeed;
        float seed = (float)dataIndex * 0.137f;
        float3 noiseP = data.position.xyz * freq + float3(scroll + seed, scroll * 0.73f, scroll * 1.37f);
        float3 noiseVec = SmoothNoise3(noiseP);

        // 微小な残留バイアスも消すため、粒子固有の逆位相ノイズを混ぜる。
        noiseVec += SmoothNoise3(noiseP + 19.19f) * 0.5f;
        noiseVec = clamp(noiseVec, -1.0f, 1.0f);

        data.velocity.xyz += noiseVec * finalNoiseStrength * elapsedTime;
    }

    // Drag。指数減衰なのでdtが変わっても挙動が破綻しにくい。
    if (drag > 0.0f)
    {
        data.velocity.xyz *= exp(-drag * elapsedTime);
    }

    data.position.xyz += data.velocity.xyz * elapsedTime;
    data.rotation.z += data.rotation.w * elapsedTime;

    uint spriteIndex = (uint)(data.parameter.x + 0.5f);
    if (useSpriteAnimation != 0)
    {
        uint animationStart = min(spriteAnimationStartIndex, spriteAnimationEndIndex);
        uint animationEnd = max(spriteAnimationStartIndex, spriteAnimationEndIndex);
        uint animationLength = max(1u, animationEnd - animationStart + 1u);
        uint animationFrame = (uint)floor(age * max(spriteAnimationFps, 0.0f));
        if (spriteAnimationLoop != 0) animationFrame %= animationLength;
        else animationFrame = min(animationFrame, animationLength - 1u);
        spriteIndex = animationStart + animationFrame;
    }
    spriteIndex = min(spriteIndex, max(1u, textureSplitCount.x * textureSplitCount.y) - 1u);
    float w = 1.0f / max(1u, textureSplitCount.x);
    float h = 1.0f / max(1u, textureSplitCount.y);
    data.texcoord.xy = float2((spriteIndex % textureSplitCount.x) * w, (spriteIndex / textureSplitCount.x) * h);
    data.texcoord.zw = float2(w, h);

    float lifeRate = saturate(data.parameter.y / totalLife);
    float fadeIn = fadeDuration.x > 0.0f ? saturate(age / fadeDuration.x) : 1.0f;
    float fadeOut = fadeDuration.y > 0.0f ? saturate(data.parameter.y / fadeDuration.y) : 1.0f;
    float alphaFade = min(fadeIn, fadeOut);

    if (useColorOverLife != 0)
    {
        float4 lifeColor = lerp(colorStart, colorEnd, life01);
        data.color.rgb = lifeColor.rgb;
        data.color.a = lifeColor.a * alphaFade;
    }
    else
    {
        data.color.rgb = manualColor.rgb;
        data.color.a = manualColor.a * alphaFade;
    }

    float4 clip = mul(float4(data.position.xyz, 1.0f), viewProjection);
    header.depth = clip.w;

    particleDataBuffer[dataIndex] = data;
    particleHeaderBuffer[headerIndex] = header;
}

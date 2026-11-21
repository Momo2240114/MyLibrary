#include "ParticleCommon.hlsli"

StructuredBuffer<Particle> particleDataBuffer : register(t9);
StructuredBuffer<ParticleHeader> particleHeaderBuffer : register(t10);

[maxvertexcount(4)]
void main(point VS_OUT input[1], inout TriangleStream<GS_OUT> outStream)
{
    uint headerIndex = input[0].vertexID;
    ParticleHeader header = particleHeaderBuffer[headerIndex];
    if (header.alive == 0)
    {
        return;
    }

    Particle p = particleDataBuffer[header.particleIndex];

    float4 centerWorld = mul(float4(0.0f, 0.0f, 0.0f, 1.0f), invViewProjection);
    float4 rightWorld = mul(float4(1.0f, 0.0f, 0.0f, 1.0f), invViewProjection);
    float4 upWorld = mul(float4(0.0f, 1.0f, 0.0f, 1.0f), invViewProjection);
    centerWorld.xyz /= centerWorld.w;
    rightWorld.xyz /= rightWorld.w;
    upWorld.xyz /= upWorld.w;

    float3 right = normalize(rightWorld.xyz - centerWorld.xyz);
    float3 up = normalize(upWorld.xyz - centerWorld.xyz);
    float3 cameraForward = normalize(cross(right, up));

    float lifeRate = saturate(p.parameter.y / max(p.parameter.z, 0.0001f));
    float size = lerp(p.scale.z, p.scale.x, lifeRate);
    float renderType = p.parameter.w;

    // Render Type 1: stretched billboard. Velocity direction is used as long axis.
    if (renderType > 0.5f && dot(p.velocity.xyz, p.velocity.xyz) > 0.0001f)
    {
        up = normalize(p.velocity.xyz);
        float3 stretchedRight = cross(up, cameraForward);
        right = dot(stretchedRight, stretchedRight) > 0.0001f ? normalize(stretchedRight) : right;
        up = normalize(cross(cameraForward, right));
    }

    float s = sin(p.rotation);
    float c = cos(p.rotation);

    float2 offsets[4] = {
        float2(-0.5f, -0.5f),
        float2( 0.5f, -0.5f),
        float2(-0.5f,  0.5f),
        float2( 0.5f,  0.5f)
    };
    float2 uvs[4] = {
        float2(0, 1),
        float2(1, 1),
        float2(0, 0),
        float2(1, 0)
    };

    [unroll]
    for (uint i = 0; i < 4; ++i)
    {
        float2 local;
        local.x = offsets[i].x * c - offsets[i].y * s;
        local.y = offsets[i].x * s + offsets[i].y * c;

        float width = size;
        float height = size;
        if (renderType > 0.5f)
        {
            width = size * 0.5f;
            height = min(stretchMax, max(size, length(p.velocity.xyz) * max(stretchScale, 0.0f) + size));
        }

        float3 worldPos = p.position.xyz + right * local.x * width + up * local.y * height;

        GS_OUT output;
        output.pos = mul(float4(worldPos, 1.0f), viewProjection);
        output.color = p.color;
        output.tex = p.texcoord.xy + p.texcoord.zw * uvs[i];
        output.lifeRate = 1.0f - lifeRate;
        output.localTex = uvs[i];
        outStream.Append(output);
    }
    outStream.RestartStrip();
}

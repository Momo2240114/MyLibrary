#include "ParticleCommon.hlsli"


StructuredBuffer<Particle> ParticleBuffer : register(t9);

[maxvertexcount(4)]
void main(point VS_OUT input[1], inout TriangleStream<GS_OUT> outStream)
{
    uint id = input[0].vertexID;
    Particle p = ParticleBuffer[id];
    
    
    if (p.isActive == 0)
        return; // 生きている粒子のみ

    // --- row_major 行列からのビルボードベクトル抽出 ---
    // 行優先の場合、1行目がRight、2行目がUpベクトルに相当します
    float3 right = float3(viewProjection[0][0], viewProjection[1][0], viewProjection[2][0]);
    float3 up = float3(viewProjection[0][1], viewProjection[1][1], viewProjection[2][1]);
    
    float t = saturate(p.age / p.lifeSpan);
    float currentSize = lerp(emissionSize.x, emissionSize.y, t);
    float4 currentColor = lerp(p.startColor, p.endColor, t);

    float2 offsets[4] = { float2(-1, 1), float2(1, 1), float2(-1, -1), float2(1, -1) };
    float2 uvs[4] = { float2(0, 0), float2(1, 0), float2(0, 1), float2(1, 1) };

    [unroll]
    for (int i = 0; i < 4; i++)
    {
        GS_OUT output = (GS_OUT) 0;

        float3 worldPos = p.position + (right * offsets[i].x * currentSize) + (up * offsets[i].y * currentSize);
        output.pos = mul(float4(worldPos, 1.0f), viewProjection);
        output.worldPos = worldPos; // ★ここで PS へ渡す
        output.color = currentColor;
        output.tex = uvs[i];
        output.type = p.type;

        outStream.Append(output);
    }
    outStream.RestartStrip();
}
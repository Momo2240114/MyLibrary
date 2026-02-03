// 深度とUVからワールド座標を復元する関数
float3 ReconstructWorldPos(float2 uv, float depth, float4x4 invViewProj)
{
    float4 ndcPos;
    ndcPos.x = uv.x * 2.0f - 1.0f;
    ndcPos.y = (1.0f - uv.y) * 2.0f - 1.0f;
    ndcPos.z = depth;
    ndcPos.w = 1.0f;

    float4 worldPos = mul(ndcPos, invViewProj);
    return worldPos.xyz / worldPos.w;
}
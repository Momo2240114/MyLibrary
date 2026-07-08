cbuffer CbObjectMotionVelocity : register(b0)
{
    row_major float4x4 currentViewProjection;
    row_major float4x4 previousViewProjection;
    row_major float4x4 currentOwnerWorld;
    row_major float4x4 previousOwnerWorld;
    row_major float4x4 inverseCurrentOwnerWorld;
    float velocityScale;
    float3 padding;
};

struct VS_OUT
{
    float4 position : SV_POSITION;
    float4 currentClip : TEXCOORD0;
    float4 previousClip : TEXCOORD1;
};

float4 main(VS_OUT input_) : SV_TARGET
{
    if (abs(input_.currentClip.w) < 0.00001f || abs(input_.previousClip.w) < 0.00001f)
    {
        return 0.0f;
    }

    const float2 currentNdc_ = input_.currentClip.xy / input_.currentClip.w;
    const float2 previousNdc_ = input_.previousClip.xy / input_.previousClip.w;
    float2 velocity_ = (currentNdc_ - previousNdc_) * float2(0.5f, -0.5f) * velocityScale;
    return float4(velocity_, 0.0f, 1.0f);
}

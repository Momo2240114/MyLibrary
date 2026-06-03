#include "Scene.hlsli"

struct RADIAL_BLUR_PS_IN
{
float4 position : SV_POSITION;
float2 texcoord : TEXCOORD0;
};

cbuffer RADIAL_BLUR_CONSTANT_BUFFER : register(b2)
{

float blur_radius;
int blur_sampling_count;
float2 blur_center;
float blur_mask_radius;
float3 blur_dummy;
};

Texture2D scene_map : register(t0);
SamplerState linear_sampler_state : register(s0);

float4 main(RADIAL_BLUR_PS_IN pin) : SV_TARGET
{
    const float4 base_color = scene_map.Sample(linear_sampler_state, pin.texcoord);

    int sample_count = max(blur_sampling_count, 1);
    float2 blur_vector = (blur_center - pin.texcoord) * (blur_radius / sample_count);

    float4 blur_color = 0.0f;
    [loop]
    for (int index = 0; index < sample_count; ++index)
    {
        blur_color += scene_map.Sample(linear_sampler_state, pin.texcoord + blur_vector * index);
    }
    blur_color /= sample_count;

    float dist = distance(pin.texcoord, blur_center);
    float mask = smoothstep(0.0f, max(blur_mask_radius, 1e-5f), dist);

    return lerp(base_color, blur_color, mask);
}

#include "sprite.hlsli" 

Texture2D color_map : register(t0);
SamplerState point_sampler_state : register(s0);
SamplerState linear_sampler_state : register(s1);
SamplerState anisotropic_sampler_state : register(s2);
float4 main(VS_OUT pin) : SV_TARGET
{
    float4 color_ = color_map.Sample(point_sampler_state, pin.texcoord) * pin.color;
    // モデル描画と同じく、完全に近い透明ピクセルは描画対象から外す。
    if (color_.a < 0.1f)
    {
        discard;
    }
    return color_;
}

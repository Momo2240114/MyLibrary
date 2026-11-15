#include "Decal_Common.hlsli"
#include "bidirectional_reflectance_distribution_function.hlsli"

float4 main(VS_OUT pin) : SV_TARGET
{
    float2 screen_size;
    scene_depth_texture.GetDimensions(screen_size.x, screen_size.y);
    float2 screen_uv = pin.position.xy / screen_size;

    float depth = scene_depth_texture.Sample(sampler_states[0], screen_uv).r;

    float4 clip_pos;
    clip_pos.x = screen_uv.x * 2.0f - 1.0f;
    clip_pos.y = (1.0f - screen_uv.y) * 2.0f - 1.0f;
    clip_pos.z = depth;
    clip_pos.w = 1.0f;

    float4 world_pos_h = mul(clip_pos, invViewProjection);
    float3 world_pos = world_pos_h.xyz / world_pos_h.w;

    float4 local_pos = mul(float4(world_pos, 1.0f), decal_inverse_transform);
    local_pos.xyz /= local_pos.w;

    clip(1.0f - abs(local_pos.x));
    clip(1.0f - abs(local_pos.y));
    clip(1.0f - abs(local_pos.z));

    float2 base_texcoord = float2(local_pos.x * 0.5f + 0.5f, local_pos.y * -0.5f + 0.5f);
    float2 decal_texcoord = base_texcoord * decalUvScaleOffset.xy + decalUvScaleOffset.zw;
    float2 decal_noise_texcoord = base_texcoord * decalNoiseUvScaleOffset.xy + decalNoiseUvScaleOffset.zw;
    float4 decal_tex = decal_texture.Sample(sampler_states[1], decal_texcoord);
    float4 mask_sample = decal_mask_texture.Sample(sampler_states[1], decal_texcoord);
    float mask = decalGroundSurface != 0u ? mask_sample.a : mask_sample.r;

    float image_alpha = decalGroundSurface != 0u ? decal_tex.a : decal_tex.r;
    float3 albedo = decal_tex.rgb * decalBaseColor.rgb * pin.color.rgb;
    float alpha = image_alpha * mask * decalBaseColor.a * pin.color.a;

    if (decalUseDissolve != 0u)
    {
        float noise = decal_noise_texture.Sample(sampler_states[1], decal_noise_texcoord).r;
        float threshold = decalDissolveParams.y;
        float edgeWidth = max(decalDissolveParams.z, 0.0001f);
        float dissolveAlpha = smoothstep(threshold, threshold + edgeWidth, noise);
        float edgeAlpha = saturate(1.0f - abs(noise - threshold) / edgeWidth) * decalDissolveEdgeColor.a;
        albedo = lerp(albedo, decalDissolveEdgeColor.rgb, edgeAlpha);
        alpha *= dissolveAlpha;
    }

    float3 N = normalize(scene_normal_texture.Sample(sampler_states[0], screen_uv).xyz * 2.0f - 1.0f);
    if (decalGroundSurface != 0u)
    {
        float3 groundNormal = -normalize(decal_world_matrix[2].xyz);
        alpha *= smoothstep(0.65f, 0.9f, dot(N, groundNormal));
    }
    float3 V = normalize(cameraPosition.xyz - world_pos);

    float metalness = 0.0f;
    float roughness = 0.05f;
    float alphaRoughness = roughness * roughness;

    float3 f0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metalness);
    float3 f90 = float3(1.0f, 1.0f, 1.0f);
    float NdotV = abs(dot(N, V)) + 1e-5f;

    float3 totalDirectLighting = float3(0.0f, 0.0f, 0.0f);

    {
        float3 L = normalize(-lightDirection.xyz);
        float3 H = normalize(L + V);
        float NdotL = saturate(dot(N, L));
        float NdotH = saturate(dot(N, H));
        float VoH = saturate(dot(V, H));

        float3 specular = brdf_specular_ggx(f0, f90, alphaRoughness, VoH, NdotL, NdotV, NdotH);
        float3 diffuse = brdf_lambertian(f0, f90, albedo, VoH) * (1.0f - metalness);
        totalDirectLighting += (diffuse + specular) * (lightColor.rgb * lightColor.w) * NdotL;
    }

    [loop]
    for (int i = 0; i < numPointLights; i++)
    {
        PointLightData pl = pointLights[i];
        float3 Lvec = pl.position - world_pos;
        float d = length(Lvec);
        if (d > pl.range) continue;

        float3 L = normalize(Lvec);
        float3 H = normalize(L + V);
        float NdotL = saturate(dot(N, L));
        float NdotH = saturate(dot(N, H));
        float VoH = saturate(dot(V, H));

        float attenuation = saturate(1.0f - (d / pl.range));
        attenuation *= attenuation;

        float3 specular = brdf_specular_ggx(f0, f90, alphaRoughness, VoH, NdotL, NdotV, NdotH);
        float3 diffuse = brdf_lambertian(f0, f90, albedo, VoH) * (1.0f - metalness);
        totalDirectLighting += (diffuse + specular) * (pl.color * pl.intensity) * NdotL * attenuation;
    }

    [loop]
    for (int j = 0; j < numSpotLights; j++)
    {
        SpotLightData sl = spotLights[j];
        float3 Lvec = sl.position - world_pos;
        float d = length(Lvec);
        if (d > sl.range) continue;

        float3 L = normalize(Lvec);
        float cosAngle = dot(normalize(sl.direction), -L);
        float cosInner = cos(radians(sl.innerAngle));
        float cosOuter = cos(radians(sl.outerAngle));
        float spotAttenuation = saturate((cosAngle - cosOuter) / (cosInner - cosOuter));
        spotAttenuation *= spotAttenuation;
        if (spotAttenuation <= 0.0f) continue;

        float3 H = normalize(L + V);
        float NdotL = saturate(dot(N, L));
        float NdotH = saturate(dot(N, H));
        float VoH = saturate(dot(V, H));

        float attenuation = saturate(1.0f - (d / sl.range));
        attenuation *= attenuation * spotAttenuation;

        float3 specular = brdf_specular_ggx(f0, f90, alphaRoughness, VoH, NdotL, NdotV, NdotH);
        float3 diffuse = brdf_lambertian(f0, f90, albedo, VoH) * (1.0f - metalness);
        totalDirectLighting += (diffuse + specular) * (sl.color * sl.intensity) * NdotL * attenuation;
    }

    float3 edge_weight = saturate((1.0f - abs(local_pos.xyz)) * 8.0f);
    float boundary_fade = edge_weight.x * edge_weight.y * edge_weight.z;
    float final_alpha = alpha * boundary_fade;
    clip(final_alpha - 0.001f);

    float4 bg_color = scene_color_texture.Sample(sampler_states[0], screen_uv);
    float3 ambientColor = float3(0.03f, 0.03f, 0.03f) * albedo;
    float3 emissiveColor = decalEmissive.rgb * decalEmissive.w * albedo;
    float3 final_decal_color = totalDirectLighting + ambientColor + emissiveColor;
    final_decal_color = pow(final_decal_color, 1.0f / 1.25f);

    float3 final_rgb = decalBlendMode == 1u
        ? bg_color.rgb + final_decal_color * final_alpha
        : lerp(bg_color.rgb, final_decal_color, final_alpha);

    // 接地跡は現在の描画先と合成し、上層の発光が下層の焦げを塗り戻さないようにする。
    if (decalGroundSurface != 0u) return float4(final_decal_color, final_alpha);
    return float4(final_rgb, 1.0f);
}
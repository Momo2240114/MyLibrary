#include "Decal_Common.hlsli"

#include "bidirectional_reflectance_distribution_function.hlsli"

float4 main(VS_OUT pin) : SV_TARGET
{
    // --- 1. スクリーン情報の取得 ---
    float2 screen_size;
    scene_depth_texture.GetDimensions(screen_size.x, screen_size.y);
    float2 screen_uv = pin.position.xy / screen_size;

    // --- 2. 深度とワールド座標の復元 ---
    float depth = scene_depth_texture.Sample(sampler_states[0], screen_uv).r;
    
    float4 clip_pos;
    clip_pos.x = screen_uv.x * 2.0f - 1.0f;
    clip_pos.y = (1.0f - screen_uv.y) * 2.0f - 1.0f;
    clip_pos.z = depth;
    clip_pos.w = 1.0f;

    float4 world_pos_h = mul(clip_pos, invViewProjection);
    float3 world_pos = world_pos_h.xyz / world_pos_h.w;

    // --- 3. デカール空間（-1 ～ 1）への変換と範囲チェック ---
    float4 local_pos = mul(float4(world_pos, 1.0f), decal_inverse_transform);
    local_pos.xyz /= local_pos.w;

    clip(1.0f - abs(local_pos.x));
    clip(1.0f - abs(local_pos.y));
    clip(1.0f - abs(local_pos.z));

    // --- 4. UV計算とベースカラー取得 (JPG透過対応) ---
    float2 decal_texcoord = float2(local_pos.x * 0.5f + 0.5f, local_pos.y * -0.5f + 0.5f);
    float4 decal_tex = decal_texture.Sample(sampler_states[1], decal_texcoord);
    
    // 黒背景を透明度として抽出（明るい部分が残る）
    float image_alpha = decal_tex.r;
    float3 albedo = decal_tex.rgb * decalBaseColor.rgb * pin.color.rgb;
    float alpha = image_alpha * decalBaseColor.a * pin.color.a;

    // --- 5. PBR共通パラメータの準備 ---
    // GBufferから「壁の法線」を取得
    float3 N = normalize(scene_normal_texture.Sample(sampler_states[0], screen_uv).xyz * 2.0f - 1.0f);
    float3 V = normalize(cameraPosition.xyz - world_pos);

    float metalness = 0.0;
    float roughness = max(0.0, 0.05f); // 0除算防止
    float alphaRoughness = roughness * roughness;

    float3 f0 = lerp(float3(0.04, 0.04, 0.04), albedo, metalness);
    float3 f90 = float3(1.0, 1.0, 1.0);
    float NdotV = abs(dot(N, V)) + 1e-5f;

    float3 totalDirectLighting = float3(0, 0, 0);

    // --- 6. ライティング計算開始 ---

    // A. 平行光源 (Directional Light)
    {
        float3 L = normalize(-lightDirection.xyz);
        float3 H = normalize(L + V);
        float NdotL = saturate(dot(N, L));
        float NdotH = saturate(dot(N, H));
        float VoH = saturate(dot(V, H));

        float3 specular = brdf_specular_ggx(f0, f90, alphaRoughness, VoH, NdotL, NdotV, NdotH);
        float3 diffuse = brdf_lambertian(f0, f90, albedo, VoH) * (1.0 - metalness);

        totalDirectLighting += (diffuse + specular) * (lightColor.rgb * lightColor.w) * NdotL;
    }

    // B. 点光源ループ (Point Lights)
    [loop]
    for (int i = 0; i < numPointLights; i++)
    {
        PointLightData pl = pointLights[i];
        float3 Lvec = pl.position - world_pos;
        float d = length(Lvec);
        if (d > pl.range)
            continue;

        float3 L = normalize(Lvec);
        float3 H = normalize(L + V);
        float NdotL = saturate(dot(N, L));
        float NdotH = saturate(dot(N, H));
        float VoH = saturate(dot(V, H));

        // 距離減衰
        float attenuation = saturate(1.0 - (d / pl.range));
        attenuation *= attenuation;

        float3 specular = brdf_specular_ggx(f0, f90, alphaRoughness, VoH, NdotL, NdotV, NdotH);
        float3 diffuse = brdf_lambertian(f0, f90, albedo, VoH) * (1.0 - metalness);

        totalDirectLighting += (diffuse + specular) * (pl.color * pl.intensity) * NdotL * attenuation;
    }

    // C. スポットライトループ (Spot Lights)
    [loop]
    for (int j = 0; j < numSpotLights; j++)
    {
        SpotLightData sl = spotLights[j];
        float3 Lvec = sl.position - world_pos;
        float d = length(Lvec);
        if (d > sl.range)
            continue;

        float3 L = normalize(Lvec);
        float cosAngle = dot(normalize(sl.direction), -L);
        float cosInner = cos(radians(sl.innerAngle));
        float cosOuter = cos(radians(sl.outerAngle));
        
        // スポット配光減衰
        float spotAttenuation = saturate((cosAngle - cosOuter) / (cosInner - cosOuter));
        spotAttenuation *= spotAttenuation;
        if (spotAttenuation <= 0.0f)
            continue;

        float3 H = normalize(L + V);
        float NdotL = saturate(dot(N, L));
        float NdotH = saturate(dot(N, H));
        float VoH = saturate(dot(V, H));

        // 距離減衰 × スポット減衰
        float attenuation = saturate(1.0 - (d / sl.range));
        attenuation *= attenuation * spotAttenuation;

        float3 specular = brdf_specular_ggx(f0, f90, alphaRoughness, VoH, NdotL, NdotV, NdotH);
        float3 diffuse = brdf_lambertian(f0, f90, albedo, VoH) * (1.0 - metalness);

        totalDirectLighting += (diffuse + specular) * (sl.color * sl.intensity) * NdotL * attenuation;
    }

    // --- 7. フェード処理 ---
    // ボックスの境界を滑らかに消す
    float3 edge_weight = saturate((1.0f - abs(local_pos.xyz)) * 8.0f);
    float boundary_fade = edge_weight.x * edge_weight.y * edge_weight.z;
    
    // 壁の向きとデカール投影方向による減衰
    float dot_nl =1;
    
    float final_alpha = alpha * boundary_fade * dot_nl;
    clip(final_alpha - 0.001f);

    // --- 8. 最終合成 ---
    float4 bg_color = scene_color_texture.Sample(sampler_states[0], screen_uv);
    
    // 環境光 (Ambient) - 真っ暗な場所でもデカールが少し見えるように
    float3 ambientColor = float3(0.03, 0.03, 0.03) * albedo;
    
    float3 final_decal_color = totalDirectLighting + ambientColor;

    // ポストプロセス（トーンマップ等）との調整用
    final_decal_color = pow(final_decal_color, 1.0 / 1.25);

    // 背景色とライティング後のデカール色を補間
    float3 final_rgb = lerp(bg_color.rgb, final_decal_color, final_alpha);

    return float4(final_rgb, 1.0f);
}
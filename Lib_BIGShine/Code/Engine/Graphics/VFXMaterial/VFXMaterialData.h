#pragma once

#include <DirectXMath.h>
#include <cstdint>
#include <string>
#include <vector>

enum class VFX_MATERIAL_BLEND_MODE
{
    // 通常Alpha合成。
    ALPHA,
    // 加算合成。火花や発光系VFXに使う。
    ADDITIVE
};

enum class VFX_MATERIAL_CULL_MODE
{
    NONE,
    BACK,
    FRONT
};

struct VFXMaterialUVData
{
    // Texture UVの拡大縮小とOffset。
    DirectX::XMFLOAT2 scale = { 1.0f, 1.0f };
    DirectX::XMFLOAT2 offset = { 0.0f, 0.0f };
    // 時間経過でUVを流す速度。
    DirectX::XMFLOAT2 scrollSpeed = { 0.0f, 0.0f };
    // UV回転と回転速度。
    float rotation = 0.0f;
    float rotationSpeed = 0.0f;
    DirectX::XMFLOAT2 pivot = { 0.5f, 0.5f };
    float padding = 0.0f;
};

struct VFXMaterialDissolveData
{
    // Noise/Maskを使ったDissolveの有効状態。
    bool enabled = false;
    // 消え始めのしきい値。
    float threshold = 0.0f;
    // Dissolve境界の幅と色。
    float edgeWidth = 0.0f;
    DirectX::XMFLOAT4 edgeColor = { 1.0f, 1.0f, 1.0f, 1.0f };
};

struct VFXMaterialDistortionData
{
    // 背景歪みの有効状態と強さ。
    bool enabled = false;
    float strength = 0.0f;
};

enum class VFX_MATERIAL_COLOR_RAMP_SAMPLE_SOURCE
{
    // Particle/VFXの寿命割合からRampをSampleする。
    LIFE,
    // 経過時間からRampをSampleする。
    TIME,
    // fixedPositionを使って固定色としてSampleする。
    FIXED
};

enum class VFX_MATERIAL_COLOR_RAMP_BLEND_MODE
{
    MULTIPLY,
    OVERRIDE,
    ADD
};

enum class VFX_MATERIAL_COLOR_RAMP_INTERPOLATION
{
    LINEAR,
    CONSTANT,
    SMOOTH
};

enum class VFX_MATERIAL_COLOR_RAMP_WRAP_MODE
{
    CLAMP,
    REPEAT,
    MIRROR
};

enum class VFX_MATERIAL_RANDOM_COLOR_MODE
{
    MULTIPLY,
    OVERRIDE,
    ADD
};

struct VFXMaterialColorRampStopData
{
    // 0-1上のRamp位置。
    float position = 0.0f;
    DirectX::XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
};

struct VFXMaterialColorRampData
{
    // 色変化Rampを使うかどうか。
    bool enabled = false;
    VFX_MATERIAL_COLOR_RAMP_SAMPLE_SOURCE sampleSource = VFX_MATERIAL_COLOR_RAMP_SAMPLE_SOURCE::LIFE;
    VFX_MATERIAL_COLOR_RAMP_BLEND_MODE blendMode = VFX_MATERIAL_COLOR_RAMP_BLEND_MODE::MULTIPLY;
    VFX_MATERIAL_COLOR_RAMP_INTERPOLATION interpolation = VFX_MATERIAL_COLOR_RAMP_INTERPOLATION::LINEAR;
    VFX_MATERIAL_COLOR_RAMP_WRAP_MODE wrapMode = VFX_MATERIAL_COLOR_RAMP_WRAP_MODE::CLAMP;
    // FIXED Sample時に使う位置。
    float fixedPosition = 0.0f;
    // TIME Sample時にRamp位置を進める速度。
    float scrollSpeed = 0.0f;
    // Ramp色を最終色へどれだけ混ぜるか。
    float influence = 1.0f;
    // AlphaにもRampを適用するかどうか。
    bool affectAlpha = true;
    std::vector<VFXMaterialColorRampStopData> stops =
    {
        { 0.0f, { 1.0f, 1.0f, 1.0f, 1.0f } },
        { 1.0f, { 1.0f, 1.0f, 1.0f, 1.0f } },
    };
};

struct VFXMaterialRandomColorData
{
    // InstanceごとのRandom Colorを使うかどうか。
    bool enabled = false;
    VFX_MATERIAL_RANDOM_COLOR_MODE mode = VFX_MATERIAL_RANDOM_COLOR_MODE::MULTIPLY;
    // Random色の範囲。
    DirectX::XMFLOAT4 minColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT4 maxColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    float influence = 1.0f;
    bool affectAlpha = false;
    // Randomの再現性を調整するSeed。
    uint32_t seed = 0u;
};

struct VFXMaterialRenderStateData
{
    // VFX描画時のBlend/Cull/Depth設定。
    VFX_MATERIAL_BLEND_MODE blendMode = VFX_MATERIAL_BLEND_MODE::ALPHA;
    VFX_MATERIAL_CULL_MODE cullMode = VFX_MATERIAL_CULL_MODE::NONE;
    bool depthTest = true;
    bool depthWrite = false;
};

struct VFXMaterialData
{
    // Asset形式Version。
    uint32_t version = 1u;
    // Editor表示名。
    std::string materialName = "VFXMaterial";
    // 各Texture SlotのPath。
    std::string baseTexturePath;
    std::string maskTexturePath;
    std::string noiseTexturePath;
    std::string normalTexturePath;
    std::string distortionTexturePath;
    // 基本色、発光色、Alpha。
    DirectX::XMFLOAT4 baseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT3 emissiveColor = { 1.0f, 1.0f, 1.0f };
    float emissiveIntensity = 0.0f;
    float alpha = 1.0f;
    // Base/NoiseそれぞれのUV制御。
    VFXMaterialUVData mainUV;
    VFXMaterialUVData noiseUV;
    // Dissolve/Distortion/ColorRamp/RandomColorなどのVFX表現設定。
    VFXMaterialDissolveData dissolve;
    VFXMaterialDistortionData distortion;
    VFXMaterialColorRampData colorRamp;
    VFXMaterialRandomColorData randomColor;
    VFXMaterialRenderStateData renderState;
};

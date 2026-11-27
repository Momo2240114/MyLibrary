#ifndef PARTICLE_COMMON_HLSLI
#define PARTICLE_COMMON_HLSLI

#include "Scene.hlsli"

// Particle系Compute Shaderの1グループあたりスレッド数。C++側のNumParticleThreadと合わせる。
static const uint NumParticleThread = 1024;

// Indirect引数バッファ内の各値のバイトオフセット。
static const uint IndirectArgumentsNumCurrentParticle = 0;
static const uint IndirectArgumentsNumPreviousParticle = 4;
static const uint IndirectArgumentsNumDeadParticle = 8;
static const uint IndirectArgumentsEmitParticleDispatchIndirect = 12;
static const uint IndirectArgumentsUpdateParticleDispatchIndirect = 24;
static const uint IndirectArgumentsNumEmitParticleIndex = 36;
static const uint IndirectArgumentsDrawIndirect = 40;

// GPU上で生存しているParticle 1個分の状態。
struct Particle
{
    float4 parameter; // x:スプライト番号 y:残り寿命 z:総寿命 w:描画タイプ。
    float4 position; // ワールド座標。
    uint2 packedColorRandomRates; // 開始色・終了色のRGBA補間率を各8bitで保持する。
    float rotation;
    float angularSpeed;
    float4 scale; // x/y:開始サイズ z:終了サイズ。
    float4 velocity; // 現在速度。
    float4 acceleration; // 基本加速度。
    float4 texcoord; // スプライトシート上のUV領域。
    float4 color; // 現在色。
};

// CPUが作り、Emit_CSがParticleへコピーする生成時データ。
struct EmitParticleData
{
    float4 parameter; // x:スプライト番号 y:初期寿命 z:未使用 w:描画タイプ。
    float4 position; // 生成位置。
    uint2 packedColorRandomRates;
    float rotation;
    float angularSpeed;
    float4 scale; // 開始サイズと終了サイズ。
    float4 velocity; // 初速。
    float4 acceleration; // 基本加速度。
    float4 color; // 初期色。
};

// 生存管理と深度ソート用の補助データ。
struct ParticleHeader
{
    uint alive; // 0なら未使用、1なら生存中。
    uint particleIndex; // Particle配列内の実体番号。
    float depth; // カメラから見た深度。
    uint dummy; // 16byteアライン用。
};

// ParticleSystemConstantsはC++のParticleSystemConstantsと順序を合わせる。
cbuffer ParticleSystemConstants : register(b9)
{
    float4 emissionPosition; // エミッター中心位置。
    float2 emissionOffset; // 旧Point半径Min/Max。
    float2 emissionSize; // 開始サイズ/終了サイズ。
    float2 emissionConeAngle; // Cone方向角度Min/Max。
    float2 emissionSpeed; // 初速Min/Max。
    float2 emissionAngularSpeed; // 回転速度Min/Max。
    float2 lifespan; // 寿命Min/Max。
    float2 spawnDelay; // 予約領域。
    float2 fadeDuration; // フェードイン/アウト時間。
    float4 manualColor; // 固定色。
    float4 colorStart; // 寿命開始色。
    float4 colorEnd; // 寿命終了色。
    float2 sizeRandomStart; // 開始サイズランダム倍率。
    float2 sizeRandomEnd; // 終了サイズランダム倍率。
    float time; // Emitter内時間。
    float deltaTime; // 今フレームの経過時間。
    float noiseScale; // 旧Noise強度互換値。
    float gravity; // Y方向加速度。C++側の初期値は-9.8。

    float noiseFrequency; // Noiseの細かさ。
    float noiseSpeed; // Noiseの流れる速度。
    float noiseStrength; // Noise加速度の強さ。
    float drag; // 空気抵抗。

    float4 windVelocity; // xyz:追加加速度。旧名Windとの互換のため名前を残している。

    float radialVelocity; // 中心から外/内へ向かう速度加算。
    float orbitVelocity; // Y軸周りの周回速度加算。
    float stretchScale; // Stretched Billboardの伸び率。
    float stretchMax; // Stretched Billboardの最大長。

    uint maxParticleCount; // 最大Particle数。
    uint particleRenderType; // 0:通常 1:速度方向Stretch。
    uint emissionMode; // 0:連続 1:Burst。
    uint burstCount; // 発生レートまたはBurst数。
    float burstTrigger; // Burst発火フラグ。
    uint spawnShape; // 発生形状。
    uint useMaskTexture; // Maskを使うか。
    uint useGradientTexture; // Gradientを使うか。
    float4 shapeStart; // Line開始位置。
    float4 shapeEnd; // Line終了位置。
    float4 shapeSize; // Box/Circle/Sphereのサイズ。
    uint useColorOverLife; // 寿命色補間を使うか。
    uint useRandomSize; // サイズランダムを使うか。
    uint useRandomColor;
    uint padding1; // 16byteアライン用。
    uint2 textureSplitCountSetting; // スプライトシート列/行。
    uint2 spriteSheetIndex; // 固定セル座標。
    uint useSpriteAnimation; // スプライトアニメを使うか。
    uint spriteAnimationLoop; // スプライトアニメをループするか。
    uint spriteAnimationStartIndex; // アニメ開始セル。
    uint spriteAnimationEndIndex; // アニメ終了セル。
    float spriteAnimationFps; // スプライトアニメFPS。
    float3 spritePadding; // 16byteアライン用。
    uint useNoiseTexture; // 予約領域。
    float3 padding; // 16byteアライン用。
    float fresnelPower; // 擬似フレネルの鋭さ。
    float fresnelStrength; // 擬似フレネルの強さ。
    uint useFresnel; // 擬似フレネルを使うか。
    uint fresnelPadding; // 16byteアライン用。
    float verticalSpawnRange; // CPU生成時の縦方向発生幅。
    uint spawnDirectionMode; // CPU生成時の初速方向モード。
    float sizeTransitionDuration; // CPUが発生時にParticleのscale.wへ記録する。
    uint randomizeBurstRotation; // CPUがBurstごとの回転オフセットを決める。
    float4 emissionAxis;
    float4 targetPosition;
    float targetAttractionStrength;
    float targetAttractionRange;
    float targetAttractionFalloff;
    uint useTargetAttraction;
    uint gradientSampleMode;
    uint gradientDirection;
    uint useShadeGradientTexture;
    uint gradientPadding0;
    float gradientFixedUv;
    float gradientStartUv;
    float gradientEndUv;
    float shadeGradientFixedUv;
    float shadeGradientStartUv;
    float shadeGradientEndUv;
    uint shadeGradientSampleMode;
    uint shadeGradientDirection;
    float4 randomColorStart;
    float4 randomColorEnd;
};

cbuffer ParticleCommonConstants : register(b10)
{
    float elapsedTime;
    uint2 textureSplitCount;
    uint systemNumParticles;
    uint totalEmitCount;
    uint commonDummy[3];
};

cbuffer ParticleBitonicSortConstants : register(b11)
{
    uint increment;
    uint direction;
    uint sortDummy[2];
};

static const uint BitonicSortB2Thread = 256;
static const uint BitonicSortC2Thread = 512;

struct VS_OUT
{
    uint vertexID : TEXCOORD0;
};

struct GS_OUT
{
    float4 pos : SV_POSITION;
    float4 color : COLOR;
    float2 tex : TEXCOORD0;
    float lifeRate : TEXCOORD1;
    float2 localTex : TEXCOORD2;
};

#endif

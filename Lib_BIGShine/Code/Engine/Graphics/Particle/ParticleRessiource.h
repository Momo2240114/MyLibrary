#pragma once

#include <directxmath.h>
#include <cstdint>
#include <string>
#include <vector>

// Particleの合成方法。描画順や色の足し方を決める。
enum class ParticleBlendMode
{
    ALPHA,      // 通常の半透明合成。煙や薄い残像などに使う。
    ADDITIVE,   // 加算合成。火花や魔法光など、光らせたい表現に使う。
};

// エミッター位置をどの空間として扱うかを表す。
enum class ParticleSpawnSpace
{
    OWNER_RELATIVE, // GameObjectのワールド位置に追従するローカルオフセットとして扱う。
    WORLD,          // 指定値をそのままワールド座標として扱う。
};

// 生成直後の初速方向を決めるモード。
enum class PARTICLE_SPAWN_DIRECTION_MODE : uint32_t
{
    CONE = 0,       // Cone Angleで決めた方向へ飛ばす。
    OUTWARD = 1,    // エミッター中心から外側へ飛ばす。
    INWARD = 2,     // 発生位置からエミッター中心へ向けて飛ばす。
};

// GPU上で生存しているParticle 1個分の状態。
struct Particle
{
    DirectX::XMFLOAT4 parameter = { 0, 0, 0, 0 };      // x:スプライト番号 y:残り寿命 z:総寿命 w:描画タイプ。
    DirectX::XMFLOAT4 position = { 0, 0, 0, 1 };       // ワールド座標。wは同次座標用。
    DirectX::XMFLOAT4 rotation = { 0, 0, 0, 0 };       // z:現在角度 w:角速度。
    DirectX::XMFLOAT4 scale = { 1, 1, 1, 0 };          // x/y:開始サイズ z:終了サイズ。
    DirectX::XMFLOAT4 velocity = { 0, 0, 0, 0 };       // 速度。Update_CSで位置へ積分する。
    DirectX::XMFLOAT4 acceleration = { 0, 0, 0, 0 };   // 基本加速度。重力などを入れる。
    DirectX::XMFLOAT4 texcoord = { 0, 0, 1, 1 };       // スプライトシート内のUV領域。
    DirectX::XMFLOAT4 color = { 1, 1, 1, 1 };          // 現在色。Color Over Lifeで更新される。
};

// CPU側で生成し、ParticleEmit_CSへ渡す初期化データ。
struct EmitParticleData
{
    DirectX::XMFLOAT4 parameter = { 0, 1, 0, 0 };      // x:スプライト番号 y:初期寿命 z:未使用 w:描画タイプ。
    DirectX::XMFLOAT4 position = { 0, 0, 0, 1 };       // 生成時のワールド座標。
    DirectX::XMFLOAT4 rotation = { 0, 0, 0, 0 };       // 生成時角度と角速度。
    DirectX::XMFLOAT4 scale = { 1, 1, 1, 0 };          // 開始サイズと終了サイズ。
    DirectX::XMFLOAT4 velocity = { 0, 0, 0, 0 };       // 生成時の初速。
    DirectX::XMFLOAT4 acceleration = { 0, 0, 0, 0 };   // 生成時の基本加速度。
    DirectX::XMFLOAT4 color = { 1, 1, 1, 1 };          // 生成時の色。
};

// ソートや間接描画で参照するParticleの管理情報。
struct ParticleHeader
{
    uint32_t alive = 0;           // 0なら未使用、1なら生存中。
    uint32_t particleIndex = 0;   // Particle配列内の実体インデックス。
    float depth = 0.0f;           // カメラから見た深度。半透明ソートに使う。
    uint32_t dummy = 0;           // 16byte境界維持用。
};

// Particleシステム全体のパラメータ。C++とHLSLでレイアウトを合わせる必要がある。
struct ParticleSystemConstants
{
    DirectX::XMFLOAT4 emissionPosition = { 0, 0, 0, 1 };      // エミッター中心位置。空間はParticleSpawnSpaceで決まる。
    DirectX::XMFLOAT2 emissionOffset = { 0, 0 };              // Point形状の旧式半径Min/Max。
    DirectX::XMFLOAT2 emissionSize = { 1, 1 };                // 開始サイズ/終了サイズ。
    DirectX::XMFLOAT2 emissionConeAngle = { 0, 0 };           // Cone方向の角度Min/Max。
    DirectX::XMFLOAT2 emissionSpeed = { 0, 0 };               // 初速Min/Max。
    DirectX::XMFLOAT2 emissionAngularSpeed = { 0, 0 };        // 回転速度Min/Max。
    DirectX::XMFLOAT2 lifespan = { 1, 1 };                    // 寿命Min/Max。
    DirectX::XMFLOAT2 spawnDelay = { 0, 0 };                  // 予約領域。発生遅延を拡張するための値。
    DirectX::XMFLOAT2 fadeDuration = { 0, 0 };                // フェードイン/アウト時間。
    DirectX::XMFLOAT4 manualColor = { 1, 1, 1, 1 };           // Color Over Life未使用時の固定色。
    DirectX::XMFLOAT4 colorStart = { 1, 1, 1, 1 };            // 寿命開始時の色。
    DirectX::XMFLOAT4 colorEnd = { 1, 1, 1, 1 };              // 寿命終了時の色。
    DirectX::XMFLOAT2 sizeRandomStart = { 1, 1 };             // 開始サイズに掛けるランダム倍率Min/Max。
    DirectX::XMFLOAT2 sizeRandomEnd = { 1, 1 };               // 終了サイズに掛けるランダム倍率Min/Max。

    float time = 0.0f;
    float deltaTime = 0.0f;
    float noiseScale = 0.0f;      // 互換用: 旧Noise Scale。0以外ならnoiseStrengthとして扱う
    float gravity = -9.8f;       // Y方向の加速度。物理寄りの初期値: -9.8m/s^2

    float noiseFrequency = 1.0f;  // ノイズの細かさ。大きいほど細かく揺れる
    float noiseSpeed = 1.0f;      // ノイズ場のスクロール速度
    float noiseStrength = 0.0f;   // ノイズ加速度の強さ。旧noiseScaleよりこっちを使う
    float drag = 0.0f;            // 空気抵抗。0=なし、値が大きいほど減速

    DirectX::XMFLOAT4 windVelocity = { 0, 0, 0, 0 }; // xyz: 追加加速度。旧名Wind。Acceleration XYZとしてGUI表示

    float radialVelocity = 0.0f;  // エミッター中心から外へ押し出す速度加算
    float orbitVelocity = 0.0f;   // Y軸周りの回転速度加算
    float stretchScale = 0.04f;   // Stretched Billboardの伸び率
    float stretchMax = 100.0f;    // Stretched Billboardの最大長

    uint32_t maxParticleCount = 1000;     // GPU上で確保する最大Particle数。
    uint32_t type = 0;                    // 0:通常Billboard 1:速度方向に伸びるBillboard。
    uint32_t emissionMode = 0;            // 0:連続発生 1:Burst発生。
    uint32_t burstCount = 0;              // 連続時は発生レート、Burst時は発生数。

    float burstTrigger = 0.0f;            // 0->1でBurst発火を通知する。
    uint32_t spawnShape = 0;              // 0:Point 1:Box 2:Circle 3:Sphere 4:Line 5:MeshVertices。
    uint32_t useMaskTexture = 0;          // Mask Textureを使用するか。
    uint32_t useGradientTexture = 0;      // Gradient Textureを使用するか。

    DirectX::XMFLOAT4 shapeStart = { 0, 0, 0, 1 };    // Line発生形状の開始位置。
    DirectX::XMFLOAT4 shapeEnd = { 0, 0, 0, 1 };      // Line発生形状の終了位置。
    DirectX::XMFLOAT4 shapeSize = { 1, 1, 1, 0 };     // Box/Circle/Sphereの大きさ。

    uint32_t useColorOverLife = 0;        // 寿命に応じた色補間を行うか。
    uint32_t useRandomSize = 0;           // サイズにランダム倍率を掛けるか。
    uint32_t padding0 = 0;                // HLSLとの16byteアライン用。
    uint32_t padding1 = 0;                // HLSLとの16byteアライン用。

    DirectX::XMUINT2 textureSplitCount = { 1, 1 };    // スプライトシートの列数/行数。
    DirectX::XMUINT2 spriteSheetIndex = { 0, 0 };     // 固定表示するセル座標。

    uint32_t useSpriteAnimation = 0;       // スプライトシートを時間で再生するか。
    uint32_t spriteAnimationLoop = 1;      // スプライトアニメーションをループするか。
    uint32_t spriteAnimationStartIndex = 0;// 再生開始セル番号。
    uint32_t spriteAnimationEndIndex = 0;  // 再生終了セル番号。

    float spriteAnimationFps = 12.0f;      // スプライトアニメーションの再生FPS。
    DirectX::XMFLOAT3 spritePadding = { 0, 0, 0 };    // 16byteアライン用。

    uint32_t useNoiseTexture = 0;          // 予約領域。Texture Noise拡張用。
    DirectX::XMFLOAT3 padding = { 0, 0, 0 };           // 16byteアライン用。

    float fresnelPower = 2.0f;             // Billboard擬似フレネルの鋭さ。
    float fresnelStrength = 1.0f;          // Billboard擬似フレネルの強さ。
    uint32_t useFresnel = 0;               // 擬似フレネルを使用するか。
    uint32_t fresnelPadding = 0;           // 16byteアライン用。

    float verticalSpawnRange = 0.0f;       // Point/Circle/Mesh spawnに追加するY方向のランダム幅
    uint32_t spawnDirectionMode = 0;       // PARTICLE_SPAWN_DIRECTION_MODE
    uint32_t cpuSpawnPadding0 = 0;
    uint32_t cpuSpawnPadding1 = 0;
};
static_assert(sizeof(ParticleSystemConstants) % 16 == 0);

struct particle_system_settings
{
    ParticleSystemConstants data;
    std::string texture_path;
    std::string color_temper_path;
};

#pragma once

#include <DirectXMath.h>
#include <d3d11.h>
#include <memory>
#include <string>
#include <vector>
#include <wrl.h>

class ModelResource;
class GltfModelCommponennt;

// MeshVFXの合成方法。
enum class MeshVFXBlendMode
{
    ALPHA,      // 通常半透明。透明な膜やオーバーレイ向き。
    ADDITIVE,   // 加算合成。発光エフェクトや斬撃光向き。
};

// 複数Mask Textureを合成するときの方法。
enum class MeshVFXMaskBlendMode
{
    MULTIPLY,   // 既存Maskに掛け合わせる。くり抜きを強める。
    UNION,      // 既存Maskと最大値合成する。範囲を広げる。
};

// Edge HighlightにTexture模様を混ぜる方法。
enum class MESH_VFX_EDGE_PATTERN_MODE
{
    NONE,           // 単色のEdge Highlight。
    BASE_TEXTURE,   // Base Textureの明度でEdgeに模様を入れる。
    MASK_TEXTURE,   // Mask Textureの値でEdgeに模様を入れる。
};

enum class MESH_VFX_GRADIENT_SAMPLE_MODE
{
    HORIZONTAL,
    VERTICAL,
};

enum class MESH_VFX_GRADIENT_DIRECTION
{
    FORWARD,
    REVERSE,
};

// Mask Texture 1枚分の情報。
struct MeshVFXMaskLayer
{
    std::string path;   // Texture Assetのパス。
    MeshVFXMaskBlendMode blendMode = MeshVFXMaskBlendMode::MULTIPLY; // 合成方法。
    bool invert = false; // trueなら白黒を反転してから合成する。
};

// MeshVFXの1パーツ分の編集データ。モデル、Texture、UV、発光、歪み、Edgeなどをまとめる。
struct MeshVFXPart
{
    std::string partAssetPath; // 単体Part Assetとして保存したJSONパス。
    std::string modelPath;     // 表示に使うモデルAssetパス。
    int meshIndex = -1;        // 描画するMesh番号。-1なら全Mesh対象。
    bool renderEnabled = false;// falseならこのPartは描画しない。
    bool useMaterialBaseTexture = true;  // BaseTexture未指定時にモデルMaterialのBaseMapを使う。
    bool useMaterialNormalTexture = true;// NormalTexture未指定時にモデルMaterialのNormalMapを使う。

    DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };          // Partのローカル位置。
    DirectX::XMFLOAT3 rotation = { 0.0f, 0.0f, 0.0f };          // Partのローカル回転。度数法。
    DirectX::XMFLOAT3 scale = { 1.0f, 1.0f, 1.0f };             // Partのローカル拡縮。
    float shapeCylinderRadius = 1.0f;
    DirectX::XMFLOAT3 meshRotationSpeed = { 0.0f, 0.0f, 0.0f }; // 時間で加算する回転速度。度/秒。

    float duration = 0.0f;            // 0なら時間制限なし。正の値ならPartの再生長。
    bool loop = true;                 // duration到達後に時間をループするか。
    bool animateScale = false;        // 時間でscaleStartからscaleEndへ補間するか。
    DirectX::XMFLOAT3 scaleStart = { 1.0f, 1.0f, 1.0f }; // Scaleアニメーション開始値。
    DirectX::XMFLOAT3 scaleEnd = { 1.0f, 1.0f, 1.0f };   // Scaleアニメーション終了値。
    bool animateAlpha = false;        // 時間でalphaStartからalphaEndへ補間するか。
    float alphaStart = 1.0f;          // Alphaアニメーション開始値。
    float alphaEnd = 1.0f;            // Alphaアニメーション終了値。

    std::string baseTexturePath;              // 色・模様の主Textureパス。
    std::string normalTexturePath;            // 法線Textureパス。
    std::string maskTexturePath;              // 旧互換用Mask Textureパス。
    std::string noiseTexturePath;             // 旧互換用Noise Textureパス。
    std::string distortionNoiseTexturePath;   // 背景歪みに使うNoise Textureパス。
    std::string vertexNoiseTexturePath;       // 頂点揺らしに使うNoise Textureパス。
    std::string edgePatternTexturePath;
    std::string emissiveTexturePath;
    std::string gradientTexturePath;
    std::vector<MeshVFXMaskLayer> maskLayers; // 複数Maskの合成設定。
    std::vector<std::string> noiseTexturePaths;// 複数Noiseの平均合成用Textureパス。

    DirectX::XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, 1.0f }; // BaseTextureに掛ける色。
    DirectX::XMFLOAT2 baseUVSpeed = { 0.0f, -0.5f };      // BaseTextureのUVスクロール速度。
    DirectX::XMFLOAT2 maskUVSpeed = { 0.0f, -0.2f };      // MaskTextureのUVスクロール速度。
    DirectX::XMFLOAT2 noiseUVSpeed = { 0.2f, -0.8f };     // NoiseTextureのUVスクロール速度。
    DirectX::XMFLOAT2 baseUVScale = { 1.0f, 1.0f };
    DirectX::XMFLOAT2 maskUVScale = { 1.0f, 1.0f };
    DirectX::XMFLOAT2 noiseUVScale = { 1.0f, 1.0f };
    DirectX::XMFLOAT2 normalUVScale = { 1.0f, 1.0f };
    DirectX::XMFLOAT2 distortionNoiseUVScale = { 1.0f, 1.0f };
    DirectX::XMFLOAT2 edgePatternUVScale = { 1.0f, 1.0f };
    DirectX::XMFLOAT2 emissiveUVScale = { 1.0f, 1.0f };
    DirectX::XMFLOAT2 uvRotationCenter = { 0.5f, 0.5f };  // UV回転・放射拡縮の中心。
    float baseUVRotation = 0.0f;          // BaseTextureの初期UV回転。度数法。
    float maskUVRotation = 0.0f;          // MaskTextureの初期UV回転。度数法。
    float noiseUVRotation = 0.0f;         // NoiseTextureの初期UV回転。度数法。
    float baseUVRotationSpeed = 0.0f;     // BaseTextureのUV回転速度。度/秒。
    float maskUVRotationSpeed = 0.0f;     // MaskTextureのUV回転速度。度/秒。
    float noiseUVRotationSpeed = 0.0f;    // NoiseTextureのUV回転速度。度/秒。
    float uvRadialScale = 1.0f;           // UV中心からの放射拡縮倍率。
    float uvRadialScaleSpeed = 0.0f;      // 放射拡縮倍率の時間変化。

    float alpha = 1.0f;                  // Part全体の透明度。
    float emissiveStrength = 3.0f;       // 予約値。発光強度としてAssetへ保持する。
    float emissivePulseSpeed = 2.0f;     // 予約値。発光パルス速度としてAssetへ保持する。
    float emissivePulseAmount = 0.35f;   // 予約値。発光パルス量としてAssetへ保持する。
    float normalStrength = 1.0f;         // Normal TextureのXY成分強度。
    float noiseStrength = 0.05f;         // NoiseでBase/Mask UVを歪ませる強さ。
    float vertexNoiseHorizontalStrength = 0.1f; // 頂点を法線横方向へ揺らす強さ。
    float vertexNoiseVerticalStrength = 0.1f;   // 頂点をY方向へ揺らす強さ。
    float vertexNoiseHorizontalScale = 1.0f;    // 横方向頂点Noiseの細かさ。
    float vertexNoiseVerticalScale = 1.0f;      // 縦方向頂点Noiseの細かさ。
    DirectX::XMFLOAT3 vertexNoiseSpeed = { 0.0f, 1.0f, 0.0f }; // 頂点Noiseのスクロール速度。
    float maskDistortionStrength = 8.0f; // Mask外側で背景を歪ませる画素単位の強さ。
    float maskDistortionOpacity = 0.5f;  // 歪み部分のAlpha。
    DirectX::XMFLOAT4 edgeColor = { 0.25f, 0.75f, 1.0f, 1.0f }; // Mesh外周強調の色。
    float silhouetteEdgeStrength = 0.0f; // 視線に対して外周に見える面を強調する強さ。
    float silhouetteEdgePower = 3.0f;    // シルエット強調の細さ。大きいほど細い。
    float intersectionEdgeStrength = 0.0f;// 深度差で交差部分を強調する強さ。
    float intersectionEdgeWidth = 0.002f;// 交差判定の幅。
    float edgePatternStrength = 0.0f;    // EdgeにTexture模様を混ぜる割合。
    float fresnelPower = 2.0f;           // Fresnelの鋭さ。
    float fresnelStrength = 1.0f;        // Fresnelの強さ。
    float dissolveThreshold = 0.0f;      // Mask/Alphaがこの値以下なら破棄する。
    float gradientFixedUv = 0.5f;
    float gradientStartUv = 0.0f;
    float gradientEndUv = 1.0f;

    bool useMask = false;                // Mask Textureで表示範囲を制御するか。
    bool useNoise = false;               // Noise TextureでUVを歪ませるか。
    bool useVertexNoise = false;         // Vertex ShaderでMesh頂点を揺らすか。
    bool useMaskDistortion = false;      // Maskで隠れる領域に背景歪みを出すか。
    bool useEdgeHighlight = false;       // Mesh表面内のシルエット/交差周辺を強調するか。
    bool useNormal = false;              // Normal Textureで法線を補正するか。
    bool useFresnel = false;             // 視線角によるFresnel発光を使うか。
    bool useGradientTexture = false;
    MESH_VFX_EDGE_PATTERN_MODE edgePatternMode = MESH_VFX_EDGE_PATTERN_MODE::NONE; // Edge模様の参照元。
    MESH_VFX_GRADIENT_SAMPLE_MODE gradientSampleMode = MESH_VFX_GRADIENT_SAMPLE_MODE::HORIZONTAL;
    MESH_VFX_GRADIENT_DIRECTION gradientDirection = MESH_VFX_GRADIENT_DIRECTION::FORWARD;
    MeshVFXBlendMode blendMode = MeshVFXBlendMode::ALPHA; // 描画時の合成方法。
};

// 読み込んだGPUリソース。MeshVFXPartは保存データ、この構造体は実行時キャッシュ。
struct MeshVFXRuntimePart
{
    std::shared_ptr<ModelResource> modelResource; // 描画対象モデル。
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> baseTexture; // Base TextureのSRV。
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> normalTexture; // Normal TextureのSRV。
    std::vector<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> maskTextures; // 合成前Mask Texture群。
    std::vector<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> noiseTextures; // 合成前Noise Texture群。
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> distortionNoiseTexture; // 背景歪み用Noise。
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> vertexNoiseTexture; // 頂点揺らし用Noise。
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> edgePatternTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> emissiveTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> gradientTexture;
};

// Trailなどが実行時に作ったMeshをMeshVFXとして描くためのバッファ参照。
// バッファの所有権は派生Component側にあり、RenderSystemは借りて描画するだけ。
struct MeshVFXDynamicMesh
{
    ID3D11Buffer* vertexBuffer = nullptr; // 動的Meshの頂点バッファ。
    ID3D11Buffer* indexBuffer = nullptr;  // 動的MeshのIndexバッファ。
    UINT indexCount = 0;                  // 描画するIndex数。
    UINT vertexStride = 0;                // 頂点1個分のバイトサイズ。
};

// Afterimageなど、モデル姿勢を複製して描くためのPoseデータ。
struct MeshVFXModelPose
{
    std::shared_ptr<GltfModelCommponennt> model; // 姿勢を持つモデル。
    std::vector<std::vector<DirectX::XMFLOAT4X4>> meshTransforms; // Meshごとのボーン行列。
    float time = 0.0f;  // Poseごとの経過時間。Shaderの時間演出に渡す。
    float alpha = 1.0f; // Poseごとの透明度。
};

// MeshVFXModelPoseをコピーせずに参照する描画用ビュー。
struct MeshVFXModelPoseView
{
    std::shared_ptr<GltfModelCommponennt> model; // 描画元モデル。
    const std::vector<std::vector<DirectX::XMFLOAT4X4>>* meshTransforms = nullptr; // 外部所有のボーン行列。
    float time = 0.0f;  // インスタンス時間。
    float alpha = 1.0f; // インスタンス透明度。
};

// MeshVFX Asset全体。複数Partを1つのVFXとして扱う。
struct MeshVFXAsset
{
    std::vector<MeshVFXPart> parts; // 再生・描画するPart一覧。
};

#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "DecalManager.h"

#include <memory>
#include <string>
#include <vector>

class VFXMaterialResource;

class DecalComponent : public Component
{
public:
    struct DecalConstantBuffer
    {
        // ピクセルシェーダー側でワールド座標をデカール空間へ戻し、
        // 投影範囲内かどうかを判定するための逆変換行列。
        DirectX::XMMATRIX decal_inverse_transform;

        // デカールの向き情報。投影方向やフェード計算をシェーダーへ渡す。
        DirectX::XMFLOAT4 decal_direction;
    };

    const std::string& GetName() const override { return componentName; }

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture;
    DirectX::XMFLOAT4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT4X4 invWorldMatrix;

    void Start() override;

    // テクスチャだけで描画する旧形式のデカール設定。
    void SetTexture(const std::string& path_);
    void SetGroundSurface(float emissiveStrength_, const std::string& maskPath_);

    // VFXMaterial を 1 つ割り当て、色・ブレンド・歪みなどをまとめて反映する。
    void SetMaterial(const std::string& path_);

    // 時間経過に応じて複数の VFXMaterial を切り替えるためのリスト設定。
    void SetMaterials(const std::vector<std::string>& paths_);
    const std::string& GetMaterialPath() const { return materialPath; }
    const std::string& GetTexturePath() const { return texturePath; }

    // lifeTime_ が 0 以下なら永続、正の値なら fadeOutDuration_ を使って終端で透明化する。
    void SetLifeTime(float lifeTime_, float fadeOutDuration_ = 0.0f);
    void SetDestroyOnLifeEnd(bool destroyOnLifeEnd_)
    {
        destroyOnLifeEnd = destroyOnLifeEnd_;
    }

    // 再利用するデカールを先頭フレームから再生し直す。
    void ResetLifeTime();

    // 現在の Transform と Material 状態を DecalData に詰め、描画キューへ登録する。
    void SubmitDecal(DecalManager& manager_);
    void Update(float elapsedTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

private:
    // VFXMaterialResource の描画設定を DecalData へ焼き込み、Manager 側で扱える形にする。
    void ApplyMaterialToDecalData(DecalData& data_) const;

    // 旧 texturePath 指定を VFXMaterial ベースの描画情報へ変換できるか試す。
    bool ConvertToMaterial();

private:
    // エディタ確認用に、デカールの投影ボックスを表示するかどうか。
    bool showDebugBounds = true;
    bool groundSurface = false;
    float groundEmissiveStrength = 0.0f;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> groundMask;

    // ライフタイム終了時に GameObject ごと破棄するか、描画だけ止めるかを切り替える。
    bool destroyOnLifeEnd = false;

    // デカールの生存時間とフェード時間。0 以下の lifetime は時間制限なしとして扱う。
    float lifeTime = 0.0f;
    float fadeOutDuration = 0.0f;
    float elapsedTime = 0.0f;

    // VFXMaterial の UV アニメーションや連番切り替えに使う再生時間。
    float materialTime = 0.0f;

    // フェードアウト時に元の透明度から自然に減衰させるための基準値。
    float initialAlpha = 1.0f;
    static inline const std::string componentName = "DecalComponent";

    // Serialize 用に最後に指定されたアセットパスを保持する。
    std::string texturePath;
    std::string materialPath;
    std::vector<std::string> materialPaths;

    // 実際の描画時に参照する VFXMaterial。複数指定時は materialTime で選択する。
    std::shared_ptr<VFXMaterialResource> materialResource;
    std::vector<std::shared_ptr<VFXMaterialResource>> materialResources;
};

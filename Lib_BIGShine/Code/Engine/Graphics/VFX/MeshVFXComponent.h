#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "MeshVFXResource.h"

#include <nlohmann/json_fwd.hpp>
#include <string>
#include <vector>

// Meshを使ったVFXをGameObject上で再生・編集する基底Component。
// 通常のMeshVFX、WeaponTrail、AfterimageなどはこのAsset/Runtime管理を共有する。
class MeshVFXComponent : public Component
{
public:
    MeshVFXComponent() = default;


    const std::string& GetName() const override
    {
        static std::string name = "MeshVFXComponent";
        return name;
    }

    void Start() override;
    void Update(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;
    bool SaveAsset(const std::string& path_) const;
    bool LoadAsset(const std::string& path_);
    bool SavePartAsset(size_t partIndex_, const std::string& path_) const;
    bool LoadPartAsset(size_t partIndex_, const std::string& path_);
    void SetAsset(const MeshVFXAsset& asset_);
    void RebuildRuntimeResources();

    void Play();
    void Stop();
    void Reset();
    void SetPlaybackSpeed(float playbackSpeed_) { playbackSpeed = playbackSpeed_; }
    float GetPlaybackSpeed() const { return playbackSpeed; }

    MeshVFXAsset& GetAsset() { return asset; }
    const MeshVFXAsset& GetAsset() const { return asset; }
    const std::vector<MeshVFXRuntimePart>& GetRuntimeParts() const { return runtimeParts; }
    bool HasRuntimeResources() const;
    virtual size_t GetMaxRenderParts() const { return static_cast<size_t>(-1); }

    std::vector<std::string> GetCurveTrackNames() const;
    float EvaluateCurveValue(const std::string& curveName_, float normalizedTime_, float fallbackValue_) const;

    float GetTime() const { return time; }
    bool IsPlaying() const { return isPlaying; }
    // Assetを上書きせず、環境演出中だけ大きさと色を差し替える。
    void SetRuntimeAppearance(const DirectX::XMFLOAT3& scale_, const DirectX::XMFLOAT4& color_, const DirectX::XMFLOAT4& edgeColor_)
    {
        runtimeAppearanceEnabled = true;
        runtimeAppearanceScale = scale_;
        runtimeAppearanceColor = color_;
        runtimeAppearanceEdgeColor = edgeColor_;
    }
    void ClearRuntimeAppearance() { runtimeAppearanceEnabled = false; }
    bool HasRuntimeAppearance() const { return runtimeAppearanceEnabled; }
    const DirectX::XMFLOAT3& GetRuntimeAppearanceScale() const { return runtimeAppearanceScale; }
    const DirectX::XMFLOAT4& GetRuntimeAppearanceColor() const { return runtimeAppearanceColor; }
    const DirectX::XMFLOAT4& GetRuntimeAppearanceEdgeColor() const { return runtimeAppearanceEdgeColor; }
    virtual bool UsesOwnerModel() const { return false; }
    virtual bool UsesModelPoses() const { return false; }
    virtual bool HasModelPoses() const { return false; }
    virtual bool GetDynamicMesh(MeshVFXDynamicMesh& mesh_) const
    {
        mesh_ = {};
        return false;
    }
    virtual bool GetModelPoses(std::vector<MeshVFXModelPose>& poses_) const
    {
        poses_.clear();
        return false;
    }
    virtual bool GetModelPoseViews(std::vector<MeshVFXModelPoseView>& poses_) const
    {
        poses_.clear();
        return false;
    }

private:
    void RefreshAssetLists();
    void LoadPartResources();
    void LoadPartResource(size_t partIndex_);
    bool ConvertPartToMaterial(size_t partIndex_);
    void LoadTexture(
        ID3D11Device* device_,
        const std::string& path_,
        UINT fallbackColor_,
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& texture_);

private:
    MeshVFXAsset asset;                          // 保存対象となるMeshVFX設定。
    std::vector<MeshVFXRuntimePart> runtimeParts;// asset.partsに対応するGPUリソースキャッシュ。
    std::vector<std::string> modelAssetPaths;    // GUI候補として列挙したモデルAssetパス。
    std::vector<std::string> textureAssetPaths;  // GUI候補として列挙したTexture Assetパス。
    std::string assetPath;                       // 現在読み書きしているMeshVFX JSONパス。
    float time = 0.0f;                           // Component全体の再生時間。
    float playbackSpeed = 1.0f;                  // Component全体の再生速度。
    bool isPlaying = true;                       // trueならUpdateでtimeを進める。
    bool runtimeAppearanceEnabled = false;
    DirectX::XMFLOAT3 runtimeAppearanceScale{ 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT4 runtimeAppearanceColor{ 1.0f, 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT4 runtimeAppearanceEdgeColor{ 1.0f, 1.0f, 1.0f, 1.0f };
};

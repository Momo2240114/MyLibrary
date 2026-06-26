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

    MeshVFXAsset& GetAsset() { return asset; }
    const MeshVFXAsset& GetAsset() const { return asset; }
    const std::vector<MeshVFXRuntimePart>& GetRuntimeParts() const { return runtimeParts; }
    virtual size_t GetMaxRenderParts() const { return static_cast<size_t>(-1); }

    float GetTime() const { return time; }
    bool IsPlaying() const { return isPlaying; }
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
    bool isPlaying = true;                       // trueならUpdateでtimeを進める。
};

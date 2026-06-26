#pragma once

#include "MeshVFXComponent.h"

#include <vector>

class GameObject;

class AfterimageComponent : public MeshVFXComponent
{
public:
    AfterimageComponent();

    const std::string& GetName() const override
    {
        static std::string name = "AfterimageComponent";
        return name;
    }

    void Start() override;
    void Update(float deltaTime_) override;
    void LateUpdate(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    bool UsesOwnerModel() const override { return true; }
    bool UsesModelPoses() const override { return true; }
    bool HasModelPoses() const override { return !snapshots.empty(); }
    size_t GetMaxRenderParts() const override;
    bool GetModelPoseViews(std::vector<MeshVFXModelPoseView>& poses_) const override;

    void StartEmission();
    void StopEmission();
    void Burst();
    void Clear();

private:
    struct Snapshot
    {
        std::vector<MeshVFXModelPose> poses;
        float age = 0.0f;
    };

    void RegisterActions();
    void CaptureSnapshot();
    void CaptureObject(const std::shared_ptr<GameObject>& object_, Snapshot& snapshot_);

    std::vector<Snapshot> snapshots;
    float spawnInterval = 0.06f;
    float lifetime = 0.4f;
    float startAlpha = 0.8f;
    float spawnTimer = 0.0f;
    int maxSnapshots = 8;
    int maxRenderSnapshots = 4;
    int maxRenderParts = 1;
    int burstCount = 3;
    int pendingBurstCount = 0;
    bool includeChildren = true;
    bool isEmitting = false;
};

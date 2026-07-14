#pragma once

#include "ColliderComponent.h"

class MeshCollider : public ColliderComponent
{
public:
    bool convex = false;

    MeshCollider()
    {
        isStatic = true;
        useGravity = false;
        physicsControlsTransform = false;
        requiredForSceneStart = true;
    }

    const std::string& GetName() const override
    {
        static std::string name = "MeshCollider";
        return name;
    }

    ColliderType GetColliderType() const override { return ColliderType::Mesh; }
    void Start() override;
    void Gui() override;
    void Update(float dt_) override;
    bool IsLoadReady() const override;
    bool IsRuntimeReady() const override;
    bool HasRuntimeFailure() const override;

    void Serialize(nlohmann::json& j_) const override
    {
        ColliderComponent::Serialize(j_);
        j_["Convex"] = convex;
    }

    void Deserialize(const nlohmann::json& j_) override
    {
        ColliderComponent::Deserialize(j_);
        convex = j_.value("Convex", false);
        if (!convex) isStatic = true;
        waitingForModel = true;
    }

private:
    bool CanBuildMesh() const;
    bool IsCollisionBuilt() const;
    bool waitingForModel = true;
};

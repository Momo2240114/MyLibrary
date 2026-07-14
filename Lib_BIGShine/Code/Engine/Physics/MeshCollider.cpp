#include "MeshCollider.h"

#include "Engine/Debug/TracyProfiler.h"
#include "Engine/Graphics/Resource/GlTFModel.h"

void MeshCollider::Start()
{
    BS_PROFILE_SCOPE("MeshCollision::BuildRuntimeCollision");
    if (!CanBuildMesh())
    {
        waitingForModel = true;
        return;
    }

    waitingForModel = false;
    {
        BS_PROFILE_SCOPE("MeshCollision::RegisterPhysics");
        ColliderComponent::Start();
    }
}

void MeshCollider::Update(float)
{
    if (!waitingForModel && IsCollisionBuilt())
    {
        return;
    }

    if (!CanBuildMesh())
    {
        waitingForModel = true;
        return;
    }

    waitingForModel = false;
    BS_PROFILE_SCOPE("MeshCollision::BuildRuntimeCollision");
    RecreatePhysXActor();
}

void MeshCollider::Gui()
{
    ColliderComponent::Gui();

    ImGui::TextDisabled("Uses the GltfModel meshes on this GameObject.");
    ImGui::TextDisabled("Triangle Mesh is static-only. Use Convex for moving objects.");
    ImGui::TextDisabled("Animated/skinned deformation is not rebuilt automatically.");
    if (ImGui::Checkbox("Convex Mesh", &convex))
    {
        if (!convex) isStatic = true;
        RecreatePhysXActor();
    }
    if (ImGui::Button("Rebuild Mesh Collider"))
    {
        RecreatePhysXActor();
    }
}

bool MeshCollider::CanBuildMesh() const
{
    auto owner_ = gameObject.lock();
    if (!owner_) return false;

    auto model_ = owner_->GetComponent<GltfModelCommponennt>();
    if (model_) model_->UpdateLoadState();
    return model_ && model_->IsModelReady() && !model_->GetMeshes().empty();
}

bool MeshCollider::IsCollisionBuilt() const
{
    return actor && actor->getNbShapes() > 0;
}

bool MeshCollider::IsLoadReady() const
{
    return CanBuildMesh();
}

bool MeshCollider::IsRuntimeReady() const
{
    return CanBuildMesh() && IsCollisionBuilt();
}

bool MeshCollider::HasRuntimeFailure() const
{
    auto owner_ = gameObject.lock();
    if (!owner_) return true;

    auto model_ = owner_->GetComponent<GltfModelCommponennt>();
    return !model_ || model_->HasRuntimeFailure();
}

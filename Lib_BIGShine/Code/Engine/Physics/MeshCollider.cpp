#include "MeshCollider.h"

#include "Engine/Graphics/Resource/GlTFModel.h"

void MeshCollider::Start()
{
    if (!CanBuildMesh())
    {
        waitingForModel = true;
        return;
    }

    waitingForModel = false;
    ColliderComponent::Start();
}

void MeshCollider::Update(float)
{
    if (!waitingForModel && actor && actor->getNbShapes() > 0)
    {
        return;
    }

    if (!CanBuildMesh())
    {
        waitingForModel = true;
        return;
    }

    waitingForModel = false;
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
    return model_ && !model_->GetMeshes().empty();
}

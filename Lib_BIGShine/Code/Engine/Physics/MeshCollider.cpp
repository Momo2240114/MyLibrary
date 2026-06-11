#include "MeshCollider.h"

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

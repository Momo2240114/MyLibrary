#include "PrecompiledHeader.h"

#include "MeshCollider.h"

#include "Engine/Debug/TracyProfiler.h"
#include "Engine/Graphics/Resource/GlTFModel.h"

void MeshCollider::Start()
{
    // MeshColliderはModel AssetのLoad完了後でないとPhysX Meshを作れない。
    // Start時点でまだModelが未準備なら、Updateで再確認するためwaitingForModelにして抜ける。
    BS_PROFILE_SCOPE("MeshCollision::BuildRuntimeCollision");
    if (!CanBuildMesh())
    {
        waitingForModel = true;
        return;
    }

    waitingForModel = false;
    {
        // Modelが準備できた時だけColliderComponent::Startへ進み、PhysX Actor/Shapeを作る。
        BS_PROFILE_SCOPE("MeshCollision::RegisterPhysics");
        ColliderComponent::Start();
    }
}

void MeshCollider::Update(float)
{
    // すでにColliderが作られているなら何もしない。
    // 非同期Model Load待ちの場合だけ、毎Frame準備完了を確認してColliderを構築する。
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
    // Model Load完了後にPhysX Actorを作り直し、Scene中でColliderを有効化する。
    RecreatePhysXActor();
}

void MeshCollider::Gui()
{
    // MeshColliderのPhysX制約をEditor上で明示し、Convex切り替えや手動再構築を行う。
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
    // 同じGameObjectのGltfModelがReadyになり、Mesh配列を持っている時だけCollider作成可能。
    auto owner_ = gameObject.lock();
    if (!owner_) return false;

    auto model_ = owner_->GetComponent<GltfModelCommponennt>();
    if (model_) model_->UpdateLoadState();
    return model_ && model_->IsModelReady() && !model_->GetMeshes().empty();
}

bool MeshCollider::IsCollisionBuilt() const
{
    // ActorだけでなくShapeが存在することまで確認し、空のActorをReady扱いしない。
    return actor && actor->getNbShapes() > 0;
}

bool MeshCollider::IsLoadReady() const
{
    // Scene Loading中はModel Meshが読めるまで待つ。
    return CanBuildMesh();
}

bool MeshCollider::IsRuntimeReady() const
{
    // Loadできるだけでなく、実際にPhysX Shapeが作られていることをRuntime Ready条件にする。
    return CanBuildMesh() && IsCollisionBuilt();
}

bool MeshCollider::HasRuntimeFailure() const
{
    // 必須Modelが存在しない、またはModel側がLoad失敗した場合はScene Runtime失敗として扱う。
    auto owner_ = gameObject.lock();
    if (!owner_) return true;

    auto model_ = owner_->GetComponent<GltfModelCommponennt>();
    return !model_ || model_->HasRuntimeFailure();
}

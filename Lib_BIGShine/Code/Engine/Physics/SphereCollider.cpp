#include "BoxCollider.h"
#include "SphereCollider.h"
#include "CapsuleCollider.h"
#include "Engine/Math/CollisionMath.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"

void SphereCollider::Gui() {
    ColliderComponent::Gui();
    ImGui::Checkbox("Is Trigger", &isTrigger);
    ImGui::DragFloat("Radius", &radius, 0.01f, 0.0f, 10.0f);
}

void SphereCollider::Update(float dt) {

    auto& renderer = GraphicsDeviceManager::Instance().GetShapeRendererSystem();
    auto t = GetGameObject()->transform;

    // ワールド座標 + オフセット
    DirectX::XMFLOAT3 pos = t->GetWorldPosition();
    pos.x += offset.x; pos.y += offset.y; pos.z += offset.z;

    float worldRadius = radius * t->GetWorldScale().x;
    if(shapeRenderEnabled)
    {
        renderer.DrawSphere(pos, worldRadius, isTrigger ? DirectX::XMFLOAT4(0, 1, 1, 1) : DirectX::XMFLOAT4(0, 1, 0, 1));
    }
}

CollisionManifold SphereCollider::CheckCollision(std::shared_ptr<ColliderComponent> other)
{
    Sphere self = GetSphereData();
    switch (other->GetColliderType()) {
    case ColliderType::Sphere:
        return CollisionMath::SphereVsSphere(self, ((SphereCollider*)other.get())->GetSphereData());
    case ColliderType::Box: {
        auto m = CollisionMath::OBBVsSphere(((BoxCollider*)other.get())->CreateOBB(), self);
        m.normal = DirectX::XMVectorNegate(m.normal); // OBB->Sphere を Sphere->OBB に反転
        return m;
    }
    case ColliderType::Capsule: {
        auto m = CollisionMath::CapsuleVsSphere(((CapsuleCollider*)other.get())->GetCapsuleData(), self);
        m.normal = DirectX::XMVectorNegate(m.normal);
        return m;
    }
    }
    return {};
}

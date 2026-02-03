#include "BoxCollider.h"
#include "SphereCollider.h"
#include "CapsuleCollider.h"
#include "CollisionMath.h"
#include "GraphicsDeviceManager.h"

void CapsuleCollider::Gui() {
    ColliderComponent::Gui();

    ImGui::DragFloat("Radius", &radius, 0.01f, 0.0f, 10.0f);
    ImGui::DragFloat("Half Height", &halfHeight, 0.01f, 0.0f, 10.0f);
}

void CapsuleCollider::Update(float dt) {
    auto& renderer = GraphicsDeviceManager::Instance().GetShapeRendererSystem();
    auto t = GetGameObject()->transform;

    using namespace DirectX;
    // 一度変数に受ける
    DirectX::XMFLOAT4X4 worldFloat = t->GetWorld();
    DirectX::XMMATRIX world = DirectX::XMLoadFloat4x4(&worldFloat);

    // オフセット行列の適用
    XMMATRIX offMat = XMMatrixTranslation(offset.x, offset.y, offset.z);
    XMMATRIX finalMat = offMat * world;

    XMFLOAT4X4 drawWorld;
    XMStoreFloat4x4(&drawWorld, finalMat);

    float scale = t->GetWorldScale().x;
    float r = radius * scale;
    float h = (halfHeight * 2.0f) * t->GetWorldScale().y;

    if(shapeRenderFlg)
    {
        renderer.DrawCapsule(drawWorld, r, h, isTrigger ? DirectX::XMFLOAT4(0, 1, 1, 1) : DirectX::XMFLOAT4(1, 0, 1, 1));
    }
}

CollisionManifold CapsuleCollider::CheckCollision(std::shared_ptr<ColliderComponent> other)
{
    CollisionManifold m = {};
    m.isHit = false;

    Capsule self = GetCapsuleData();
    switch (other->GetColliderType()) {
    case ColliderType::Sphere:
        return CollisionMath::CapsuleVsSphere(self, ((SphereCollider*)other.get())->GetSphereData());
    case ColliderType::Box: {
        m = CollisionMath::OBBVsCapsule(((BoxCollider*)other.get())->CreateOBB(), self);
        m.normal = DirectX::XMVectorNegate(m.normal);
        return m;
    }
    case ColliderType::Capsule:
        return CollisionMath::CapsuleVsCapsule(self, ((CapsuleCollider*)other.get())->GetCapsuleData());
    }
    return m;
}

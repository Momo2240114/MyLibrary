#include "BoxCollider.h"
#include "SphereCollider.h"
#include "CapsuleCollider.h"
#include "CollisionMath.h"
#include "GraphicsDeviceManager.h"
#include"GameObjectManager.h"

void BoxCollider::Start()
{
    //ゲームオブジェクトを追加する
    GameObjectManager::Instance().GetRayCastManager()->AddGameObject(GetGameObject());
}

void BoxCollider::Gui() {
    ColliderComponent::Gui();
    ImGui::Checkbox("UsedNaviMeshSystemFlg", &usedNaviMeshSystemFlg);
    ImGui::DragFloat3("halfSize", &halfSize.x, 0.01f);
}

// BoxCollider::Update の例
void BoxCollider::Update(float dt) {

    auto& renderer = GraphicsDeviceManager::Instance().GetShapeRendererSystem();
    auto t = GetGameObject()->transform;

    using namespace DirectX;
    // 一度変数に受ける
    DirectX::XMFLOAT4X4 worldFloat = t->GetWorld();
    DirectX::XMMATRIX world = DirectX::XMLoadFloat4x4(&worldFloat);

    // offset をワールド座標系に変換して加算
    DirectX::XMVECTOR worldOffset = DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&offset), world);
    DirectX::XMVECTOR finalPosVec = DirectX::XMVectorAdd(world.r[3], worldOffset);

    DirectX::XMFLOAT3 pos;
    DirectX::XMStoreFloat3(&pos, finalPosVec);
    // 現在「度」で返ってくる回転を取得
    DirectX::XMFLOAT3 rotDeg = t->GetWorldRotation();

    // 描画用にラジアンへ変換
    DirectX::XMFLOAT3 rotRad = {
        DirectX::XMConvertToRadians(rotDeg.x),
        DirectX::XMConvertToRadians(rotDeg.y),
        DirectX::XMConvertToRadians(rotDeg.z)
    };
    DirectX::XMFLOAT3 scale = t->GetWorldScale();

    DirectX::XMFLOAT3 drawSize = {
        halfSize.x * 2.0f * scale.x,
        halfSize.y * 2.0f * scale.y,
        halfSize.z * 2.0f * scale.z
    };
    if(shapeRenderFlg)
    {
        renderer.DrawBox(pos, rotRad, drawSize,m_DebugPrimitiveColor);
    }
}

CollisionManifold BoxCollider::CheckCollision(std::shared_ptr<ColliderComponent> other)
{
    OBB self = CreateOBB();
    m_Obb = self;
    switch (other->GetColliderType()) {
    case ColliderType::Sphere:
        return CollisionMath::OBBVsSphere(self, ((SphereCollider*)other.get())->GetSphereData());
    case ColliderType::Box:
        return CollisionMath::OBBVsOBB(self, ((BoxCollider*)other.get())->CreateOBB());
    case ColliderType::Capsule:
        return CollisionMath::OBBVsCapsule(self, ((CapsuleCollider*)other.get())->GetCapsuleData());
    }
    return {};
}
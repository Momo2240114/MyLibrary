#include "PhysicsCollisionComponent.h"
#include "PhysXSystem.h"
#include "PhysXMath.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"

using namespace physx;

//--- 
//@brief 衝突形状をボックスとして設定する 
//@param half ボックスの半サイズ（幅，高さ，奥行） 
//--- 
void PhysicsCollisionComponent::SetBox(const DirectX::XMFLOAT3& half) 
{ shapeType = ShapeType::Box; boxHalfSize = half; } 
//---
// @brief 衝突形状を球として設定する 
// @param r 球の半径 
//--- 
void PhysicsCollisionComponent::SetSphere(float r)
{ 
    shapeType = ShapeType::Sphere; sphereRadius = r;
} 
//--- 
// @brief 衝突形状をカプセルとして設定する 
// @param r カプセルの半径
// @param h カプセルの高さ（中心軸方向の長さ） 
//--- 

void PhysicsCollisionComponent::SetCapsule(float r, float h) 
{
    shapeType = ShapeType::Capsule; 
    capsuleRadius = r;
    capsuleHalfHeight = h; 
}



PhysicsCollisionComponent::~PhysicsCollisionComponent()
{
    if (actor)
    {
        if (actor->isReleasable())
        {
            auto* scene = actor->getScene();
            if (scene) scene->removeActor(*actor);
            actor->release();
        }
        actor = nullptr;
    }
}

void PhysicsCollisionComponent::CreateActor()
{
    if (!PhysXSystem::GetPhysics() || !PhysXSystem::GetScene())
    {
        pendingCreate = true;
        return;
    }
    pendingCreate = false;

    auto t = GetGameObject()->transform;

    // --- 【修正】初期位置にオフセットを適用 ---
    // Actorの座標 = Transformの座標 + collisionOffset
    PxTransform pose(
        PxVec3(t->localPosition.x + collisionOffset.x,
            t->localPosition.y + collisionOffset.y,
            t->localPosition.z + collisionOffset.z),
        EulerToPxQuat(t->localRotation)
    );

    actor = PhysXSystem::GetPhysics()->createRigidDynamic(pose);
    actor->userData = this;

    PxMaterial* mat = PhysXSystem::GetDefaultMaterial();
    PxShape* shape = nullptr;

    auto scale = t->GetWorldScale();

    switch (shapeType)
    {
    case ShapeType::Box:
        shape = PxRigidActorExt::createExclusiveShape(
            *actor,
            PxBoxGeometry(boxHalfSize.x * scale.x,
                boxHalfSize.y * scale.y,
                boxHalfSize.z * scale.z),
            *mat);
        break;

    case ShapeType::Sphere:
    {
        float r = sphereRadius * ((scale.x + scale.y + scale.z) / 3.f);
        shape = PxRigidActorExt::createExclusiveShape(*actor, PxSphereGeometry(r), *mat);
        break;
    }

    case ShapeType::Capsule:
    {
        // PhysXのカプセルは初期状態でX軸を向いているため、直立させるには回転が必要な場合があります
        // ここではジオメトリの作成のみ
        float r = capsuleRadius * ((scale.x + scale.y + scale.z) / 3.f);
        float h = capsuleHalfHeight * scale.y;
        shape = PxRigidActorExt::createExclusiveShape(*actor, PxCapsuleGeometry(r, h), *mat);

        // カプセルを縦（Y軸上）に向けるためのローカル回転設定
        if (shape) {
            PxTransform relativePose(PxQuat(PxHalfPi, PxVec3(0, 0, 1))); // 90度回転して縦にする
            shape->setLocalPose(relativePose);
        }
        break;
    }
    default:
        return;
    }

    if (shape)
    {
        shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, !isTrigger);
        shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, isTrigger);
    }

    // --- 設定の反映 ---
    actor->setActorFlag(PxActorFlag::eDISABLE_GRAVITY, !useGravity);
    actor->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, isKinematic);

    // 【追加】角度固定の適用
    ApplyLockRotation();

    PhysXSystem::GetScene()->addActor(*actor);
}

void PhysicsCollisionComponent::Gui()
{

    // --- 基本設定 (変更なし) ---
    if (ImGui::Checkbox("Is Kinematic", &isKinematic))
    {
        if (actor) actor->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, isKinematic);
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("Use Gravity", &useGravity))
    {
        if (actor) actor->setActorFlag(physx::PxActorFlag::eDISABLE_GRAVITY, !useGravity);
    }

    if (ImGui::Checkbox("Is Trigger", &isTrigger))
    {
        if (actor) {
            physx::PxShape* shapes[1];
            actor->getShapes(shapes, 1);
            shapes[0]->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, !isTrigger);
            shapes[0]->setFlag(physx::PxShapeFlag::eTRIGGER_SHAPE, isTrigger);
        }
    }

    ImGui::Separator();

    // --- オフセットと回転固定 ---
    ImGui::DragFloat3("Offset", &collisionOffset.x, 0.05f);

    ImGui::Text("Lock Rotation:");
    bool changedLock = false;
    changedLock |= ImGui::Checkbox("X", &lockRotationX); ImGui::SameLine();
    changedLock |= ImGui::Checkbox("Y", &lockRotationY); ImGui::SameLine();
    changedLock |= ImGui::Checkbox("Z", &lockRotationZ);

    if (changedLock) ApplyLockRotation();

    ImGui::Separator();

    // --- 形状ごとのパラメータ（変更時にGeometryを更新する） ---
    bool sizeChanged = false;
    auto scale = GetGameObject()->transform->GetWorldScale();

    if (shapeType == ShapeType::Box)
    {
        sizeChanged |= ImGui::DragFloat3("Box Half Size", &boxHalfSize.x, 0.05f, 0.01f, 10.0f);
        if (sizeChanged && actor) {
            physx::PxShape* shapes[1];
            actor->getShapes(shapes, 1);
            shapes[0]->setGeometry(physx::PxBoxGeometry(boxHalfSize.x * scale.x, boxHalfSize.y * scale.y, boxHalfSize.z * scale.z));
        }
    }
    else if (shapeType == ShapeType::Sphere)
    {
        sizeChanged |= ImGui::DragFloat("Sphere Radius", &sphereRadius, 0.05f, 0.01f, 10.0f);
        if (sizeChanged && actor) {
            physx::PxShape* shapes[1];
            actor->getShapes(shapes, 1);
            float r = sphereRadius * ((scale.x + scale.y + scale.z) / 3.f);
            shapes[0]->setGeometry(physx::PxSphereGeometry(r));
        }
    }
    else if (shapeType == ShapeType::Capsule)
    {
        sizeChanged |= ImGui::DragFloat("Capsule Radius", &capsuleRadius, 0.05f, 0.01f, 10.0f);
        sizeChanged |= ImGui::DragFloat("Capsule Half Height", &capsuleHalfHeight, 0.05f, 0.01f, 10.0f);
        if (sizeChanged && actor) {
            physx::PxShape* shapes[1];
            actor->getShapes(shapes, 1);
            float r = capsuleRadius * ((scale.x + scale.y + scale.z) / 3.f);
            float h = capsuleHalfHeight * scale.y;
            shapes[0]->setGeometry(physx::PxCapsuleGeometry(r, h));
        }
    }

    // 基本は自動更新されるが、念のための再生成ボタン
    if (ImGui::Button("Force Recreate Actor"))
    {
        if (actor) {
            actor->release();
            actor = nullptr;
        }
        CreateActor();
    }
}

void PhysicsCollisionComponent::Update(float dt)
{
    auto& t = GetGameObject()->transform;
    if (!actor) return;

    if (isKinematic)
    {
        // --- Kinematic: Transform -> PhysX ---
        // GameObjectのワールド位置に、回転を考慮したオフセットを加える
        DirectX::XMVECTOR basePos = DirectX::XMLoadFloat3(&t->localPosition);
        DirectX::XMVECTOR off = DirectX::XMLoadFloat3(&collisionOffset);

        // オフセットにも回転を適用したい場合はここで行いますが、
        // 通常は単純な加算（軸固定オフセット）で十分なことが多いです
        DirectX::XMVECTOR targetPos = DirectX::XMVectorAdd(basePos, off);

        PxTransform target(
            PxVec3(targetPos.m128_f32[0], targetPos.m128_f32[1], targetPos.m128_f32[2]),
            EulerToPxQuat(t->localRotation)
        );
        actor->setKinematicTarget(target);
    }
    else
    {
        // --- Dynamicモード ---

        // 1. 他のコンポーネントが与えた回転や移動を物理アクターに反映
        PxTransform pose = actor->getGlobalPose();

        // 【回転の同期】
        // 他のコンポーネントが決定した Transform の回転を物理にセット。
        // これにより「他のコンポーネントの回転が無効化される」のを防ぎます。
        pose.q = EulerToPxQuat(t->localRotation);

        // 【手動移動の反映】
        if (t->moveOffset.x != 0 || t->moveOffset.y != 0 || t->moveOffset.z != 0)
        {
            pose.p.x += t->moveOffset.x;
            pose.p.y += t->moveOffset.y;
            pose.p.z += t->moveOffset.z;
            t->moveOffset = { 0, 0, 0 };
        }

        // 物理アクターの姿勢を更新
        actor->setGlobalPose(pose);

        // 2. 物理シミュレーションの結果を Transform に戻す
        // シミュレーション（衝突応答）後の最新の座標を取得
        PxTransform currentPose = actor->getGlobalPose();

        // 【位置】は物理（衝突による押し戻し）を反映
        t->localPosition.x = currentPose.p.x - collisionOffset.x;
        t->localPosition.y = currentPose.p.y - collisionOffset.y;
        t->localPosition.z = currentPose.p.z - collisionOffset.z;

        // 【回転】ロック機能のロジック
        // 物理演算の結果 (currentPose.q) は一切 Transform に戻しません。
        // 代わりに、ロックが有効な軸については、Update開始時の回転を「初期状態」として維持します。
        // ロックされていない軸のみ、物理の計算結果を反映させたい場合はここで行いますが、
        // 「他のコンポーネントの回転を優先したい」場合は、基本このままでOKです。

        // もし「物理で回転させたい軸」がある場合のみ、以下のように書きます
        if (!lockRotationX || !lockRotationY || !lockRotationZ)
        {
            //DirectX::XMFLOAT3 physRot = QuaternionToEuler(currentPose.q);
            //if (!lockRotationX) t->localRotation.x = DirectX::XMConvertToDegrees(physRot.x);
            //if (!lockRotationY) t->localRotation.y = DirectX::XMConvertToDegrees(physRot.y);
            //if (!lockRotationZ) t->localRotation.z = DirectX::XMConvertToDegrees(physRot.z);
        }

        t->MarkDirty();
    }

    // ★ デバッグ用コリジョン描画
    auto& renderer = GraphicsDeviceManager::Instance().GetShapeRendererSystem();

    // 1. GameObjectのベースとなる情報
    DirectX::XMFLOAT3 basePos = t->GetWorldPosition();
    DirectX::XMFLOAT3 angle = t->GetWorldRotation();
    DirectX::XMFLOAT3 scale = t->GetWorldScale();

    // 2. オフセットを適用した「描画位置」を計算
    DirectX::XMFLOAT3 drawPos = {
        basePos.x + collisionOffset.x,
        basePos.y + collisionOffset.y,
        basePos.z + collisionOffset.z
    };

    DirectX::XMFLOAT4 colorRed = { 1,0,0,1 };
    DirectX::XMFLOAT4 colorGreen = { 0,1,0,1 };
    DirectX::XMFLOAT4 colorP = { 1,0,1,1 };

    switch (shapeType)
    {
    case ShapeType::Box:
    {
        DirectX::XMFLOAT3 size = { boxHalfSize.x * 2.f * scale.x,
                                   boxHalfSize.y * 2.f * scale.y,
                                   boxHalfSize.z * 2.f * scale.z };
        // drawPos を使用
        renderer.DrawBox(drawPos, angle, size, colorRed);
        break;
    }
    case ShapeType::Sphere:
    {
        float r = sphereRadius * ((scale.x + scale.y + scale.z) / 3.f);
        // drawPos を使用
        renderer.DrawSphere(drawPos, r, colorGreen);
        break;
    }
    case ShapeType::Capsule:
    {
        // カプセルの場合、GetWorld()に行列としてオフセットを乗算する
        using namespace DirectX;
        // 戻り値を変数にコピーして実体化させる
        DirectX::XMFLOAT4X4 world4x4 = t->GetWorld();
        // 変数なのでアドレス (&) が取れるようになる
        DirectX::XMMATRIX worldMat = DirectX::XMLoadFloat4x4(&world4x4);

        DirectX::XMMATRIX offsetMat = DirectX::XMMatrixTranslation(collisionOffset.x, collisionOffset.y, collisionOffset.z);
        DirectX::XMMATRIX finalDrawMat = offsetMat * worldMat;

        XMFLOAT4X4 drawWorld;
        XMStoreFloat4x4(&drawWorld, finalDrawMat);

        float r = capsuleRadius * ((scale.x + scale.y + scale.z) / 3.f);
        float h = capsuleHalfHeight * 2.f * scale.y;

        // 合成した行列を使用して描画
        renderer.DrawCapsule(drawWorld, r, h, colorP);
        break;
    }
    }
}


void PhysicsCollisionComponent::ApplyLockRotation()
{
    if (!actor) return;

    PxRigidDynamicLockFlags flags;
    if (lockRotationX) flags |= PxRigidDynamicLockFlag::eLOCK_ANGULAR_X;
    if (lockRotationY) flags |= PxRigidDynamicLockFlag::eLOCK_ANGULAR_Y;
    if (lockRotationZ) flags |= PxRigidDynamicLockFlag::eLOCK_ANGULAR_Z;

    actor->setRigidDynamicLockFlags(flags);
}


// イベント転送
void PhysicsCollisionComponent::OnCollisionEnter(std::shared_ptr<GameObject> other)
{
    if (auto obj = GetGameObject())
    {
        auto col = obj->GetComponent<ColliderComponent>();
        if (col) col->OnCollisionEnter(other);
    }
}

void PhysicsCollisionComponent::OnTriggerEnter(std::shared_ptr<GameObject> other) { /* 同上 */ }
void PhysicsCollisionComponent::OnTriggerStay(std::shared_ptr<GameObject> other) { /* 同上 */ }
void PhysicsCollisionComponent::OnTriggerExit(std::shared_ptr<GameObject> other) { /* 同上 */ }

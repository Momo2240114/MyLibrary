#pragma once

#include "ColliderComponent.h"

// GltfModelのMeshからPhysX用Mesh Colliderを作るComponent。
// Triangle Meshは静的Actor専用、動かしたいObjectはConvex Meshとして使う。
class MeshCollider : public ColliderComponent
{
public:
    bool convex = false; // trueならConvex Mesh、falseなら静的Triangle Meshとして作る。

    MeshCollider()
    {
        // Triangle MeshはPhysX上でDynamicにできないため、標準はStatic Colliderとして扱う。
        // Model Load完了を待つ必要があるので、Scene開始前の必須準備対象にする。
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
        // Collider共通設定に加えて、Convex/Triangle Meshの選択を保存する。
        ColliderComponent::Serialize(j_);
        j_["Convex"] = convex;
    }

    void Deserialize(const nlohmann::json& j_) override
    {
        // Load直後はModelがまだ非同期Load中の可能性があるため、waitingForModelを立てる。
        // 非Convexの場合はPhysX制約に合わせ、Static Colliderへ戻す。
        ColliderComponent::Deserialize(j_);
        convex = j_.value("Convex", false);
        if (!convex) isStatic = true;
        waitingForModel = true;
    }

private:
    // GltfModelが読み込み済みで、Mesh情報からColliderを作れるか確認する。
    bool CanBuildMesh() const;
    // PhysX ActorにShapeが作られていれば、Runtime判定可能とみなす。
    bool IsCollisionBuilt() const;
    bool waitingForModel = true;
};

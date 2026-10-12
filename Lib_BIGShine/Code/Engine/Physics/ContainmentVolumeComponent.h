#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"

namespace physx
{
    class PxRigidStatic;
}

enum class CONTAINMENT_SHAPE
{
    // 円柱状の閉じ込めVolume。
    CYLINDER,
    // 表示上はCapsuleだが、現在のPhysics壁は円柱Meshで作る。
    CAPSULE
};

// PlayerやCameraなどを範囲内へ留めるための閉じ込めVolume。
// 現在は対象Objectを手動で押し戻すのではなく、PhysXのStatic Triangle Mesh壁としてSceneへ置く。
class ContainmentVolumeComponent : public Component
{
public:
    ~ContainmentVolumeComponent() override;

    const std::string& GetName() const override
    {
        static std::string name = "ContainmentVolumeComponent";
        return name;
    }

    void Start() override;
    void OnDestroy() override { ReleasePhysicsVolume(); }
    void Update(float deltaTime_) override;
    void ApplyContainment();
    // Sceneへ保存する半径を保ち、演出中だけ物理壁の半径を差し替える。
    void SetRuntimeRadius(float radius_) { runtimeRadius = radius_ > 0.0f ? radius_ : 0.001f; }
    void ClearRuntimeRadius() { runtimeRadius = -1.0f; }
    float GetEffectiveRadius() const { return runtimeRadius >= 0.0f ? runtimeRadius : radius; }
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    CONTAINMENT_SHAPE shape = CONTAINMENT_SHAPE::CYLINDER; // Debug表示上の形状種別。
    uint32_t targetMask = static_cast<uint32_t>(LayerType::Player); // 旧手動補正用の対象Mask。
    float radius = 5.0f; // Volume半径。
    float halfHeight = 2.0f; // Volume半分高さ。
    float targetRadius = 0.35f; // 旧手動補正用の対象半径。
    bool constrainHeight = true; // 上下面を閉じるか。falseなら筒状の壁だけ作る。
    int segments = 64; // 円柱Meshの分割数。
    bool showDebugShape = true; // Debug表示するか。
    DirectX::XMFLOAT4 debugShapeColor = { 0.0f, 0.75f, 1.0f, 1.0f };

private:
    // 現在設定からPhysXのStatic Mesh壁を作り直す。
    void RebuildPhysicsVolume();
    // PhysX SceneからActorを外して解放する。
    void ReleasePhysicsVolume();
    // GUI/Deserializeで設定が変わったことを記録し、次回ApplyContainmentで再構築する。
    void MarkPhysicsDirty();

    physx::PxRigidStatic* actor = nullptr;
    bool physicsDirty = true;
    float runtimeRadius = -1.0f;
    // 再構築が必要かを判定するため、最後にPhysXへ反映した値をCacheする。
    CONTAINMENT_SHAPE cachedShape = CONTAINMENT_SHAPE::CYLINDER;
    float cachedRadius = -1.0f;
    float appliedRadius = -1.0f; // MeshをCookした半径と、現在のShape倍率を分けて保持する。
    float cachedHalfHeight = -1.0f;
    bool cachedConstrainHeight = false;
    int cachedSegments = -1;
    DirectX::XMFLOAT3 cachedScale = { 0.0f, 0.0f, 0.0f };
};

#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class CameraComponent;
class CameraLockOnComponent;
class GameObject;

class CameraCollisionComponent : public Component
{
public:
    const std::string& GetName() const override
    {
        static std::string name_ = "CameraCollisionComponent";
        return name_;
    }

    void Start() override;
    void LateUpdate(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    // CutSceneや特殊Camera中に衝突補正を一時停止する。
    void SetCollisionSuppressed(bool suppressed_);
    bool IsCollisionSuppressed() const { return collisionSuppressed; }

private:
    // Camera/LockOn/Ignore対象を解決する。
    void ResolveReferences();
    // focusからdesired camera positionまでをSphere/RayでSweepし、最寄りHit距離を返す。
    bool SweepCameraSegment(
        const DirectX::XMFLOAT3& startPosition_,
        const DirectX::XMFLOAT3& endPosition_,
        const std::vector<const void*>& ignoredUserData_,
        float& hitDistance_) const;
    // 補正後のCamera位置をCameraComponentとTransformへ反映する。
    void ApplyCameraPose(
        const DirectX::XMFLOAT3& cameraPosition_,
        const DirectX::XMFLOAT3& lookAtPosition_);
    // Camera Collisionで標準的に見るLayer Maskを返す。
    static uint32_t GetDefaultCollisionLayerMask();
    // Layer Mask編集用のGuiを描画する。
    void DrawCollisionLayerMaskGui();

private:
    // 衝突補正対象のCameraと、LockOn構図補正との連携先。
    std::weak_ptr<CameraComponent> camera;
    std::weak_ptr<CameraLockOnComponent> lockOnCamera;
    std::weak_ptr<GameObject> ignoreTarget;
    std::weak_ptr<GameObject> ignoreLockOnTarget;
    // Scene読込後にIgnore対象を名前で再解決するための名前。
    std::string ignoreTargetName;
    std::string ignoreLockOnTargetName;

    // Hit位置から少し手前にCameraを置くための余白。
    float collisionOffset = 0.15f;
    // Sweep対象Layer。
    uint32_t cameraCollisionLayerMask = GetDefaultCollisionLayerMask();
    // Focusに近づきすぎないための最短距離。
    float minimumCameraDistance = 0.05f;
    // Sphere Sweep半径。0に近い場合もRaycastで補助する。
    float probeRadius = 0.3f;
    bool includeDynamicColliders = true;
    bool collisionSuppressed = false;
    // LockOn中、壁で圧縮されたときにCameraを上方向へ逃がす補正。
    bool enableLockOnCollisionLift = true;
    bool reduceCollisionLiftWhenArenaWallCameraActive = true;
    float maxCollisionLiftHeight = 3.0f;
    float collisionLiftStartRate = 0.25f;
    float collisionLiftUpSpeed = 14.0f;
    float collisionLiftDownSpeed = 5.0f;
    float currentCollisionLift = 0.0f;
    // Editor表示用の直近補正情報。
    float debugDesiredDistance = 0.0f;
    float debugCorrectedDistance = 0.0f;
    float debugCompressionRate = 0.0f;
    mutable std::string debugLastHitObjectName;
    mutable std::string debugLastHitColliderName;
    mutable uint32_t debugLastHitLayer = 0u;
    mutable bool debugLastHitAccepted = false;
};

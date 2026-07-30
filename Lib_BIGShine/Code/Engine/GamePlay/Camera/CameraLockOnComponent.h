#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"

#include <DirectXMath.h>
#include <memory>
#include <string>
#include <vector>

class CameraComponent;
class GameObject;
class ThirdPersonCameraComponent;

struct LockOnCameraCompositionCorrection
{
    float sideOffset = 0.0f;
    float distanceOffset = 0.0f;
    float heightOffset = 0.0f;
    float lookAtHeightOffset = 0.0f;
    float focusTargetRateOffset = 0.0f;
    float fovOffset = 0.0f;
    float weight = 0.0f;
};

class CameraLockOnComponent : public Component
{
public:
    const std::string& GetName() const override
    {
        static std::string name_ = "CameraLockOnComponent";
        return name_;
    }

    void Start() override;
    void LateUpdate(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    void SetLockOnEnabled(bool enabled_);
    void SetActionSuppressed(bool suppressed_);
    bool IsLockOnEnabled() const { return lockOnEnabled; }
    bool IsActionSuppressed() const { return actionSuppressed; }
    bool IsWallCameraActive() const;
    float GetWallCameraRate() const { return currentWallCameraRate; }
    float GetCurrentTargetDistance() const { return currentTargetDistance; }
    DirectX::XMFLOAT3 GetCurrentArenaOutwardDirection() const { return previousArenaOutwardDirection; }
    DirectX::XMFLOAT3 GetCurrentBaseCameraPosition() const { return currentBaseCameraPosition; }
    DirectX::XMFLOAT3 GetCurrentBaseLookAtPosition() const { return currentBaseLookAtPosition; }
    const LockOnCameraCompositionCorrection& GetCompositionCorrection() const { return compositionCorrection; }
    std::shared_ptr<GameObject> GetPlayerTarget() const { return playerTarget.lock(); }
    std::shared_ptr<GameObject> GetLockOnTarget() const { return lockOnTarget.lock(); }
    std::shared_ptr<GameObject> GetLockOnFocusTarget() const { return lockOnFocusTarget.lock(); }
    void SetPlayerTarget(const std::shared_ptr<GameObject>& playerTarget_);
    void SetLockOnTarget(const std::shared_ptr<GameObject>& lockOnTarget_);
    void SetLockOnFocusTarget(const std::shared_ptr<GameObject>& focusTarget_);
    std::shared_ptr<GameObject> FindBestLockOnTarget(
        const std::shared_ptr<GameObject>& playerTarget_) const;
    bool SwitchLockOnTarget(const DirectX::XMFLOAT2& inputDirection_);
    bool SwitchLockOnTargetNext();
    bool SwitchLockOnTargetPrevious();
    bool ToggleLockOnForPlayer(const std::shared_ptr<GameObject>& playerTarget_);
    bool ApplyCollisionComposition(
        const DirectX::XMFLOAT3& cameraPosition_,
        const DirectX::XMFLOAT3& baseLookAtPosition_,
        float compressionRate_,
        float collisionLift_,
        float deltaTime_);
    void SetCompositionCorrection(
        const LockOnCameraCompositionCorrection& correction_);
    void ClearCompositionCorrection();

private:
    void ResolveReferences();
    void RegisterActions();
    void ResetTracking();
    void ReleaseThirdPersonCamera();
    void ApplyCameraPose(
        const DirectX::XMFLOAT3& cameraPosition_,
        const DirectX::XMFLOAT3& lookAtPosition_);
    void SynchronizeExternalFovChange(const std::shared_ptr<CameraComponent>& camera_);
    DirectX::XMFLOAT3 GetFocusPosition(
        const std::shared_ptr<GameObject>& object_,
        const DirectX::XMFLOAT3& fallbackOffset_) const;
    DirectX::XMFLOAT3 GetPlayerBottomPosition(
        const std::shared_ptr<GameObject>& object_) const;
    DirectX::XMFLOAT3 CalculateCollisionLookAt(
        const DirectX::XMFLOAT3& cameraPosition_,
        const DirectX::XMFLOAT3& baseLookAtPosition_,
        const DirectX::XMFLOAT3& playerTopPosition_,
        const DirectX::XMFLOAT3& playerBottomPosition_,
        const DirectX::XMFLOAT3& targetPosition_,
        float compressionRate_,
        float liftRate_) const;
    DirectX::XMFLOAT3 CalculateFramingCameraPosition(
        const DirectX::XMFLOAT3& cameraPosition_,
        const DirectX::XMFLOAT3& lookAtPosition_,
        const DirectX::XMFLOAT3& playerTopPosition_,
        const DirectX::XMFLOAT3& playerBottomPosition_,
        const DirectX::XMFLOAT3& targetPosition_,
        float compressionRate_) const;
    float CalculateCollisionFov(
        const DirectX::XMFLOAT3& cameraPosition_,
        const DirectX::XMFLOAT3& lookAtPosition_,
        const DirectX::XMFLOAT3& playerTopPosition_,
        const DirectX::XMFLOAT3& playerBottomPosition_,
        const DirectX::XMFLOAT3& targetPosition_,
        float compressionRate_) const;
    bool ProjectToScreenPosition(
        const DirectX::XMFLOAT3& cameraPosition_,
        const DirectX::XMFLOAT3& lookAtPosition_,
        const DirectX::XMFLOAT3& worldPosition_,
        float fovY_,
        DirectX::XMFLOAT2& screenPosition_) const;
    float CalculateProjectionScore(
        const DirectX::XMFLOAT2& playerTopScreen_,
        const DirectX::XMFLOAT2& playerBottomScreen_,
        const DirectX::XMFLOAT2& targetScreen_,
        bool playerTopVisible_,
        bool playerBottomVisible_,
        bool targetVisible_) const;
    DirectX::XMFLOAT3 CalculateWeightedLookAt(
        const DirectX::XMFLOAT3& playerPosition_,
        const DirectX::XMFLOAT3& targetPosition_) const;
    DirectX::XMFLOAT3 ClampCameraPositionToArena(
        const DirectX::XMFLOAT3& cameraPosition_) const;
    float CalculateWallCameraCandidateScore(
        const DirectX::XMFLOAT3& candidatePosition_,
        const DirectX::XMFLOAT3& lookAtPosition_,
        const DirectX::XMFLOAT3& playerTopPosition_,
        const DirectX::XMFLOAT3& playerBottomPosition_,
        const DirectX::XMFLOAT3& targetPosition_,
        float fovY_,
        int candidateSide_) const;
    struct LockOnDistanceInfo
    {
        DirectX::XMFLOAT3 playerPosition{};
        DirectX::XMFLOAT3 targetPosition{};
        DirectX::XMFLOAT3 difference{};
        float horizontalDistance = 0.0f;
        float targetDistance = 0.0f;
        bool isValid = false;
    };

    struct LockOnCandidate
    {
        std::weak_ptr<GameObject> focusTarget;
        std::weak_ptr<GameObject> combatOwner;
        DirectX::XMFLOAT3 worldPosition{};
        DirectX::XMFLOAT2 screenPosition{};
        float distance = 0.0f;
        float priority = 1.0f;
        bool isVisible = false;
        bool isDefaultTarget = false;
        bool isCombatOwnerSelf = false;
        float score = 0.0f;
    };

    LockOnDistanceInfo CalculateLockOnDistanceInfo() const;
    std::vector<LockOnCandidate> CollectLockOnCandidates(
        const std::shared_ptr<GameObject>& playerTarget_) const;
    LockOnCandidate FindBestLockOnCandidate(
        const std::shared_ptr<GameObject>& playerTarget_) const;
    std::shared_ptr<GameObject> FindDirectionalLockOnTarget(
        const DirectX::XMFLOAT2& inputDirection_) const;
    bool ApplyLockOnCandidate(const LockOnCandidate& candidate_);
    bool ResolveFocusTargetFallback();
    void UpdateLockOnSwitchInput(float deltaTime_);
    DirectX::XMFLOAT2 GetKeyboardSwitchInput() const;
    DirectX::XMFLOAT2 GetControllerSwitchInput() const;
    DirectX::XMFLOAT2 GetMouseSwitchInput(float deltaTime_);
    void RegisterDefaultSwitchInputBindings();
    void DrawLockOnDebugGui();
private:
    std::weak_ptr<GameObject> playerTarget;
    std::weak_ptr<GameObject> lockOnTarget;
    std::weak_ptr<GameObject> lockOnFocusTarget;
    std::weak_ptr<CameraComponent> camera;
    std::weak_ptr<ThirdPersonCameraComponent> thirdPersonCamera;

    std::string playerTargetName;
    std::string lockOnTargetName;
    std::string lockOnFocusTargetName;
    bool lockOnEnabled = false;
    bool actionSuppressed = false;

    DirectX::XMFLOAT3 playerOffset{ 0.0f, 1.5f, 0.0f };
    DirectX::XMFLOAT3 playerBottomOffset{ 0.0f, 0.05f, 0.0f };
    DirectX::XMFLOAT3 targetOffset{ 0.0f, 1.2f, 0.0f };
    float maxDistance = 28.0f;
    float baseDistance = 7.0f;
    float distanceScale = 0.45f;
    float minCameraDistance = 6.0f;
    float maxCameraDistance = 16.0f;
    float cameraHeight = 2.2f;
    float separationHeightScale = 0.08f;
    float heightDifferenceScale = 0.35f;
    float sideOffset = 1.25f;
    float currentSide = 1.0f;
    float closeDistance = 8.0f;
    float closeFocusBias = 0.28f;
    float farFocusBias = 0.5f;
    float positionInterpolationSpeed = 10.0f;
    float lookAtInterpolationSpeed = 12.5f;
    float horizontalSafeMin = 0.10f;
    float horizontalSafeMax = 0.90f;
    float verticalSafeMin = 0.22f;
    float verticalSafeMax = 0.78f;
    float playerBottomLimit = 0.70f;
    float playerFocusWeight = 0.62f;
    float targetFocusWeight = 0.38f;
    float verticalDistanceCorrectionStrength = 2.5f;
    float verticalFocusCorrectionStrength = 1.4f;
    float framingSmoothSpeed = 16.0f;
    float collisionLookAtLowerHeight = 1.1f;
    float collisionLookAtSearchRadius = 1.8f;
    float collisionLookAtInterpolationSpeed = 18.0f;
    float collisionFovMaxAddition = 18.0f;
    float collisionFovInterpolationSpeed = 10.0f;
    bool enableCircularArenaCamera = false;
    DirectX::XMFLOAT3 arenaCenter{ 0.0f, 0.0f, 0.0f };
    float arenaRadius = 15.0f;
    float arenaCameraMargin = 0.8f;
    float wallCameraStartDistance = 3.0f;
    float wallCameraDistance = 5.5f;
    float wallCameraHeight = 3.0f;
    float wallCameraSideOffset = 1.5f;
    float wallCenterWeight = 0.75f;
    float wallCombatWeight = 0.25f;
    float wallCameraPositionSmoothSpeed = 8.0f;
    float wallCameraRateSmoothSpeed = 7.0f;
    float wallCameraMaximumHeightAddition = 1.2f;
    float wallCameraMaximumFovAddition = 12.0f;
    float wallLookAtPlayerBias = 0.4f;
    float wallCameraSideMinimumHoldTime = 0.5f;
    float wallCameraSideSwitchScoreThreshold = 0.35f;
    float wallCameraOppositeSideMinimumHoldTime = 0.85f;
    float wallCameraOppositeSideSwitchScoreThreshold = 0.75f;
    float wallCameraContinuityScoreWeight = 0.12f;
    float switchStickThreshold = 0.65f;
    float switchStickReleaseThreshold = 0.25f;
    float switchCooldown = 0.18f;
    float mouseSwitchThreshold = 35.0f;
    float mouseAccumulationResetTime = 0.12f;
    float directionalDotThreshold = 0.35f;
    float screenDistanceWeight = 0.55f;
    float centerDistanceWeight = 0.35f;
    float priorityWeight = 0.25f;

    DirectX::XMFLOAT3 interpolatedPosition{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 interpolatedLookAt{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 collisionLookAt{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 collisionCameraPosition{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 previousWallCameraPosition{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 previousArenaOutwardDirection{ 0.0f, 0.0f, 1.0f };
    DirectX::XMFLOAT3 currentBaseCameraPosition{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 currentBaseLookAtPosition{ 0.0f, 0.0f, 0.0f };
    std::weak_ptr<GameObject> previousLockOnTarget;
    std::weak_ptr<GameObject> previousFocusTarget;
    LockOnCameraCompositionCorrection compositionCorrection;
    float baseFovY = 60.0f;
    float currentCollisionFov = 60.0f;
    float currentTargetDistance = 0.0f;
    DirectX::XMFLOAT2 debugPlayerScreen{ 0.5f, 0.5f };
    DirectX::XMFLOAT2 debugPlayerBottomScreen{ 0.5f, 0.5f };
    DirectX::XMFLOAT2 debugTargetScreen{ 0.5f, 0.5f };
    DirectX::XMFLOAT3 debugNormalCameraPosition{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 debugSelectedWallCameraPosition{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 debugFinalDesiredCameraPosition{ 0.0f, 0.0f, 0.0f };
    float debugDistanceToWall = 0.0f;
    float debugRawWallCameraRate = 0.0f;
    float debugCameraArenaRadius = 0.0f;
    float debugMaximumCameraArenaRadius = 0.0f;
    float debugSelectedCandidateScore = 0.0f;
    float debugLeftCandidateScore = 0.0f;
    float debugCenterCandidateScore = 0.0f;
    float debugRightCandidateScore = 0.0f;
    mutable int debugCandidateCount = 0;
    mutable float debugLastSwitchScore = 0.0f;
    DirectX::XMFLOAT2 debugLastSwitchDirection{ 0.0f, 0.0f };
    DirectX::XMFLOAT2 debugControllerSwitchInput{ 0.0f, 0.0f };
    DirectX::XMFLOAT2 debugMouseAccumulatedDelta{ 0.0f, 0.0f };
    mutable DirectX::XMFLOAT2 debugCurrentFocusScreen{ 0.5f, 0.5f };
    int currentWallCameraSide = 0;
    float currentWallCameraRate = 0.0f;
    float wallCameraSideHoldTimer = 0.0f;
    bool hasWallCameraPosition = false;
    bool hasArenaOutwardDirection = false;
    bool debugHorizontalCorrection = false;
    bool debugVerticalCorrection = false;
    bool debugPlayerBottomCorrection = false;
    bool debugFovCorrection = false;
    bool hasTrackingPose = false;
    bool hasBaseFov = false;
    bool hasCollisionComposition = false;
    bool ownsThirdPersonControl = false;
    bool switchInputLatched = false;
    float switchCooldownTimer = 0.0f;
    float mouseAccumulationTimer = 0.0f;
    DirectX::XMFLOAT2 accumulatedMouseDelta{ 0.0f, 0.0f };
};

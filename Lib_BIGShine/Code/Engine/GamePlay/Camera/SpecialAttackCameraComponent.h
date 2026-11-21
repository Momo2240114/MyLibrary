#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"

#include <DirectXMath.h>
#include <memory>

class CameraComponent;
class GameObject;

class SpecialAttackCameraComponent : public Component
{
public:
    SpecialAttackCameraComponent();
    const std::string& GetName() const override
    {
        static const std::string NAME = "SpecialAttackCameraComponent";
        return NAME;
    }

    void LateUpdate(float deltaTime_) override;
    void OnDestroy() override { EndTracking(); }
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    bool BeginTracking(const std::shared_ptr<GameObject>& player_, const std::shared_ptr<GameObject>& attacker_);
    void EndTracking();
    bool IsTracking() const;
    bool HasTrackingPose() const { return hasTrackingPose && IsTracking(); }
    std::shared_ptr<GameObject> GetPlayerTarget() const { return playerTarget.lock(); }
    std::shared_ptr<GameObject> GetAttackTarget() const { return attackTarget.lock(); }
    bool IsWallCameraActive() const { return enableCircularArenaCamera && currentWallCameraRate > 0.05f; }
    float GetWallCameraRate() const { return currentWallCameraRate; }
    float GetCurrentTargetDistance() const { return currentTargetDistance; }
    DirectX::XMFLOAT3 GetCurrentBaseCameraPosition() const { return currentBaseCameraPosition; }
    DirectX::XMFLOAT3 GetCurrentBaseLookAtPosition() const { return currentBaseLookAtPosition; }

    // バリア遷移と同じ中心・半径を壁際カメラの制限にも反映する。
    void SetArenaBounds(const DirectX::XMFLOAT3& center_, float radius_)
    {
        if (!hasRuntimeArenaBounds)
        {
            originalArenaCenter = arenaCenter;
            originalArenaRadius = arenaRadius;
            hasRuntimeArenaBounds = true;
        }
        arenaCenter = center_;
        arenaRadius = radius_;
    }

    // 単体再生の終了時に、演出前の壁際カメラ設定へ戻す。
    void ClearArenaBounds()
    {
        if (!hasRuntimeArenaBounds)
        {
            return;
        }
        arenaCenter = originalArenaCenter;
        arenaRadius = originalArenaRadius;
        hasRuntimeArenaBounds = false;
    }

    // CameraCollisionComponentから圧縮率を受け取り、壁際用の構図へ補正する。
    bool ApplyCollisionComposition(
        const DirectX::XMFLOAT3& cameraPosition_,
        const DirectX::XMFLOAT3& baseLookAtPosition_,
        float compressionRate_,
        float collisionLift_,
        float deltaTime_);

private:
    void ResetTracking();
    void ApplyCameraPose(
        const DirectX::XMFLOAT3& cameraPosition_,
        const DirectX::XMFLOAT3& lookAtPosition_);

    // 外部のFovDirectorなどがFOVを変えた時、baseFovYを同期する。
    void SynchronizeExternalFovChange(const std::shared_ptr<CameraComponent>& camera_);
    DirectX::XMFLOAT3 GetFocusPosition(
        const std::shared_ptr<GameObject>& object_,
        const DirectX::XMFLOAT3& fallbackOffset_) const;
    DirectX::XMFLOAT3 GetPlayerBottomPosition(
        const std::shared_ptr<GameObject>& object_) const;

    // 壁接触時でもPlayer/Targetが画面内に残るよう、注視点を下げたり寄せたりする。
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

    // 画面内収まりを評価しながら、必要なFOV追加量を求める。
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

    // Player/Targetの画面上の見切れ具合をスコア化する。
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

    // 円形Arena設定が有効な時、カメラ位置をArena外へ出しすぎない。
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

    std::weak_ptr<GameObject> playerTarget;
    std::weak_ptr<GameObject> attackTarget;
    std::weak_ptr<CameraComponent> camera;

    // Player/Targetの注視点補正。
    DirectX::XMFLOAT3 playerOffset{ 0.0f, 1.5f, 0.0f };
    DirectX::XMFLOAT3 playerBottomOffset{ 0.0f, 0.05f, 0.0f };
    DirectX::XMFLOAT3 targetOffset{ 0.0f, 1.2f, 0.0f };

    // PlayerとTargetの距離から決める基本カメラ距離。
    float baseDistance = 7.0f;
    float distanceScale = 0.45f;
    float minCameraDistance = 6.0f;
    float maxCameraDistance = 16.0f;
    float cameraHeight = 2.2f;
    float separationHeightScale = 0.08f;
    float heightDifferenceScale = 0.35f;
    float sideOffset = 1.25f;
    float currentSide = 1.0f;
    // 接近時の急旋回と、構図補正による過度な寄りを抑える。
    bool enableCloseRangeDistanceGuard = false;
    float closeRangeDirectionHoldDistance = 1.5f;
    float closeRangeOrbitTurnSpeed = 90.0f;
    float closeRangeMinimumHeight = 1.2f;
    DirectX::XMFLOAT3 closeRangeBackDirection{ 0.0f, 0.0f, -1.0f };
    bool hasCloseRangeBackDirection = false;

    // 近距離/遠距離で注視点のPlayer寄り/Target寄りを切り替える。
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
    bool enableCollisionVerticalCorrection = true; // ドラゴン用の足元・縦構図補正を使用する。
    float collisionLookAtSearchRadius = 1.8f;
    float collisionLookAtInterpolationSpeed = 18.0f;
    float collisionFovMaxAddition = 18.0f;
    float collisionFovInterpolationSpeed = 10.0f;

    // 円形Arenaの外周付近で、壁際カメラへ寄せるための設定。
    bool enableCircularArenaCamera = false;
    DirectX::XMFLOAT3 arenaCenter{ 0.0f, 0.0f, 0.0f };
    float arenaRadius = 15.0f;
    bool hasRuntimeArenaBounds = false;
    DirectX::XMFLOAT3 originalArenaCenter{ 0.0f, 0.0f, 0.0f };
    float originalArenaRadius = 15.0f;
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

    // カメラ位置と注視点の補間状態。
    DirectX::XMFLOAT3 interpolatedPosition{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 interpolatedLookAt{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 collisionLookAt{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 collisionCameraPosition{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 previousWallCameraPosition{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 previousArenaOutwardDirection{ 0.0f, 0.0f, 1.0f };
    DirectX::XMFLOAT3 currentBaseCameraPosition{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 currentBaseLookAtPosition{ 0.0f, 0.0f, 0.0f };

    float baseFovY = 60.0f;
    float currentCollisionFov = 60.0f;
    float currentTargetDistance = 0.0f;

    // Debug GUIで表示する画面投影・候補スコア。
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

    // FOVやCollisionCompositionなど、外部から渡された状態の有無。
    bool hasBaseFov = false;
    bool hasCollisionComposition = false;
};

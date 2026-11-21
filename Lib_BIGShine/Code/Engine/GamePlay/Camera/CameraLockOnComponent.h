#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"

#include <DirectXMath.h>
#include <memory>
#include <string>
#include <vector>

class CameraComponent;
class GameObject;
class ThirdPersonCameraComponent;
class SpecialAttackCameraComponent;

struct LockOnCameraCompositionCorrection
{
    // 横方向の構図補正。正負で左右へずらす。
    float sideOffset = 0.0f;

    // カメラ距離・高さ・注視点高さへの加算補正。
    float distanceOffset = 0.0f;
    float heightOffset = 0.0f;
    float lookAtHeightOffset = 0.0f;
    float focusTargetRateOffset = 0.0f;
    float fovOffset = 0.0f;

    // 補正全体の混ぜ具合。CombatCameraCompositionComponentから渡される。
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
    // 第二形態の設定を実行時だけ使用し、保存時は元の設定を維持する。
    void ApplyRuntimeSettings(const nlohmann::json& settings_);
    // プレビュー終了時に、切替前の設定へ戻す。
    void ClearRuntimeSettings();

    // LockOn制御全体の有効切り替え。
    void SetLockOnEnabled(bool enabled_);

    // CameraActionなど、一時演出中にLockOnの通常制御を止める。
    void SetActionSuppressed(bool suppressed_);

    bool IsLockOnEnabled() const
    {
        return lockOnEnabled;
    }

    bool IsActionSuppressed() const
    {
        return actionSuppressed;
    }

    bool HasTrackingPose() const { return lockOnEnabled && !actionSuppressed && hasTrackingPose; }
    DirectX::XMFLOAT3 GetCollisionAnchor() const { return collisionAnchor; }
    float GetWallCameraRate() const;
    float GetCurrentTargetDistance() const;
    DirectX::XMFLOAT3 GetCurrentBaseCameraPosition() const;
    DirectX::XMFLOAT3 GetCurrentBaseLookAtPosition() const;

    const LockOnCameraCompositionCorrection& GetCompositionCorrection() const
    {
        return compositionCorrection;
    }

    // 現在のPlayer/Target/FocusTarget参照。
    std::shared_ptr<GameObject> GetPlayerTarget() const
    {
        return playerTarget.lock();
    }

    std::shared_ptr<GameObject> GetLockOnTarget() const
    {
        return lockOnTarget.lock();
    }

    std::shared_ptr<GameObject> GetLockOnFocusTarget() const
    {
        return lockOnFocusTarget.lock();
    }

    void SetPlayerTarget(const std::shared_ptr<GameObject>& playerTarget_);
    void SetLockOnTarget(const std::shared_ptr<GameObject>& lockOnTarget_);
    void SetLockOnFocusTarget(const std::shared_ptr<GameObject>& focusTarget_);
    // 削除予約された対象と、名前からの再解決を解除する。
    void ForgetObject(const std::shared_ptr<GameObject>& object_);

    // Playerから見て、画面位置・距離・優先度が最も良いLockOnTargetを探す。
    std::shared_ptr<GameObject> FindBestLockOnTarget(
        const std::shared_ptr<GameObject>& playerTarget_) const;

    // 入力方向に応じて、現在Targetから左右/上下に近い候補へ切り替える。
    bool SwitchLockOnTarget(const DirectX::XMFLOAT2& inputDirection_);
    bool SwitchLockOnTargetNext();
    bool SwitchLockOnTargetPrevious();

    // Player指定でLockOnのON/OFFを切り替える。
    bool ToggleLockOnForPlayer(const std::shared_ptr<GameObject>& playerTarget_);

    // CombatCameraCompositionComponentなどから、攻撃中の構図補正を受け取る。
    void SetCompositionCorrection(
        const LockOnCameraCompositionCorrection& correction_);
    void ClearCompositionCorrection();

private:
    // 所有ObjectからCamera/ThirdPersonCameraを探し、名前指定Targetも解決する。
    void ResolveReferences();

    // LockOn切り替えなどのActionを登録する。
    void RegisterActions();

    // 補間状態や壁カメラ状態を初期化する。
    void ResetTracking();

    // LockOnが通常TPS制御を奪っていた場合に返す。
    void ReleaseThirdPersonCamera();
    void ApplyCameraPose(
        const DirectX::XMFLOAT3& cameraPosition_,
        const DirectX::XMFLOAT3& lookAtPosition_);

    DirectX::XMFLOAT3 GetFocusPosition(
        const std::shared_ptr<GameObject>& object_,
        const DirectX::XMFLOAT3& fallbackOffset_) const;
    bool ProjectToScreenPosition(
        const DirectX::XMFLOAT3& cameraPosition_,
        const DirectX::XMFLOAT3& lookAtPosition_,
        const DirectX::XMFLOAT3& worldPosition_,
            float fovY_,
            DirectX::XMFLOAT2& screenPosition_) const;

    struct LockOnDistanceInfo
    {
        // PlayerとTarget間の距離情報。カメラ距離や近距離補正の基準になる。
        DirectX::XMFLOAT3 playerPosition{};
        DirectX::XMFLOAT3 targetPosition{};
        DirectX::XMFLOAT3 difference{};
        float horizontalDistance = 0.0f;
        float targetDistance = 0.0f;
        bool isValid = false;
    };

    struct LockOnCandidate
    {
        // LockOn可能な候補。focusTargetは実際に見る点、combatOwnerは本体Object。
        std::weak_ptr<GameObject> focusTarget;
        std::weak_ptr<GameObject> combatOwner;
        DirectX::XMFLOAT3 worldPosition{};
        DirectX::XMFLOAT2 screenPosition{};
        float distance = 0.0f;
        float priority = 1.0f;
        bool isVisible = false;
        bool isDefaultTarget = false;
        bool isCombatOwnerSelf = false;

        // 距離、画面中心、優先度などを合成した選択スコア。
        float score = 0.0f;
    };

    LockOnDistanceInfo CalculateLockOnDistanceInfo() const;
    std::vector<LockOnCandidate> CollectLockOnCandidates(
        const std::shared_ptr<GameObject>& playerTarget_) const;

    // 候補一覧から最もスコアの高いTargetを選ぶ。
    LockOnCandidate FindBestLockOnCandidate(
        const std::shared_ptr<GameObject>& playerTarget_) const;
    std::shared_ptr<GameObject> FindDirectionalLockOnTarget(
        const DirectX::XMFLOAT2& inputDirection_) const;

    // 選ばれた候補を現在Targetとして反映する。
    bool ApplyLockOnCandidate(const LockOnCandidate& candidate_);
    bool ResolveFocusTargetFallback();
    void UpdateLockOnSwitchInput(float deltaTime_);
    DirectX::XMFLOAT2 GetKeyboardSwitchInput() const;
    DirectX::XMFLOAT2 GetControllerSwitchInput() const;
    DirectX::XMFLOAT2 GetMouseSwitchInput(float deltaTime_);
    void RegisterDefaultSwitchInputBindings();
    void DrawLockOnDebugGui();

private:
    // LockOnが追跡するPlayer、Target、注視点Target。
    std::weak_ptr<GameObject> playerTarget;
    std::weak_ptr<GameObject> lockOnTarget;
    std::weak_ptr<GameObject> lockOnFocusTarget;
    std::weak_ptr<CameraComponent> camera;
    std::weak_ptr<ThirdPersonCameraComponent> thirdPersonCamera;
    std::weak_ptr<SpecialAttackCameraComponent> specialAttackCamera;

    std::string playerTargetName;
    std::string lockOnTargetName;
    std::string lockOnFocusTargetName;
    bool lockOnEnabled = false;
    bool actionSuppressed = false;

    // Player/Targetの注視点補正。
    DirectX::XMFLOAT3 playerOffset{ 0.0f, 1.5f, 0.0f };
    DirectX::XMFLOAT3 targetOffset{ 0.0f, 1.2f, 0.0f };
    float maxDistance = 28.0f;

    // TPSコンポーネントが無い場合に使う距離。
    float baseDistance = 7.0f;
    float cameraHeight = 2.2f;
    float sideOffset = 1.25f;
    float currentSide = 1.0f;
    // 至近距離で対象が横切ったときの急旋回を抑える。
    bool enableCloseRangeDistanceGuard = false;
    float closeRangeDirectionHoldDistance = 1.5f;
    float closeRangeOrbitTurnSpeed = 90.0f;
    DirectX::XMFLOAT3 closeRangeBackDirection{ 0.0f, 0.0f, -1.0f };
    bool hasCloseRangeBackDirection = false;

    // 近距離/遠距離で注視点のPlayer寄り/Target寄りを切り替える。
    float closeDistance = 8.0f;
    float closeFocusBias = 0.28f;
    float farFocusBias = 0.5f;
    float positionInterpolationSpeed = 10.0f;
    float lookAtInterpolationSpeed = 12.5f;
    // LockOnTarget切り替え入力のしきい値とクールダウン。
    float switchStickThreshold = 0.65f;
    float switchStickReleaseThreshold = 0.25f;
    float switchCooldown = 0.18f;
    float mouseSwitchThreshold = 35.0f;
    float mouseAccumulationResetTime = 0.12f;
    float directionalDotThreshold = 0.35f;
    float screenDistanceWeight = 0.55f;
    float centerDistanceWeight = 0.35f;
    float priorityWeight = 0.25f;

    // カメラ位置と注視点の補間状態。
    DirectX::XMFLOAT3 interpolatedPosition{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 interpolatedLookAt{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 collisionAnchor{};
    DirectX::XMFLOAT3 currentBaseCameraPosition{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 currentBaseLookAtPosition{ 0.0f, 0.0f, 0.0f };

    // Target切り替え検出用の前回参照。
    std::weak_ptr<GameObject> previousLockOnTarget;
    LockOnCameraCompositionCorrection compositionCorrection;
    float currentTargetDistance = 0.0f;

    // Debug GUIで表示する画面投影・候補スコア。
    mutable int debugCandidateCount = 0;
    mutable float debugLastSwitchScore = 0.0f;
    DirectX::XMFLOAT2 debugLastSwitchDirection{ 0.0f, 0.0f };
    DirectX::XMFLOAT2 debugControllerSwitchInput{ 0.0f, 0.0f };
    DirectX::XMFLOAT2 debugMouseAccumulatedDelta{ 0.0f, 0.0f };
    mutable DirectX::XMFLOAT2 debugCurrentFocusScreen{ 0.5f, 0.5f };
    bool hasTrackingPose = false;
    bool ownsThirdPersonControl = false;
    bool switchInputLatched = false;
    float switchCooldownTimer = 0.0f;
    float mouseAccumulationTimer = 0.0f;
    DirectX::XMFLOAT2 accumulatedMouseDelta{ 0.0f, 0.0f };
    bool runtimeSettingsActive = false;
    nlohmann::json originalRuntimeSettings;
    nlohmann::json legacySpecialAttackSettings;
};

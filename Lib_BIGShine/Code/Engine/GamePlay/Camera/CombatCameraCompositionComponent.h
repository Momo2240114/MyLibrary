#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/GamePlay/EventSystem/IActionProvider.h"
#include "Engine/GamePlay/Camera/CameraLockOnComponent.h"

#include <DirectXMath.h>
#include <array>
#include <memory>
#include <string>
#include <vector>

class BlackboardComponent;
class GameObject;

enum class CAMERA_COMPOSITION_PHASE
{
    // 補正を適用していない状態。
    IDLE,

    // Preset補正を0から1へ混ぜ込む。
    ENTER,

    // 補正を維持する。
    HOLD,

    // 補正を0へ戻す。
    RETURN
};

enum class CAMERA_COMPOSITION_WALL_POLICY
{
    // 壁接近時も補正をそのまま許可する。
    ALLOW,

    // 壁接近時は補正量を弱める。
    REDUCE,

    // 壁方向へ押す成分を抑え、接線方向だけ残す。
    TANGENT_ONLY,

    // 壁接近時は補正を無効化する。
    DISABLE
};

enum class CAMERA_COMPOSITION_SOURCE
{
    // システム側の自動補正。
    SYSTEM,

    // プレイヤー行動由来の補正。
    PLAYER,
    ENEMY,
    STAGE
};

struct CameraCompositionPreset
{
    // Action名やBlackboard要求で指定するプリセット名。
    std::string presetName = "WeakAttackRight";
    CAMERA_COMPOSITION_SOURCE source = CAMERA_COMPOSITION_SOURCE::PLAYER;

    // LockOnCameraCompositionCorrectionへ加算する補正値。
    float sideOffset = 0.0f;
    float distanceOffset = 0.0f;
    float heightOffset = 0.0f;
    float lookAtHeightOffset = 0.0f;
    float focusTargetRateOffset = 0.0f;
    float fovOffset = 0.0f;

    // ENTER/HOLD/RETURNの各時間。
    float blendInDuration = 0.1f;
    float holdDuration = 0.0f;
    float blendOutDuration = 0.2f;

    int priority = 0;

    // 壁や障害物が近い時、構図補正をどう扱うか。
    CAMERA_COMPOSITION_WALL_POLICY wallPolicy =
        CAMERA_COMPOSITION_WALL_POLICY::REDUCE;
    float wallSuppressionStrength = 0.8f;

    bool useUnscaledTime = false;
    bool continueUntilStop = false;

    // trueなら攻撃方向から左右符号を決め、同じPresetを左右どちらにも使えるようにする。
    bool useAttackDirectionSide = true;
};

class CombatCameraCompositionComponent : public Component, public IActionProvider
{
public:
    const std::string& GetName() const override
    {
        static std::string name_ = "CombatCameraCompositionComponent";
        return name_;
    }

    void Start() override;
    void LateUpdate(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;
    void RegisterActions() override;

    // Preset名だけで再生する。Preset側のsourceが使われる。
    void PlayPresetByName(const std::string& presetName_);
    void PlayPresetByName(
        const std::string& presetName_,
        CAMERA_COMPOSITION_SOURCE source_);
    void StopPreset(const std::string& presetName_);
    void StopPreset(
        const std::string& presetName_,
        CAMERA_COMPOSITION_SOURCE source_);

    // すべてのレイヤー補正を解除する。
    void ClearPreset();

private:
    struct CameraCompositionLayerState
    {
        // レイヤーごとに現在再生しているPreset。
        CameraCompositionPreset activePreset;
        CAMERA_COMPOSITION_SOURCE source = CAMERA_COMPOSITION_SOURCE::PLAYER;
        CAMERA_COMPOSITION_PHASE phase = CAMERA_COMPOSITION_PHASE::IDLE;

        // Debug表示とStop要求照合に使う名前。
        std::string activePresetName;
        std::string requesterName;
        float phaseTimer = 0.0f;
        int activePriority = -1;

        // 攻撃方向から決めた左右符号。開始時に固定して途中で反転しないようにする。
        float lockedSideSign = 1.0f;
    };

    // LockOnCameraやBlackboardの参照を取り直す。
    void ResolveReferences();

    // 初期状態で使える標準Presetを補充する。
    void LoadDefaultPresets();

    // Blackboard経由のPlay/Stop/Clear要求を読み取る。
    void ReadBlackboardRequests();
    void BeginPreset(const CameraCompositionPreset& preset_);
    void BeginPreset(
        const CameraCompositionPreset& preset_,
        CAMERA_COMPOSITION_SOURCE source_);
    int FindPresetIndex(const std::string& presetName_) const;
    int FindPresetIndex(
        const std::string& presetName_,
        CAMERA_COMPOSITION_SOURCE source_) const;
    int GetSourceLayerIndex(CAMERA_COMPOSITION_SOURCE source_) const;

    // 現在Phaseに対応した秒数を返す。
    float GetPhaseDuration(const CameraCompositionLayerState& layer_) const;

    // PhaseTimerから0～1の補正重みを求める。
    float CalculatePhaseWeight(const CameraCompositionLayerState& layer_) const;
    float SmoothStep(float value_) const;
    float ResolveDirectionalSide(
        float sideOffset_,
        bool useAttackDirectionSide_) const;

    // 単一Layerの状態をLockOnCameraCompositionCorrectionへ変換する。
    LockOnCameraCompositionCorrection BuildCorrection(
        const CameraCompositionLayerState& layer_) const;
    float CalculateCloseRangeAssistWeight(float targetDistance_) const;

    // 近距離でプレイヤーが見切れにくいよう、自動の構図補正を作る。
    LockOnCameraCompositionCorrection BuildCloseRangePlayerCorrection() const;

    // 複数SourceのLayerを優先度込みで合成する。
    LockOnCameraCompositionCorrection BuildLayeredCorrection() const;
    LockOnCameraCompositionCorrection ClampCorrection(
        const LockOnCameraCompositionCorrection& correction_) const;
    std::string GetPlayActionName(const std::string& presetName_) const;
    const char* ToString(CAMERA_COMPOSITION_PHASE phase_) const;
    const char* ToString(CAMERA_COMPOSITION_WALL_POLICY policy_) const;
    const char* ToString(CAMERA_COMPOSITION_SOURCE source_) const;
    CAMERA_COMPOSITION_WALL_POLICY WallPolicyFromString(
        const std::string& value_) const;
    CAMERA_COMPOSITION_SOURCE SourceFromString(const std::string& value_) const;

private:
    // 補正を適用するLockOnCameraと、外部要求を受け取るBlackboard。
    std::weak_ptr<CameraLockOnComponent> lockOnCamera;
    std::weak_ptr<BlackboardComponent> blackboard;

    std::string targetCameraObjectName;
    std::string blackboardCategory = "CameraComposition";
    std::string playTriggerKey = "Play";
    std::string stopTriggerKey = "Stop";
    std::string clearTriggerKey = "Clear";
    std::string presetNameKey = "PresetName";
    std::string sourceKey = "Source";
    std::string requesterKey = "Requester";
    std::string attackDirectionKey = "AttackDirection";

    // 保存済みPreset一覧と、Source別に同時管理するLayer。
    std::vector<CameraCompositionPreset> presets;
    std::array<CameraCompositionLayerState, 3> layers;

    float maximumSideOffset = 0.8f;
    float minimumDistanceOffset = -1.0f;
    float maximumDistanceOffset = 1.5f;
    float minimumHeightOffset = -0.5f;
    float maximumHeightOffset = 0.7f;
    float minimumFovOffset = -5.0f;
    float maximumFovOffset = 6.0f;
    float minimumFocusTargetRateOffset = -0.2f;
    float maximumFocusTargetRateOffset = 0.25f;

    // 近距離ロックオン時に自動で少し引く補正。
    bool enableCloseRangePlayerAssist = true;
    float closeRangeAssistMinimumDistance = 0.0f;
    float closeRangeAssistMaximumDistance = 6.5f;
    float closeRangeAssistBlendDistance = 1.0f;
    float closeRangeAssistDistanceOffset = 0.45f;
    float closeRangeAssistMaximumDistanceOffset = 10.0f;
    float closeRangeAssistHeightOffset = -2.0f;

    LockOnCameraCompositionCorrection debugAppliedCorrection;
    mutable float debugCloseRangeAssistWeight = 0.0f;
};

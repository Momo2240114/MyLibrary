#include "PrecompiledHeader.h"

#include "CombatCameraCompositionComponent.h"

#include "CameraComponent.h"
#include "Engine/Debug/TracyProfiler.h"
#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Serialize/ActionRegistry.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
    DirectX::XMFLOAT3 SubtractFloat3(
        const DirectX::XMFLOAT3& left_,
        const DirectX::XMFLOAT3& right_)
    {
        return { left_.x - right_.x, left_.y - right_.y, left_.z - right_.z };
    }

    DirectX::XMFLOAT3 NormalizeHorizontal(
        const DirectX::XMFLOAT3& value_,
        const DirectX::XMFLOAT3& fallback_)
    {
        // カメラ構図は水平面の左右判定だけに使うため、Y成分を落として正規化する。
        // 無効な方向の場合はfallbackを使い、それも無効なら前方向へ倒す。
        const float length_ = std::sqrt(value_.x * value_.x + value_.z * value_.z);
        if (length_ <= 0.0001f)
        {
            const float fallbackLength_ =
                std::sqrt(fallback_.x * fallback_.x + fallback_.z * fallback_.z);
            if (fallbackLength_ <= 0.0001f) return { 0.0f, 0.0f, 1.0f };
            return
            {
                fallback_.x / fallbackLength_,
                0.0f,
                fallback_.z / fallbackLength_
            };
        }
        return { value_.x / length_, 0.0f, value_.z / length_ };
    }

    float DotHorizontal(
        const DirectX::XMFLOAT3& left_,
        const DirectX::XMFLOAT3& right_)
    {
        return left_.x * right_.x + left_.z * right_.z;
    }
}

void CombatCameraCompositionComponent::Start()
{
    BS_PROFILE_SCOPE("CombatCameraCompositionComponent::Start");
    // BlackboardとLockOnCameraを解決し、プリセットが無ければ攻撃用の初期構図を用意する。
    ResolveReferences();
    if (presets.empty())
    {
        LoadDefaultPresets();
    }
    RegisterActions();
}

void CombatCameraCompositionComponent::LateUpdate(float deltaTime_)
{
    BS_PROFILE_SCOPE("CombatCameraCompositionComponent::LateUpdate");
    ResolveReferences();
    ReadBlackboardRequests();

    const std::shared_ptr<CameraLockOnComponent> lockOn_ = lockOnCamera.lock();
    if (!lockOn_ || !lockOn_->IsLockOnEnabled())
    {
        // ロックオンしていない時は構図補正を出さず、デバッグ値も空にする。
        debugAppliedCorrection = {};
        debugCloseRangeAssistWeight = 0.0f;
        return;
    }

    bool hasActiveLayer_ = false;
    for (CameraCompositionLayerState& layer_ : layers)
    {
        if (layer_.phase != CAMERA_COMPOSITION_PHASE::IDLE)
        {
            hasActiveLayer_ = true;
            break;
        }
    }

    if (!hasActiveLayer_)
    {
        // 攻撃演出などのレイヤーが無い場合でも、近距離だけはプレイヤーが見切れない補正を入れる。
        debugAppliedCorrection =
            ClampCorrection(BuildCloseRangePlayerCorrection());
        if (debugAppliedCorrection.weight > 0.0001f)
        {
            lockOn_->SetCompositionCorrection(debugAppliedCorrection);
        }
        else
        {
            lockOn_->ClearCompositionCorrection();
        }
        return;
    }

    for (CameraCompositionLayerState& layer_ : layers)
    {
        if (layer_.phase == CAMERA_COMPOSITION_PHASE::IDLE) continue;

        // 各レイヤーはENTER -> HOLD -> RETURNの順に進み、RETURN終了で空になる。
        layer_.phaseTimer += (std::max)(deltaTime_, 0.0f);
        const float duration_ = GetPhaseDuration(layer_);
        const bool phaseFinished_ = duration_ <= 0.0001f
            ? true
            : layer_.phaseTimer >= duration_;

        if (!phaseFinished_) continue;

        if (layer_.phase == CAMERA_COMPOSITION_PHASE::ENTER)
        {
            layer_.phase = CAMERA_COMPOSITION_PHASE::HOLD;
            layer_.phaseTimer = 0.0f;
            continue;
        }

        if (layer_.phase == CAMERA_COMPOSITION_PHASE::HOLD)
        {
            if (layer_.activePreset.continueUntilStop) continue;
            layer_.phase = CAMERA_COMPOSITION_PHASE::RETURN;
            layer_.phaseTimer = 0.0f;
            continue;
        }

        layer_ = {};
    }

    debugAppliedCorrection = BuildLayeredCorrection();
    // 最終的な合成補正はLockOnCameraへ渡し、ベース追従カメラの上に重ねる。
    lockOn_->SetCompositionCorrection(debugAppliedCorrection);
}

void CombatCameraCompositionComponent::PlayPresetByName(
    const std::string& presetName_)
{
    const int index_ = FindPresetIndex(presetName_);
    if (index_ < 0) return;

    PlayPresetByName(presetName_, presets[index_].source);
}

void CombatCameraCompositionComponent::PlayPresetByName(
    const std::string& presetName_,
    CAMERA_COMPOSITION_SOURCE source_)
{
    BS_PROFILE_SCOPE("CombatCameraCompositionComponent::PlayPresetByName");
    const int index_ = FindPresetIndex(presetName_, source_);
    if (index_ < 0) return;

    BeginPreset(presets[index_], source_);
}

void CombatCameraCompositionComponent::StopPreset(
    const std::string& presetName_)
{
    // 名前指定が空なら全レイヤー、名前があれば一致するプリセットだけをRETURNへ移行する。
    for (CameraCompositionLayerState& layer_ : layers)
    {
        if (layer_.phase == CAMERA_COMPOSITION_PHASE::IDLE) continue;
        if (!presetName_.empty() && presetName_ != layer_.activePresetName) continue;

        layer_.phase = CAMERA_COMPOSITION_PHASE::RETURN;
        layer_.phaseTimer = 0.0f;
    }
}

void CombatCameraCompositionComponent::StopPreset(
    const std::string& presetName_,
    CAMERA_COMPOSITION_SOURCE source_)
{
    const int layerIndex_ = GetSourceLayerIndex(source_);
    CameraCompositionLayerState& layer_ = layers[layerIndex_];
    if (layer_.phase == CAMERA_COMPOSITION_PHASE::IDLE) return;
    if (!presetName_.empty() && presetName_ != layer_.activePresetName) return;

    layer_.phase = CAMERA_COMPOSITION_PHASE::RETURN;
    layer_.phaseTimer = 0.0f;
}

void CombatCameraCompositionComponent::ClearPreset()
{
    // すべての構図補正を即座に捨て、LockOnCamera側にも補正解除を通知する。
    for (CameraCompositionLayerState& layer_ : layers)
    {
        layer_ = {};
    }
    debugAppliedCorrection = {};
    debugCloseRangeAssistWeight = 0.0f;
    if (const std::shared_ptr<CameraLockOnComponent> lockOn_ = lockOnCamera.lock())
    {
        lockOn_->ClearCompositionCorrection();
    }
}

void CombatCameraCompositionComponent::RegisterActions()
{
    ActionRegistry& registry_ = ActionRegistry::Instance();
    // アニメーションイベントやAIからプリセット名で再生できるよう、ActionRegistryへ登録する。
    registry_.RegisterAction("CameraComposition", "---", nullptr);
    registry_.RemoveAction("CameraComposition", "CameraComposition.Stop");
    registry_.RemoveAction("CameraComposition", "CameraComposition.Clear");
    registry_.RegisterAction("CameraComposition", "CameraComposition.Stop", [this]()
    {
        StopPreset(std::string{});
    });
    registry_.RegisterAction("CameraComposition", "CameraComposition.Clear", [this]()
    {
        ClearPreset();
    });

    for (const CameraCompositionPreset& preset_ : presets)
    {
        const std::string playActionName_ = GetPlayActionName(preset_.presetName);
        const std::string sourceActionName_ =
            "CameraComposition.Play." +
            std::string(ToString(preset_.source)) +
            "." +
            preset_.presetName;
        const std::string shortActionName_ = "CameraComposition_" + preset_.presetName;
        registry_.RemoveAction("CameraComposition", playActionName_);
        registry_.RemoveAction("CameraComposition", sourceActionName_);
        registry_.RemoveAction("CameraComposition", shortActionName_);
        registry_.RegisterAction(
            "CameraComposition",
            playActionName_,
            [this, presetName_ = preset_.presetName]()
            {
                PlayPresetByName(presetName_);
            });
        registry_.RegisterAction(
            "CameraComposition",
            sourceActionName_,
            [this, presetName_ = preset_.presetName, source_ = preset_.source]()
            {
                PlayPresetByName(presetName_, source_);
            });
        registry_.RegisterAction(
            "CameraComposition",
            shortActionName_,
            [this, presetName_ = preset_.presetName]()
            {
                PlayPresetByName(presetName_);
            });
    }
}

void CombatCameraCompositionComponent::ResolveReferences()
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;

    // 基本は同じGameObject上のBlackboard/LockOnCameraを使い、必要なら指定名のカメラObjectも探す。
    if (blackboard.expired())
    {
        blackboard = owner_->GetComponent<BlackboardComponent>();
    }
    if (!lockOnCamera.expired()) return;

    lockOnCamera = owner_->GetComponent<CameraLockOnComponent>();
    if (!lockOnCamera.expired() || targetCameraObjectName.empty() || !owner_->manager)
    {
        return;
    }

    const std::shared_ptr<GameObject> cameraObject_ =
        owner_->manager->Find(targetCameraObjectName);
    if (cameraObject_)
    {
        lockOnCamera = cameraObject_->GetComponent<CameraLockOnComponent>();
    }
}

void CombatCameraCompositionComponent::LoadDefaultPresets()
{
    // アセット未設定でも攻撃演出が確認できるよう、弱/強攻撃と敵攻撃用の初期構図を登録する。
    CameraCompositionPreset weakLeft_;
    weakLeft_.presetName = "WeakAttackLeft";
    weakLeft_.sideOffset = -0.2f;
    weakLeft_.distanceOffset = 0.15f;
    weakLeft_.heightOffset = -0.05f;
    weakLeft_.fovOffset = 0.3f;
    weakLeft_.blendInDuration = 0.08f;
    weakLeft_.holdDuration = 0.08f;
    weakLeft_.blendOutDuration = 0.16f;
    weakLeft_.priority = 10;
    presets.push_back(weakLeft_);

    CameraCompositionPreset weakRight_ = weakLeft_;
    weakRight_.presetName = "WeakAttackRight";
    weakRight_.sideOffset = 0.2f;
    presets.push_back(weakRight_);

    CameraCompositionPreset strong_;
    strong_.presetName = "StrongAttack";
    strong_.sideOffset = 0.45f;
    strong_.distanceOffset = 0.4f;
    strong_.heightOffset = -0.12f;
    strong_.lookAtHeightOffset = 0.1f;
    strong_.fovOffset = 1.0f;
    strong_.blendInDuration = 0.1f;
    strong_.holdDuration = 0.12f;
    strong_.blendOutDuration = 0.24f;
    strong_.priority = 20;
    strong_.wallPolicy = CAMERA_COMPOSITION_WALL_POLICY::TANGENT_ONLY;
    presets.push_back(strong_);

    CameraCompositionPreset approach_;
    approach_.presetName = "ApproachEnemy";
    approach_.source = CAMERA_COMPOSITION_SOURCE::PLAYER;
    approach_.distanceOffset = 0.35f;
    approach_.heightOffset = 0.08f;
    approach_.lookAtHeightOffset = 0.1f;
    approach_.focusTargetRateOffset = 0.08f;
    approach_.fovOffset = 0.4f;
    approach_.blendInDuration = 0.18f;
    approach_.holdDuration = 0.0f;
    approach_.blendOutDuration = 0.25f;
    approach_.priority = 5;
    approach_.useAttackDirectionSide = false;
    presets.push_back(approach_);

    CameraCompositionPreset enemyBreath_;
    enemyBreath_.presetName = "EnemyBreathCharge";
    enemyBreath_.source = CAMERA_COMPOSITION_SOURCE::ENEMY;
    enemyBreath_.distanceOffset = 0.5f;
    enemyBreath_.heightOffset = 0.2f;
    enemyBreath_.lookAtHeightOffset = 0.15f;
    enemyBreath_.focusTargetRateOffset = 0.08f;
    enemyBreath_.fovOffset = 1.0f;
    enemyBreath_.blendInDuration = 0.2f;
    enemyBreath_.holdDuration = 0.0f;
    enemyBreath_.blendOutDuration = 0.25f;
    enemyBreath_.continueUntilStop = true;
    enemyBreath_.priority = 20;
    enemyBreath_.useAttackDirectionSide = false;
    presets.push_back(enemyBreath_);

    CameraCompositionPreset enemyRush_;
    enemyRush_.presetName = "EnemyRushAttack";
    enemyRush_.source = CAMERA_COMPOSITION_SOURCE::ENEMY;
    enemyRush_.distanceOffset = 0.7f;
    enemyRush_.heightOffset = 0.1f;
    enemyRush_.fovOffset = 1.5f;
    enemyRush_.blendInDuration = 0.12f;
    enemyRush_.holdDuration = 0.0f;
    enemyRush_.blendOutDuration = 0.2f;
    enemyRush_.continueUntilStop = true;
    enemyRush_.priority = 25;
    enemyRush_.wallPolicy = CAMERA_COMPOSITION_WALL_POLICY::TANGENT_ONLY;
    enemyRush_.useAttackDirectionSide = false;
    presets.push_back(enemyRush_);

    CameraCompositionPreset enemyWide_;
    enemyWide_.presetName = "EnemyWideAttack";
    enemyWide_.source = CAMERA_COMPOSITION_SOURCE::ENEMY;
    enemyWide_.distanceOffset = 0.8f;
    enemyWide_.heightOffset = 0.25f;
    enemyWide_.lookAtHeightOffset = 0.1f;
    enemyWide_.focusTargetRateOffset = 0.02f;
    enemyWide_.fovOffset = 2.0f;
    enemyWide_.blendInDuration = 0.15f;
    enemyWide_.holdDuration = 0.15f;
    enemyWide_.blendOutDuration = 0.25f;
    enemyWide_.priority = 30;
    enemyWide_.useAttackDirectionSide = false;
    presets.push_back(enemyWide_);

    CameraCompositionPreset enemySide_;
    enemySide_.presetName = "EnemySideAttack";
    enemySide_.source = CAMERA_COMPOSITION_SOURCE::ENEMY;
    enemySide_.sideOffset = 0.2f;
    enemySide_.distanceOffset = 0.3f;
    enemySide_.fovOffset = 0.5f;
    enemySide_.blendInDuration = 0.08f;
    enemySide_.holdDuration = 0.08f;
    enemySide_.blendOutDuration = 0.18f;
    enemySide_.priority = 15;
    enemySide_.wallPolicy = CAMERA_COMPOSITION_WALL_POLICY::TANGENT_ONLY;
    presets.push_back(enemySide_);
}

void CombatCameraCompositionComponent::ReadBlackboardRequests()
{
    const std::shared_ptr<BlackboardComponent> blackboard_ = blackboard.lock();
    if (!blackboard_) return;

    // BlackboardのTriggerを消費して、AI/戦闘側からカメラ構図のPlay/Stop/Clearを指示する。
    if (blackboard_->ConsumeTrigger(blackboardCategory, clearTriggerKey))
    {
        ClearPreset();
        return;
    }

    if (blackboard_->ConsumeTrigger(blackboardCategory, stopTriggerKey))
    {
        std::string presetName_;
        std::string sourceName_;
        blackboard_->GetString(blackboardCategory, presetNameKey, presetName_);
        blackboard_->GetString(blackboardCategory, sourceKey, sourceName_);
        StopPreset(presetName_, SourceFromString(sourceName_));
        return;
    }

    if (blackboard_->ConsumeTrigger(blackboardCategory, playTriggerKey))
    {
        std::string presetName_;
        std::string sourceName_;
        if (blackboard_->GetString(blackboardCategory, presetNameKey, presetName_))
        {
            blackboard_->GetString(blackboardCategory, sourceKey, sourceName_);
            PlayPresetByName(presetName_, SourceFromString(sourceName_));
        }
    }
}

void CombatCameraCompositionComponent::BeginPreset(
    const CameraCompositionPreset& preset_)
{
    BeginPreset(preset_, preset_.source);
}

void CombatCameraCompositionComponent::BeginPreset(
    const CameraCompositionPreset& preset_,
    CAMERA_COMPOSITION_SOURCE source_)
{
    const int layerIndex_ = GetSourceLayerIndex(source_);
    CameraCompositionLayerState& layer_ = layers[layerIndex_];
    // 同じレイヤーに高優先度の補正が残っている場合、低優先度プリセットでは上書きしない。
    if (layer_.phase != CAMERA_COMPOSITION_PHASE::IDLE &&
        preset_.priority < layer_.activePriority)
    {
        return;
    }

    layer_.activePreset = preset_;
    layer_.source = source_;
    layer_.activePreset.source = source_;
    layer_.activePresetName = preset_.presetName;
    layer_.activePriority = preset_.priority;
    // 攻撃方向基準の左右補正は、開始時に左右を固定して途中で反転しないようにする。
    layer_.lockedSideSign = ResolveDirectionalSide(
        preset_.sideOffset,
        preset_.useAttackDirectionSide);
    layer_.phase = CAMERA_COMPOSITION_PHASE::ENTER;
    layer_.phaseTimer = 0.0f;
}

int CombatCameraCompositionComponent::FindPresetIndex(
    const std::string& presetName_) const
{
    for (int index_ = 0; index_ < static_cast<int>(presets.size()); ++index_)
    {
        if (presets[index_].presetName == presetName_) return index_;
    }
    return -1;
}

int CombatCameraCompositionComponent::FindPresetIndex(
    const std::string& presetName_,
    CAMERA_COMPOSITION_SOURCE source_) const
{
    for (int index_ = 0; index_ < static_cast<int>(presets.size()); ++index_)
    {
        if (presets[index_].presetName == presetName_ &&
            presets[index_].source == source_)
        {
            return index_;
        }
    }

    return FindPresetIndex(presetName_);
}

int CombatCameraCompositionComponent::GetSourceLayerIndex(
    CAMERA_COMPOSITION_SOURCE source_) const
{
    // System/Stage、Player、Enemyでレイヤーを分け、同時発生した補正を後で合成する。
    if (source_ == CAMERA_COMPOSITION_SOURCE::ENEMY) return 2;
    if (source_ == CAMERA_COMPOSITION_SOURCE::STAGE ||
        source_ == CAMERA_COMPOSITION_SOURCE::SYSTEM)
    {
        return 0;
    }
    return 1;
}

float CombatCameraCompositionComponent::GetPhaseDuration(
    const CameraCompositionLayerState& layer_) const
{
    if (layer_.phase == CAMERA_COMPOSITION_PHASE::ENTER)
    {
        return layer_.activePreset.blendInDuration;
    }
    if (layer_.phase == CAMERA_COMPOSITION_PHASE::HOLD)
    {
        return layer_.activePreset.holdDuration;
    }
    if (layer_.phase == CAMERA_COMPOSITION_PHASE::RETURN)
    {
        return layer_.activePreset.blendOutDuration;
    }
    return 0.0f;
}

float CombatCameraCompositionComponent::CalculatePhaseWeight(
    const CameraCompositionLayerState& layer_) const
{
    // ENTER/RETURNだけSmoothStepで重みを変え、HOLD中は100%補正として扱う。
    const float duration_ = GetPhaseDuration(layer_);
    const float rate_ = duration_ <= 0.0001f
        ? 1.0f
        : SmoothStep(layer_.phaseTimer / duration_);
    if (layer_.phase == CAMERA_COMPOSITION_PHASE::ENTER) return rate_;
    if (layer_.phase == CAMERA_COMPOSITION_PHASE::HOLD) return 1.0f;
    if (layer_.phase == CAMERA_COMPOSITION_PHASE::RETURN) return 1.0f - rate_;
    return 0.0f;
}

float CombatCameraCompositionComponent::SmoothStep(float value_) const
{
    const float clampedValue_ = std::clamp(value_, 0.0f, 1.0f);
    return clampedValue_ * clampedValue_ * (3.0f - 2.0f * clampedValue_);
}

float CombatCameraCompositionComponent::ResolveDirectionalSide(
    float sideOffset_,
    bool useAttackDirectionSide_) const
{
    if (!useAttackDirectionSide_) return sideOffset_ >= 0.0f ? 1.0f : -1.0f;

    // 攻撃方向が画面右へ向いているかで、構図の横ずらし方向を決める。
    // これにより、攻撃の流れが画面内へ入る方向へカメラを寄せられる。
    const std::shared_ptr<BlackboardComponent> blackboard_ = blackboard.lock();
    const std::shared_ptr<CameraLockOnComponent> lockOn_ = lockOnCamera.lock();
    if (!blackboard_ || !lockOn_) return sideOffset_ >= 0.0f ? 1.0f : -1.0f;

    DirectX::SimpleMath::Vector3 attackDirection_{};
    if (!blackboard_->GetVector3(blackboardCategory, attackDirectionKey, attackDirection_))
    {
        return sideOffset_ >= 0.0f ? 1.0f : -1.0f;
    }

    const DirectX::XMFLOAT3 cameraPosition_ = lockOn_->GetCurrentBaseCameraPosition();
    const DirectX::XMFLOAT3 lookAtPosition_ = lockOn_->GetCurrentBaseLookAtPosition();
    const DirectX::XMFLOAT3 cameraBack_ = NormalizeHorizontal(
        SubtractFloat3(cameraPosition_, lookAtPosition_),
        { 0.0f, 0.0f, -1.0f });
    const DirectX::XMFLOAT3 cameraRight_
    {
        cameraBack_.z,
        0.0f,
        -cameraBack_.x
    };
    const DirectX::XMFLOAT3 attackDirectionXmf_
    {
        attackDirection_.x,
        attackDirection_.y,
        attackDirection_.z
    };
    return DotHorizontal(attackDirectionXmf_, cameraRight_) >= 0.0f ? 1.0f : -1.0f;
}

LockOnCameraCompositionCorrection
CombatCameraCompositionComponent::BuildCorrection(
    const CameraCompositionLayerState& layer_) const
{
    LockOnCameraCompositionCorrection correction_;
    // プリセット値をLockOnCamera用の補正値へ変換し、現在フェーズの重みも付ける。
    correction_.sideOffset = layer_.activePreset.useAttackDirectionSide
        ? std::abs(layer_.activePreset.sideOffset) * layer_.lockedSideSign
        : layer_.activePreset.sideOffset;
    correction_.distanceOffset = layer_.activePreset.distanceOffset;
    correction_.heightOffset = layer_.activePreset.heightOffset;
    correction_.lookAtHeightOffset = layer_.activePreset.lookAtHeightOffset;
    correction_.focusTargetRateOffset = layer_.activePreset.focusTargetRateOffset;
    correction_.fovOffset = layer_.activePreset.fovOffset;
    correction_.weight = CalculatePhaseWeight(layer_);

    if (const std::shared_ptr<CameraLockOnComponent> lockOn_ = lockOnCamera.lock())
    {
        // 壁際では横ずらしがめり込みやすいため、ポリシーに応じて横補正を弱める。
        const float wallRate_ = lockOn_->GetWallCameraRate();
        if (layer_.activePreset.wallPolicy == CAMERA_COMPOSITION_WALL_POLICY::DISABLE)
        {
            correction_.sideOffset *= 1.0f - wallRate_;
        }
        else if (layer_.activePreset.wallPolicy == CAMERA_COMPOSITION_WALL_POLICY::REDUCE)
        {
            correction_.sideOffset *= std::clamp(
                1.0f - wallRate_ * layer_.activePreset.wallSuppressionStrength,
                0.0f,
                1.0f);
        }
    }

    return correction_;
}

LockOnCameraCompositionCorrection
CombatCameraCompositionComponent::BuildLayeredCorrection() const
{
    // 近距離補正を土台にし、その上へSystem/Player/Enemyレイヤーの補正を加算する。
    LockOnCameraCompositionCorrection result_ = BuildCloseRangePlayerCorrection();
    for (const CameraCompositionLayerState& layer_ : layers)
    {
        if (layer_.phase == CAMERA_COMPOSITION_PHASE::IDLE) continue;

        const LockOnCameraCompositionCorrection layerCorrection_ =
            BuildCorrection(layer_);
        const float weight_ = std::clamp(layerCorrection_.weight, 0.0f, 1.0f);
        result_.sideOffset += layerCorrection_.sideOffset * weight_;
        result_.distanceOffset += layerCorrection_.distanceOffset * weight_;
        result_.heightOffset += layerCorrection_.heightOffset * weight_;
        result_.lookAtHeightOffset +=
            layerCorrection_.lookAtHeightOffset * weight_;
        result_.focusTargetRateOffset +=
            layerCorrection_.focusTargetRateOffset * weight_;
        result_.fovOffset += layerCorrection_.fovOffset * weight_;
        result_.weight = (std::max)(result_.weight, weight_);
    }

    return ClampCorrection(result_);
}

float CombatCameraCompositionComponent::CalculateCloseRangeAssistWeight(
    float targetDistance_) const
{
    // 近距離補正は指定距離帯の中だけ有効にし、境界では自然にフェードさせる。
    if (!enableCloseRangePlayerAssist) return 0.0f;
    if (closeRangeAssistMaximumDistance <= closeRangeAssistMinimumDistance)
    {
        return 0.0f;
    }
    if (targetDistance_ < closeRangeAssistMinimumDistance ||
        targetDistance_ > closeRangeAssistMaximumDistance)
    {
        return 0.0f;
    }

    const float blendDistance_ =
        (std::max)(closeRangeAssistBlendDistance, 0.0f);
    if (blendDistance_ <= 0.0001f) return 1.0f;

    const float inWeight_ = closeRangeAssistMinimumDistance > 0.0001f
        ? std::clamp(
            (targetDistance_ - closeRangeAssistMinimumDistance) / blendDistance_,
            0.0f,
            1.0f)
        : 1.0f;
    const float outWeight_ = std::clamp(
        (closeRangeAssistMaximumDistance - targetDistance_) / blendDistance_,
        0.0f,
        1.0f);
    return SmoothStep((std::min)(inWeight_, outWeight_));
}

LockOnCameraCompositionCorrection
CombatCameraCompositionComponent::BuildCloseRangePlayerCorrection() const
{
    BS_PROFILE_SCOPE("CombatCameraCompositionComponent::BuildCloseRangePlayerCorrection");
    LockOnCameraCompositionCorrection correction_;
    const std::shared_ptr<CameraLockOnComponent> lockOn_ = lockOnCamera.lock();
    if (!lockOn_)
    {
        debugCloseRangeAssistWeight = 0.0f;
        return correction_;
    }

    const float weight_ =
        CalculateCloseRangeAssistWeight(lockOn_->GetCurrentTargetDistance());
    debugCloseRangeAssistWeight = weight_;
    if (weight_ <= 0.0001f) return correction_;

    // 敵に近い時は少し距離/高さを調整し、プレイヤーと敵の見切れを防ぐ。
    correction_.distanceOffset = closeRangeAssistDistanceOffset;
    correction_.heightOffset = closeRangeAssistHeightOffset;
    correction_.lookAtHeightOffset = 0.0f;
    correction_.weight = weight_;
    return correction_;
}

LockOnCameraCompositionCorrection
CombatCameraCompositionComponent::ClampCorrection(
    const LockOnCameraCompositionCorrection& correction_) const
{
    // 複数レイヤー加算後でもカメラが破綻しないよう、最終補正量を全項目で制限する。
    LockOnCameraCompositionCorrection result_ = correction_;
    result_.sideOffset = std::clamp(
        result_.sideOffset,
        -std::abs(maximumSideOffset),
        std::abs(maximumSideOffset));
    const float maximumCameraDistanceOffset_ = enableCloseRangePlayerAssist
        ? (std::max)(
            maximumDistanceOffset,
            closeRangeAssistMaximumDistanceOffset)
        : maximumDistanceOffset;
    result_.distanceOffset = std::clamp(
        result_.distanceOffset,
        minimumDistanceOffset,
        maximumCameraDistanceOffset_);
    const float minimumCameraHeightOffset_ = enableCloseRangePlayerAssist
        ? (std::min)(minimumHeightOffset, closeRangeAssistHeightOffset)
        : minimumHeightOffset;
    result_.heightOffset = std::clamp(
        result_.heightOffset,
        minimumCameraHeightOffset_,
        maximumHeightOffset);
    result_.lookAtHeightOffset = std::clamp(
        result_.lookAtHeightOffset,
        minimumHeightOffset,
        maximumHeightOffset);
    result_.focusTargetRateOffset = std::clamp(
        result_.focusTargetRateOffset,
        minimumFocusTargetRateOffset,
        maximumFocusTargetRateOffset);
    result_.fovOffset = std::clamp(
        result_.fovOffset,
        minimumFovOffset,
        maximumFovOffset);
    result_.weight = std::clamp(result_.weight, 0.0f, 1.0f);
    return result_;
}

std::string CombatCameraCompositionComponent::GetPlayActionName(
    const std::string& presetName_) const
{
    return "CameraComposition.Play." + presetName_;
}

const char* CombatCameraCompositionComponent::ToString(
    CAMERA_COMPOSITION_PHASE phase_) const
{
    switch (phase_)
    {
    case CAMERA_COMPOSITION_PHASE::ENTER: return "ENTER";
    case CAMERA_COMPOSITION_PHASE::HOLD: return "HOLD";
    case CAMERA_COMPOSITION_PHASE::RETURN: return "RETURN";
    case CAMERA_COMPOSITION_PHASE::IDLE:
    default:
        return "IDLE";
    }
}

const char* CombatCameraCompositionComponent::ToString(
    CAMERA_COMPOSITION_WALL_POLICY policy_) const
{
    switch (policy_)
    {
    case CAMERA_COMPOSITION_WALL_POLICY::ALLOW: return "ALLOW";
    case CAMERA_COMPOSITION_WALL_POLICY::TANGENT_ONLY: return "TANGENT_ONLY";
    case CAMERA_COMPOSITION_WALL_POLICY::DISABLE: return "DISABLE";
    case CAMERA_COMPOSITION_WALL_POLICY::REDUCE:
    default:
        return "REDUCE";
    }
}

const char* CombatCameraCompositionComponent::ToString(
    CAMERA_COMPOSITION_SOURCE source_) const
{
    switch (source_)
    {
    case CAMERA_COMPOSITION_SOURCE::SYSTEM: return "SYSTEM";
    case CAMERA_COMPOSITION_SOURCE::ENEMY: return "ENEMY";
    case CAMERA_COMPOSITION_SOURCE::STAGE: return "STAGE";
    case CAMERA_COMPOSITION_SOURCE::PLAYER:
    default:
        return "PLAYER";
    }
}

CAMERA_COMPOSITION_WALL_POLICY CombatCameraCompositionComponent::WallPolicyFromString(
    const std::string& value_) const
{
    if (value_ == "ALLOW") return CAMERA_COMPOSITION_WALL_POLICY::ALLOW;
    if (value_ == "TANGENT_ONLY") return CAMERA_COMPOSITION_WALL_POLICY::TANGENT_ONLY;
    if (value_ == "DISABLE") return CAMERA_COMPOSITION_WALL_POLICY::DISABLE;
    return CAMERA_COMPOSITION_WALL_POLICY::REDUCE;
}

CAMERA_COMPOSITION_SOURCE CombatCameraCompositionComponent::SourceFromString(
    const std::string& value_) const
{
    if (value_ == "SYSTEM") return CAMERA_COMPOSITION_SOURCE::SYSTEM;
    if (value_ == "ENEMY") return CAMERA_COMPOSITION_SOURCE::ENEMY;
    if (value_ == "STAGE") return CAMERA_COMPOSITION_SOURCE::STAGE;
    return CAMERA_COMPOSITION_SOURCE::PLAYER;
}

void CombatCameraCompositionComponent::Gui()
{
    ResolveReferences();
    ImGui::Text("System Layer: %s / %s",
        ToString(layers[0].phase),
        layers[0].activePresetName.c_str());
    ImGui::Text("Player Layer: %s / %s",
        ToString(layers[1].phase),
        layers[1].activePresetName.c_str());
    ImGui::Text("Enemy Layer: %s / %s",
        ToString(layers[2].phase),
        layers[2].activePresetName.c_str());
    ImGui::Text("Weight: %.3f", debugAppliedCorrection.weight);
    ImGui::Text(
        "Applied Side/Distance/Height: %.3f / %.3f / %.3f",
        debugAppliedCorrection.sideOffset,
        debugAppliedCorrection.distanceOffset,
        debugAppliedCorrection.heightOffset);
    ImGui::Text("Target Distance: %.3f",
        lockOnCamera.lock() ? lockOnCamera.lock()->GetCurrentTargetDistance() : 0.0f);
    ImGui::Text("Wall Rate: %.3f",
        lockOnCamera.lock() ? lockOnCamera.lock()->GetWallCameraRate() : 0.0f);
    ImGui::SeparatorText("Close Range Player Assist");
    ImGui::Checkbox("Enable Close Range Assist", &enableCloseRangePlayerAssist);
    ImGui::Text("Assist Weight: %.3f", debugCloseRangeAssistWeight);
    ImGui::DragFloatRange2(
        "Assist Distance Range",
        &closeRangeAssistMinimumDistance,
        &closeRangeAssistMaximumDistance,
        0.05f,
        0.0f,
        100.0f);
    ImGui::DragFloat(
        "Assist Blend Distance",
        &closeRangeAssistBlendDistance,
        0.05f,
        0.0f,
        20.0f);
    ImGui::DragFloat(
        "Assist Distance Offset",
        &closeRangeAssistDistanceOffset,
        0.01f,
        -20.0f,
        50.0f);
    ImGui::DragFloat(
        "Assist Max Distance Offset",
        &closeRangeAssistMaximumDistanceOffset,
        0.05f,
        0.0f,
        50.0f);
    ImGui::DragFloat(
        "Assist Height Offset",
        &closeRangeAssistHeightOffset,
        0.01f,
        -5.0f,
        5.0f);

    char targetBuffer_[128]{};
    strcpy_s(targetBuffer_, targetCameraObjectName.c_str());
    if (ImGui::InputText("Target Camera Object", targetBuffer_, sizeof(targetBuffer_)))
    {
        targetCameraObjectName = targetBuffer_;
        lockOnCamera.reset();
        ResolveReferences();
    }

    char categoryBuffer_[64]{};
    strcpy_s(categoryBuffer_, blackboardCategory.c_str());
    if (ImGui::InputText("Blackboard Category", categoryBuffer_, sizeof(categoryBuffer_)))
    {
        blackboardCategory = categoryBuffer_;
    }

    if (ImGui::Button("Add Preset"))
    {
        CameraCompositionPreset preset_;
        preset_.presetName = "NewComposition";
        presets.push_back(preset_);
        RegisterActions();
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop"))
    {
        StopPreset(std::string{});
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear"))
    {
        ClearPreset();
    }

    for (int index_ = 0; index_ < static_cast<int>(presets.size()); ++index_)
    {
        CameraCompositionPreset& preset_ = presets[index_];
        ImGui::PushID(index_);
        if (ImGui::TreeNode(preset_.presetName.c_str()))
        {
            char nameBuffer_[128]{};
            strcpy_s(nameBuffer_, preset_.presetName.c_str());
            if (ImGui::InputText("Preset Name", nameBuffer_, sizeof(nameBuffer_)))
            {
                preset_.presetName = nameBuffer_;
                RegisterActions();
            }
            int source_ = static_cast<int>(preset_.source);
            if (ImGui::Combo("Source", &source_, "SYSTEM\0PLAYER\0ENEMY\0STAGE\0"))
            {
                preset_.source = static_cast<CAMERA_COMPOSITION_SOURCE>(source_);
            }
            ImGui::DragFloat("Side Offset", &preset_.sideOffset, 0.01f, -5.0f, 5.0f);
            ImGui::DragFloat("Distance Offset", &preset_.distanceOffset, 0.01f, -5.0f, 5.0f);
            ImGui::DragFloat("Height Offset", &preset_.heightOffset, 0.01f, -5.0f, 5.0f);
            ImGui::DragFloat("LookAt Height Offset", &preset_.lookAtHeightOffset, 0.01f, -5.0f, 5.0f);
            ImGui::DragFloat("Focus Target Rate Offset", &preset_.focusTargetRateOffset, 0.01f, -1.0f, 1.0f);
            ImGui::DragFloat("FOV Offset", &preset_.fovOffset, 0.05f, -30.0f, 30.0f);
            ImGui::DragFloat("Blend In", &preset_.blendInDuration, 0.01f, 0.0f, 5.0f);
            ImGui::DragFloat("Hold", &preset_.holdDuration, 0.01f, 0.0f, 5.0f);
            ImGui::DragFloat("Blend Out", &preset_.blendOutDuration, 0.01f, 0.0f, 5.0f);
            ImGui::DragInt("Priority", &preset_.priority);
            int wallPolicy_ = static_cast<int>(preset_.wallPolicy);
            if (ImGui::Combo(
                "Wall Policy",
                &wallPolicy_,
                "ALLOW\0REDUCE\0TANGENT_ONLY\0DISABLE\0"))
            {
                preset_.wallPolicy =
                    static_cast<CAMERA_COMPOSITION_WALL_POLICY>(wallPolicy_);
            }
            ImGui::DragFloat("Wall Suppression", &preset_.wallSuppressionStrength, 0.01f, 0.0f, 1.0f);
            ImGui::Checkbox("Continue Until Stop", &preset_.continueUntilStop);
            ImGui::Checkbox("Use Attack Direction Side", &preset_.useAttackDirectionSide);

            if (ImGui::Button("Play Preview"))
            {
                PlayPresetByName(preset_.presetName);
            }
            ImGui::SameLine();
            if (ImGui::Button("Delete") && presets.size() > 1)
            {
                const std::string playActionName_ = GetPlayActionName(preset_.presetName);
                const std::string sourceActionName_ =
                    "CameraComposition.Play." +
                    std::string(ToString(preset_.source)) +
                    "." +
                    preset_.presetName;
                const std::string shortActionName_ =
                    "CameraComposition_" + preset_.presetName;
                presets.erase(presets.begin() + index_);
                ActionRegistry::Instance().RemoveAction("CameraComposition", playActionName_);
                ActionRegistry::Instance().RemoveAction("CameraComposition", sourceActionName_);
                ActionRegistry::Instance().RemoveAction("CameraComposition", shortActionName_);
                ImGui::TreePop();
                ImGui::PopID();
                break;
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
}

void CombatCameraCompositionComponent::Serialize(nlohmann::json& json_) const
{
    json_["targetCameraObjectName"] = targetCameraObjectName;
    json_["blackboardCategory"] = blackboardCategory;
    json_["playTriggerKey"] = playTriggerKey;
    json_["stopTriggerKey"] = stopTriggerKey;
    json_["clearTriggerKey"] = clearTriggerKey;
    json_["presetNameKey"] = presetNameKey;
    json_["sourceKey"] = sourceKey;
    json_["requesterKey"] = requesterKey;
    json_["attackDirectionKey"] = attackDirectionKey;
    json_["maximumSideOffset"] = maximumSideOffset;
    json_["minimumDistanceOffset"] = minimumDistanceOffset;
    json_["maximumDistanceOffset"] = maximumDistanceOffset;
    json_["minimumHeightOffset"] = minimumHeightOffset;
    json_["maximumHeightOffset"] = maximumHeightOffset;
    json_["minimumFovOffset"] = minimumFovOffset;
    json_["maximumFovOffset"] = maximumFovOffset;
    json_["minimumFocusTargetRateOffset"] = minimumFocusTargetRateOffset;
    json_["maximumFocusTargetRateOffset"] = maximumFocusTargetRateOffset;
    json_["enableCloseRangePlayerAssist"] = enableCloseRangePlayerAssist;
    json_["closeRangeAssistMinimumDistance"] =
        closeRangeAssistMinimumDistance;
    json_["closeRangeAssistMaximumDistance"] =
        closeRangeAssistMaximumDistance;
    json_["closeRangeAssistBlendDistance"] = closeRangeAssistBlendDistance;
    json_["closeRangeAssistDistanceOffset"] =
        closeRangeAssistDistanceOffset;
    json_["closeRangeAssistMaximumDistanceOffset"] =
        closeRangeAssistMaximumDistanceOffset;
    json_["closeRangeAssistHeightOffset"] = closeRangeAssistHeightOffset;
    json_["presets"] = nlohmann::json::array();

    for (const CameraCompositionPreset& preset_ : presets)
    {
        nlohmann::json presetJson_;
        presetJson_["presetName"] = preset_.presetName;
        presetJson_["source"] = ToString(preset_.source);
        presetJson_["sideOffset"] = preset_.sideOffset;
        presetJson_["distanceOffset"] = preset_.distanceOffset;
        presetJson_["heightOffset"] = preset_.heightOffset;
        presetJson_["lookAtHeightOffset"] = preset_.lookAtHeightOffset;
        presetJson_["focusTargetRateOffset"] = preset_.focusTargetRateOffset;
        presetJson_["fovOffset"] = preset_.fovOffset;
        presetJson_["blendInDuration"] = preset_.blendInDuration;
        presetJson_["holdDuration"] = preset_.holdDuration;
        presetJson_["blendOutDuration"] = preset_.blendOutDuration;
        presetJson_["priority"] = preset_.priority;
        presetJson_["wallPolicy"] = ToString(preset_.wallPolicy);
        presetJson_["wallSuppressionStrength"] =
            preset_.wallSuppressionStrength;
        presetJson_["useUnscaledTime"] = preset_.useUnscaledTime;
        presetJson_["continueUntilStop"] = preset_.continueUntilStop;
        presetJson_["useAttackDirectionSide"] =
            preset_.useAttackDirectionSide;
        json_["presets"].push_back(presetJson_);
    }
}

void CombatCameraCompositionComponent::Deserialize(const nlohmann::json& json_)
{
    targetCameraObjectName =
        json_.value("targetCameraObjectName", std::string{});
    blackboardCategory =
        json_.value("blackboardCategory", std::string{ "CameraComposition" });
    playTriggerKey = json_.value("playTriggerKey", std::string{ "Play" });
    stopTriggerKey = json_.value("stopTriggerKey", std::string{ "Stop" });
    clearTriggerKey = json_.value("clearTriggerKey", std::string{ "Clear" });
    presetNameKey =
        json_.value("presetNameKey", std::string{ "PresetName" });
    sourceKey = json_.value("sourceKey", std::string{ "Source" });
    requesterKey = json_.value("requesterKey", std::string{ "Requester" });
    attackDirectionKey =
        json_.value("attackDirectionKey", std::string{ "AttackDirection" });
    maximumSideOffset = json_.value("maximumSideOffset", 0.8f);
    minimumDistanceOffset = json_.value("minimumDistanceOffset", -1.0f);
    maximumDistanceOffset = json_.value("maximumDistanceOffset", 1.5f);
    minimumHeightOffset = json_.value("minimumHeightOffset", -0.5f);
    maximumHeightOffset = json_.value("maximumHeightOffset", 0.7f);
    minimumFovOffset = json_.value("minimumFovOffset", -5.0f);
    maximumFovOffset = json_.value("maximumFovOffset", 6.0f);
    minimumFocusTargetRateOffset =
        json_.value("minimumFocusTargetRateOffset", -0.2f);
    maximumFocusTargetRateOffset =
        json_.value("maximumFocusTargetRateOffset", 0.25f);
    enableCloseRangePlayerAssist =
        json_.value("enableCloseRangePlayerAssist", true);
    closeRangeAssistMinimumDistance =
        json_.value("closeRangeAssistMinimumDistance", 0.0f);
    closeRangeAssistMaximumDistance =
        json_.value("closeRangeAssistMaximumDistance", 6.5f);
    closeRangeAssistBlendDistance =
        json_.value("closeRangeAssistBlendDistance", 1.0f);
    closeRangeAssistDistanceOffset =
        json_.value("closeRangeAssistDistanceOffset", 0.45f);
    closeRangeAssistMaximumDistanceOffset =
        json_.value("closeRangeAssistMaximumDistanceOffset", 10.0f);
    closeRangeAssistHeightOffset =
        json_.value("closeRangeAssistHeightOffset", -2.0f);

    presets.clear();
    if (json_.contains("presets") && json_["presets"].is_array())
    {
        for (const nlohmann::json& presetJson_ : json_["presets"])
        {
            CameraCompositionPreset preset_;
            preset_.presetName =
                presetJson_.value("presetName", std::string{ "Composition" });
            preset_.source = SourceFromString(
                presetJson_.value("source", std::string{ "PLAYER" }));
            preset_.sideOffset = presetJson_.value("sideOffset", 0.0f);
            preset_.distanceOffset =
                presetJson_.value("distanceOffset", 0.0f);
            preset_.heightOffset = presetJson_.value("heightOffset", 0.0f);
            preset_.lookAtHeightOffset =
                presetJson_.value("lookAtHeightOffset", 0.0f);
            preset_.focusTargetRateOffset =
                presetJson_.value("focusTargetRateOffset", 0.0f);
            preset_.fovOffset = presetJson_.value("fovOffset", 0.0f);
            preset_.blendInDuration =
                presetJson_.value("blendInDuration", 0.1f);
            preset_.holdDuration =
                presetJson_.value("holdDuration", 0.0f);
            preset_.blendOutDuration =
                presetJson_.value("blendOutDuration", 0.2f);
            preset_.priority = presetJson_.value("priority", 0);
            preset_.wallPolicy = WallPolicyFromString(
                presetJson_.value("wallPolicy", std::string{ "REDUCE" }));
            preset_.wallSuppressionStrength =
                presetJson_.value("wallSuppressionStrength", 0.8f);
            preset_.useUnscaledTime =
                presetJson_.value("useUnscaledTime", false);
            preset_.continueUntilStop =
                presetJson_.value("continueUntilStop", false);
            preset_.useAttackDirectionSide =
                presetJson_.value("useAttackDirectionSide", true);
            presets.push_back(preset_);
        }
    }
}

REGISTER_COMPONENT(
    CombatCameraCompositionComponent,
    "CombatCameraCompositionComponent")

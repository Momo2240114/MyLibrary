#include "PrecompiledHeader.h"
#include "HumanBossAIComponent.h"
#include "HumanBossActionComponent.h"
#include "Engine/Audio/AudioManager.h"
#include "Engine/Common/DebugLog.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Combat/HealthComponent.h"
#include "Engine/GamePlay/System/EffectManager.h"
#include "Engine/Graphics/Resource/Animation/AnimationControllerComponent.h"
#include "Engine/Graphics/Resource/Animation/AnimationGraphBridgeComponent.h"
#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"
#include "Engine/Graphics/Particle/ParticleComponent.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/GamePlay/System/TimeManager.h"
#include "Engine/GamePlay/Camera/CameraManager.h"
#include "Engine/GamePlay/Camera/CameraComponent.h"
#include "Engine/GamePlay/Camera/CameraShakeComponent.h"
#include "Engine/GamePlay/Camera/CameraActionDirectorComponent.h"
#include "Game/GameMode/GameModeManager.h"
#include "Engine/GamePlay/Transform/ModelAttachmentComponent.h"
#include "Engine/Graphics/Resource/GlTFModel.h"
#include <limits>
#include <cmath>

namespace
{
    // 武器Trailと子HitBoxも本体の時計へ合わせ、姿勢だけ高速になるずれを防ぐ。
    void SetHierarchyPlaybackRate(const std::shared_ptr<GameObject>& object_, float rate_)
    {
        if (!object_) return;
        object_->SetRuntimePlaybackRate(rate_);
        for (const auto& child_ : object_->transform->children)
            if (child_) SetHierarchyPlaybackRate(child_->gameObject.lock(),rate_);
    }
}

void HumanBossAIComponent::BeginSpecialIntro()
{
    RestoreSpecialIntro();
    const auto owner_ = GetGameObject();
    if (!owner_ || !owner_->manager || !owner_->transform) return;
    const auto mode_ = owner_->GetManager().GetGameModeManager();
    const auto cameras_ = owner_->GetManager().GetCameraManager();
    const auto source_ = cameras_ ? cameras_->GetSourceCamera() : nullptr;
    const auto cameraObject_ = source_ ? source_->GetGameObject() : nullptr;
    const auto director_ = cameraObject_ ? cameraObject_->GetComponent<CameraActionDirectorComponent>() : nullptr;
    if (!mode_ || !director_) return;
    const auto player_ = target.lock();
    const auto position_ = owner_->transform->GetWorldPosition();
    const auto playerPosition_ = player_ && player_->transform ? player_->transform->GetWorldPosition() : position_;
    const float dx_ = playerPosition_.x - position_.x;
    const float dz_ = playerPosition_.z - position_.z;
    const float length_ = std::hypot(dx_, dz_);
    CameraActionPreset preset_;
    preset_.actionName = "HumanBossSpecialIntro";
    preset_.targetMode = CAMERA_ACTION_TARGET_MODE::GAME_OBJECT_OFFSET;
    preset_.positionMode = CAMERA_ACTION_POSITION_MODE::WORLD_POSITION;
    preset_.targetObjectName = owner_->GetName();
    preset_.useModelBounds = false;
    preset_.autoFrameTargets = true;
    preset_.framingRadius = 1.4f;
    preset_.minimumFramingDistance = 4.0f;
    preset_.cameraOffset = length_ > 0.001f ? DirectX::XMFLOAT3{ dx_ / length_, 0.1f, dz_ / length_ }
        : DirectX::XMFLOAT3{ 0.0f, 0.1f, 1.0f };
    preset_.focusOffset = { 0.0f, 1.5f, 0.0f };
    preset_.targetFov = 42.0f;
    preset_.enterDuration = 0.5f;
    preset_.holdDuration = 0.6f;
    preset_.returnDuration = 0.35f;
    preset_.priority = 24;
    preset_.suppressCameraCollision = true;
    if (!director_->PlayPresetForTarget(preset_, owner_)) return;
    specialIntroCamera = director_;
    specialIntroRemaining = 1.45f;
    specialIntroCameraReturning = false;
    specialIntroPreviousPlayerLock = mode_->IsPlayerControlLocked();
    mode_->SetPlayerControlLocked(true);
    specialIntroLocked = true;
}

void HumanBossAIComponent::UpdateSpecialIntro()
{
    if (!specialIntroLocked) return;
    const auto owner_ = GetGameObject();
    const auto health_ = owner_ ? owner_->GetComponent<HealthComponent>() : nullptr;
    const auto time_ = owner_ && owner_->manager ? owner_->GetManager().GetTimeManager() : nullptr;
    if (!owner_ || !owner_->manager || owner_->IsPendingDestroy() || (health_ && health_->IsDead()))
    { RestoreSpecialIntro(); return; }
    specialIntroRemaining -= time_ ? time_->GetLastRawDeltaTime() : 1.0f / 60.0f;
    if (!specialIntroCameraReturning && specialIntroRemaining <= 0.35f)
    {
        if (const auto camera_ = specialIntroCamera.lock()) camera_->StopActionByName("HumanBossSpecialIntro", false);
        specialIntroCameraReturning = true;
    }
    if (specialIntroRemaining <= 0.0f) RestoreSpecialIntro();
}

void HumanBossAIComponent::RestoreSpecialIntro()
{
    if (!specialIntroLocked) return;
    if (const auto camera_ = specialIntroCamera.lock())
        camera_->StopActionByName("HumanBossSpecialIntro", !specialIntroCameraReturning);
    const auto owner_ = GetGameObject();
    if (owner_ && owner_->manager)
        if (const auto mode_ = owner_->GetManager().GetGameModeManager(); mode_ && !mode_->IsPresentationPlaying())
            mode_->SetPlayerControlLocked(specialIntroPreviousPlayerLock);
    specialIntroCamera.reset();
    specialIntroLocked = false;
    specialIntroRemaining = 0.0f;
}

void HumanBossAIComponent::StartBloodEye()
{
    const auto owner_ = GetGameObject();
    if (!owner_ || !owner_->manager || !owner_->transform) return;
    Stop();
    bloodEyeUsed = true;
    bloodEyeRemaining = bloodEyeDuration;
    BeginSpecialIntro();
    if (const auto animation_ = owner_->GetComponent<AnimationControllerComponent>())
        animation_->ChangeState("SS_Guard", 0.06f, true, 0.0f);
    const auto model_ = owner_->GetComponent<GltfModelCommponennt>();
    if (!model_) return;
    for (int index_ = 0; index_ < 2; ++index_)
    {
        ParticleSpawnRequest request_;
        request_.assetPath = "Data/Particle/HumanBossBloodEye.json";
        request_.position = owner_->transform->GetWorldPosition();
        request_.lifeTime = bloodEyeDuration + 1.0f;
        const auto effect_ = owner_->GetManager().GetEffectManager().SpawnParticle(request_);
        bloodEyeEffects[index_] = effect_;
        if (!effect_ || !effect_->transform) continue;
        effect_->transform->localPosition = { index_ == 0 ? -0.12f : 0.12f, 0.08f, 0.18f };
        effect_->transform->MarkDirty();
        const auto attachment_ = effect_->GetOrAddComponent<ModelAttachmentComponent>();
        attachment_->SetTargetNode(model_, "head");
        attachment_->SyncAttachment();
    }
}

void HumanBossAIComponent::StopBloodEye(bool immediate_)
{
    bloodEyeRemaining = 0.0f;
    const auto owner_ = GetGameObject();
    if (owner_ && owner_->manager)
        for (auto& weak_ : bloodEyeEffects)
            if (const auto effect_ = weak_.lock())
            {
                if (immediate_) owner_->GetManager().GetEffectManager().StopEffect(effect_);
                else if (const auto particle_ = effect_->GetComponent<ParticleComponent>()) particle_->SetEmissionScale(0.0f);
            }
    bloodEyeEffects = {};
}

void HumanBossAIComponent::BeginBerserkEntrance()
{
    BS_PROFILE_SCOPE("HumanBossAIComponent::BeginBerserkEntrance");
    const auto owner_ = GetGameObject();
    if (!owner_ || !owner_->manager) return;
    BeginSpecialIntro();
    // 暴走の開始通知だけで唸りと低音を重ね、毎フレームのオーラ更新では再生しない。
    if (owner_->transform)
    {
        const auto position_ = owner_->transform->GetWorldPosition();
        AudioManager::Instance().PlaySpatialAttackSound("Data/Audio/SE/Sonniss/Boss/MonsterGrowl.wav",
            position_, 0.5f, 8.0f, 100.0f);
        AudioManager::Instance().PlaySpatialAttackSound("Data/Audio/SE/Sonniss/Presentation/RattlingDowner.wav",
            position_, 0.3f, 8.0f, 100.0f);
    }
    berserkEntranceRemaining = berserkEntranceDuration;
    berserkPreviousBlur = GraphicsDeviceManager::Instance().GetRadialBlurParams();
    berserkPreviousFilter = GraphicsDeviceManager::Instance().GetFilterParams();
    berserkBlurOwned = true;
    // 開始直後は攻撃を出さず、短い振動と放射ブラーで防御への切替を知らせる。
    if (const auto cameras_ = owner_->GetManager().GetCameraManager())
        // 描画用ProxyにはGameObjectが無いため、選択元の実カメラへShakeを付ける。
        if (const auto camera_ = cameras_->GetSourceCamera())
            if (const auto cameraObject_ = camera_->GetGameObject())
                cameraObject_->GetOrAddComponent<CameraShakeComponent>()->PlayShake(berserkEntranceDuration,0.045f,0.4f,22.0f,15);
    ParticleSpawnRequest request_;
    request_.assetPath = "Data/Particle/HumanBossBerserkEntrance.json";
    request_.position = owner_->transform->GetWorldPosition(); request_.position.y += 1.4f;
    request_.lifeTime = berserkEntranceDuration + 0.5f;
    berserkEntranceEffect = owner_->GetManager().GetEffectManager().SpawnParticle(request_);
    if (const auto effect_ = berserkEntranceEffect.lock())
    {
        if (const auto particle_ = effect_->GetComponent<ParticleComponent>()) particle_->Update(1.0f/60.0f);
    }
    else BS_LOG_INFO(LOG_CATEGORY::GAME,"Berserk entrance particle unavailable: " + request_.assetPath);
}

bool HumanBossAIComponent::UpdateBerserkEntrance()
{
    BS_PROFILE_SCOPE("HumanBossAIComponent::UpdateBerserkEntrance");
    if (!berserkBlurOwned) return false;
    const auto owner_ = GetGameObject();
    const auto time_ = owner_ && owner_->manager ? owner_->GetManager().GetTimeManager() : nullptr;
    const float delta_ = time_ ? time_->GetLastRawDeltaTime() : 1.0f/60.0f;
    berserkEntranceRemaining = (std::max)(0.0f,berserkEntranceRemaining - delta_);
    const float progress_ = 1.0f - berserkEntranceRemaining / (std::max)(berserkEntranceDuration,0.01f);
    const int stage_ = std::clamp(berserkStage,0,2);
    const float envelope_ = (std::max)(0.0f,std::sin(progress_ * DirectX::XM_PI));
    auto blur_ = berserkPreviousBlur;
    // 25％・10％・5％の順に強める。どの段階も開始と終了は元の設定へ滑らかにつなぐ。
    blur_.blurRadius += berserkEntranceBlur * berserkEntranceBlurMultipliers[stage_] * envelope_;
    blur_.blurCenter = {0.5f,0.45f};
    GraphicsDeviceManager::Instance().GetRadialBlurParams() = blur_;
    const float monochrome_ = std::clamp(berserkEntranceMonochrome[stage_] * envelope_,0.0f,1.0f);
    auto filter_ = berserkPreviousFilter;
    filter_.saturation *= 1.0f - monochrome_;
    filter_.hueShift *= 1.0f - monochrome_;
    // 彩度だけ落としても色バランスで着色されるため、最終段階ではRGBを中立へそろえる。
    filter_.colorBalance = {
        filter_.colorBalance.x + (1.0f-filter_.colorBalance.x)*monochrome_,
        filter_.colorBalance.y + (1.0f-filter_.colorBalance.y)*monochrome_,
        filter_.colorBalance.z + (1.0f-filter_.colorBalance.z)*monochrome_};
    filter_.colorOffset.x *= 1.0f - monochrome_;
    filter_.colorOffset.y *= 1.0f - monochrome_;
    filter_.colorOffset.z *= 1.0f - monochrome_;
    filter_.contrast += ((std::max)(filter_.contrast,1.1f)-filter_.contrast)*monochrome_;
    filter_.filterIntensity += (1.0f-filter_.filterIntensity)*envelope_;
    GraphicsDeviceManager::Instance().GetFilterParams() = filter_;
    if (berserkEntranceRemaining <= 0.0f)
    {
        RestoreBerserkEntrance();
        if (owner_ && berserkStage >= 0) SetHierarchyPlaybackRate(owner_,berserkSpeeds[berserkStage]);
        BS_LOG_INFO(LOG_CATEGORY::GAME,"HumanBoss berserk entrance completed: stage=" + std::to_string(berserkStage));
    }
    return true;
}

void HumanBossAIComponent::RestoreBerserkEntrance()
{
    BS_PROFILE_SCOPE("HumanBossAIComponent::RestoreBerserkEntrance");
    // 中断・リトライでも暴走前のPostEffectへ戻し、次の戦闘にブラーを持ち越さない。
    if (berserkBlurOwned)
    {
        GraphicsDeviceManager::Instance().GetRadialBlurParams() = berserkPreviousBlur;
        GraphicsDeviceManager::Instance().GetFilterParams() = berserkPreviousFilter;
    }
    berserkBlurOwned = false; berserkEntranceRemaining = 0.0f;
    const auto owner_ = GetGameObject();
    if (const auto effect_ = berserkEntranceEffect.lock(); effect_ && owner_ && owner_->manager)
        owner_->GetManager().GetEffectManager().StopEffect(effect_);
    berserkEntranceEffect.reset();
}

void HumanBossAIComponent::UpdateBerserkDown(float deltaTime_)
{
    BS_PROFILE_SCOPE("HumanBossAIComponent::UpdateBerserkDown");
    const auto owner_ = GetGameObject();
    const auto animation_ = owner_ ? owner_->GetComponent<AnimationControllerComponent>() : nullptr;
    if (!animation_ || berserkDownLoop) return;
    // Startは実クリップ長まで再生する。最終スタンでもこの遷移を進めてから有効打を待つ。
    const float duration_ = animation_->GetCurrentSampleDuration();
    // ポーズの進行と同じ時計を使い、HitStop中にStartの未再生区間を飛ばさない。
    berserkDownElapsed += (std::max)(deltaTime_,0.0f);
    if (duration_ > 0.0f && berserkDownElapsed < duration_) return;
    if (duration_ <= 0.0f && berserkDownElapsed < 1.0f) return;
    animation_->ChangeState("HumanBossBerserkDownLoop",0.08f,true,0.0f);
    berserkDownLoop = true;
}

void HumanBossAIComponent::StartBerserkAura()
{
    BS_PROFILE_SCOPE("HumanBossAIComponent::StartBerserkAura");
    StopBerserkAura();
    const auto owner_ = GetGameObject();
    if (!owner_ || !owner_->manager) return;
    ParticleSpawnRequest request_;
    request_.assetPath = "Data/Particle/HumanBossBerserkAura.json";
    request_.position = owner_->transform->GetWorldPosition();
    request_.position.y += 1.4f;
    request_.lifeTime = 3600.0f;
    // 攻撃所有Effectへ登録すると各連撃のStopで消えるため、AIが状態終了まで所有する。
    berserkAuraEffect = owner_->GetManager().GetEffectManager().SpawnParticle(request_);
    if (const auto effect_ = berserkAuraEffect.lock())
    {
        if (const auto particle_ = effect_->GetComponent<ParticleComponent>())
        {
            particle_->SetEmissionScale(1.0f);
            // 連続Emitterの初期蓄積を進め、暴走開始フレームから体を包む粒子を用意する。
            for (int step_ = 0; step_ < 4; ++step_) particle_->Update(1.0f/60.0f);
        }
    }
    else BS_LOG_INFO(LOG_CATEGORY::GAME, "Berserk aura unavailable: " + request_.assetPath);
}

void HumanBossAIComponent::StopBerserkAura()
{
    BS_PROFILE_SCOPE("HumanBossAIComponent::StopBerserkAura");
    const auto owner_ = GetGameObject();
    if (const auto effect_ = berserkAuraEffect.lock(); effect_ && owner_ && owner_->manager)
        owner_->GetManager().GetEffectManager().StopEffect(effect_);
    berserkAuraEffect.reset();
}

void HumanBossAIComponent::LateUpdate(float deltaTime_)
{
    BS_PROFILE_SCOPE("HumanBossAIComponent::LateUpdate");
    (void)deltaTime_;
    UpdateSpecialIntro();
    // 観察対象を一時的に見失ってAIのUpdateが早期終了しても、ブラーの復元時計は止めない。
    UpdateBerserkEntrance();
    const auto owner_ = GetGameObject();
    const auto effect_ = berserkAuraEffect.lock();
    if (!owner_ || !effect_ || !IsBerserkInvulnerable()) return;
    // 独立したプールObjectの発生源だけを追従させ、放出済みの煙は短い残像として残す。
    auto position_ = owner_->transform->GetWorldPosition();
    position_.y += 1.4f;
    // プールから生成したObjectは親を持たないため、localPositionがワールド位置に一致する。
    effect_->transform->localPosition = position_;
    effect_->transform->MarkDirty();
    effect_->UpdateTransform();
}

void HumanBossAIComponent::OnDestroy()
{
    BS_PROFILE_SCOPE("HumanBossAIComponent::OnDestroy");
    ResetBerserk();
}

void HumanBossAIComponent::ResetBerserk()
{
    BS_PROFILE_SCOPE("HumanBossAIComponent::ResetBerserk");
    StopBloodEye();
    RestoreSpecialIntro();
    StopBerserkAura();
    RestoreBerserkEntrance();
    const auto owner_ = GetGameObject();
    if (const auto health_ = owner_ ? owner_->GetComponent<HealthComponent>() : nullptr;
        health_ && health_->GetMaxHealth() > 0.0f &&
        health_->GetCurrentHealth() / health_->GetMaxHealth() > bloodEyeHpThreshold)
        bloodEyeUsed = false;
    if (const auto effect_ = berserkStunEffect.lock(); effect_ && owner_ && owner_->manager)
        owner_->GetManager().GetEffectManager().StopEffect(effect_);
    berserkStunEffect.reset();
    if (owner_)
    {
        SetHierarchyPlaybackRate(owner_,1.0f);
        if (berserkStunned) if (const auto animation_ = owner_->GetComponent<AnimationControllerComponent>())
        {
            animation_->SetAnimationPaused(false);
            animation_->SetPresentationControlled(false);
        }
    }
    if (const auto executor_ = executor.lock()) executor_->SetBerserkProtection(false);
    berserkPending.fill(false); berserkUsed.fill(false);
    berserkStage = -1; berserkStartedCount = 0;
    berserkAttackInFlight = false; berserkStunned = false; finalPhaseReserved = false; finishRequested = false;
    berserkTimer = 0.0f; berserkFailureTime = 0.0f;
    berserkStartReason = "Waiting for HP threshold";
}

float HumanBossAIComponent::ConstrainBattleDamage(float damage_)
{
    BS_PROFILE_SCOPE("HumanBossAIComponent::ConstrainBattleDamage");
    const auto owner_ = GetGameObject();
    const auto health_ = owner_ ? owner_->GetComponent<HealthComponent>() : nullptr;
    if (!health_ || health_->GetMaxHealth() <= 0.0f || !CanRunBerserkBattle()) return damage_;
    if (finishRequested) return 0.0f;
    // HitBoxを経由しない直接ダメージも遮断する。0の呼び出しでは実HPからの閾値確認を維持する。
    if (IsBerserkInvulnerable() && damage_ > 0.0f) return 0.0f;
    constexpr float THRESHOLDS[] = {0.25f,0.10f,0.05f};
    const float rate_ = (health_->GetCurrentHealth()-(std::max)(damage_,0.0f)) / health_->GetMaxHealth();
    // 大ダメージでも通過した閾値を記録する。5％到達時は未実行の下位予約を統合する。
    for (int index_ = 0; index_ < 3; ++index_)
        if (rate_ <= THRESHOLDS[index_] && !berserkUsed[index_]) berserkPending[index_] = true;
    if (berserkPending[2] || finalPhaseReserved)
    {
        finalPhaseReserved = true;
        berserkPending[0] = false; berserkPending[1] = false;
        const float floor_ = (std::min)(health_->GetCurrentHealth(), (std::max)(0.01f, health_->GetMaxHealth()*0.01f));
        // HP制御は最終フェーズに限定し、回復や通常のダメージ倍率を変更しない。
        return (std::min)((std::max)(damage_,0.0f), (std::max)(health_->GetCurrentHealth()-floor_,0.0f));
    }
    return damage_;
}

bool HumanBossAIComponent::TryBeginFinalFinish(const std::shared_ptr<GameObject>& attacker_)
{
    BS_PROFILE_SCOPE("HumanBossAIComponent::TryBeginFinalFinish");
    const auto owner_ = GetGameObject();
    if (!owner_ || !owner_->manager || !attacker_ || berserkStage != 2 || !berserkStunned || finishRequested) return false;
    if ((attacker_->GetLayerBit() & static_cast<uint32_t>(LayerType::Player)) == 0) return false;
    const auto mode_ = owner_->GetManager().GetGameModeManager();
    if (!mode_ || mode_->GetCurrentGameState() != GameProgressState::PHASE_2_BATTLE) return false;
    // 最初の有効打で先にラッチを立て、多段攻撃からの再入を拒否してから予約を閉じる。
    finishRequested = true;
    StopBerserkAura();
    if (const auto executor_ = executor.lock()) { executor_->Stop(); executor_->SetBerserkProtection(false); }
    owner_->GetManager().GetEffectManager().StopAttackEffects(owner_);
    berserkAttackInFlight = false; berserkPending.fill(false);
    mode_->NotifyPhase2BossDefeated();
    return true;
}

bool HumanBossAIComponent::CanRunBerserkBattle()
{
    const auto owner_ = GetGameObject();
    const auto mode_ = owner_ && owner_->manager ? owner_->GetManager().GetGameModeManager() : nullptr;
    // 単体戦闘とEditor Playでは、通常AIが動いていても進行状態が第二形態とは限らない。
    // シーン進行のEnumではなく実際の戦闘ロックで判定し、導入・終了演出への割り込みは拒否する。
    return owner_ && owner_->IsCombatReady() && mode_ && !mode_->IsEnemyAILocked() &&
        !mode_->IsPresentationPlaying() && !mode_->IsGameplayLocked() && !mode_->IsBattlePaused() &&
        mode_->GetCurrentGameState() != GameProgressState::BOSS_DEFEATED &&
        mode_->GetCurrentGameState() != GameProgressState::RESULT;
}

bool HumanBossAIComponent::TryStartBerserk()
{
    BS_PROFILE_SCOPE("HumanBossAIComponent::TryStartBerserk");
    if (berserkStage >= 0 || finishRequested) return false;
    if (comboBindTimer > 0.0f || playerCounterProtectionTimer > 0.0f || reflectedOrbTimer > 0.0f || specialIntroLocked) return false;
    const auto owner_ = GetGameObject();
    const auto executor_ = executor.lock();
    if (!owner_ || !executor_) { berserkStartReason = "Owner or action executor missing"; return false; }
    if (!CanRunBerserkBattle()) { berserkStartReason = "Combat not ready or game mode locked"; return false; }
    const auto health_ = owner_->GetComponent<HealthComponent>();
    if (!health_ || health_->IsDead()) { berserkStartReason = "Health missing or boss dead"; return false; }
    ConstrainBattleDamage(0.0f);
    int next_ = -1;
    for (int index_ = 0; index_ < 3; ++index_)
        if (berserkPending[index_] && !berserkUsed[index_]) { next_ = index_; break; }
    if (berserkPending[2]) next_ = 2;
    if (next_ < 0) { berserkStartReason = "Waiting for an unused HP threshold (25 / 10 / 5%)"; return false; }
    // IsActiveの終了待ちは行わない。通常Skillが長いコンボや通知待ちでも確実に開始できるよう、
    // 既存Stop経路で予約・判定・JG硬直を閉じてから暴走状態を確定する。
    Stop();
    StopBloodEye();
    hitTimer = 0.0f;
    berserkStage = next_; berserkUsed[next_] = true; berserkPending[next_] = false;
    berserkStartedCount = 0; berserkTimer = 0.0f; berserkFailureTime = 0.0f;
    berserkStunned = false; berserkAttackInFlight = false;
    berserkStartReason = "Berserk entrance started";
    executor_->SetBerserkProtection(true);
    SetHierarchyPlaybackRate(owner_,1.0f);
    // ラッチを立てた後で演出を呼び、同一フレームの多段通知から二重に開始しない。
    BeginBerserkEntrance();
    StartBerserkAura();
    BS_LOG_INFO(LOG_CATEGORY::GAME,"HumanBoss berserk entrance: stage=" + std::to_string(next_) +
        " hp=" + std::to_string(health_->GetCurrentHealth()) + "/" + std::to_string(health_->GetMaxHealth()));
    return true;
}

bool HumanBossAIComponent::UpdateBerserk(float deltaTime_)
{
    BS_PROFILE_SCOPE("HumanBossAIComponent::UpdateBerserk");
    const auto owner_ = GetGameObject();
    const auto executor_ = executor.lock();
    if (!owner_ || !executor_) return false;
    // GUIや旧データから直接HPが変わった場合も、同じ予約経路へ収束させる。
    ConstrainBattleDamage(0.0f);
    if (berserkStage < 0) return false;
    if (finishRequested) return true;
    if (const auto blackboard_ = owner_->GetComponent<BlackboardComponent>())
    {
        blackboard_->SetInt("HumanBossAI", "BerserkStage", berserkStage);
        blackboard_->SetInt("HumanBossAI", "BerserkActionsStarted", berserkStartedCount);
        blackboard_->SetBool("HumanBossAI", "BerserkStun", berserkStunned);
        blackboard_->SetBool("HumanBossAI", "FinalStun", berserkStunned && berserkStage == 2);
    }
    if (berserkStunned)
    {
        UpdateBerserkDown(deltaTime_);
        // 最終スタンには自動復帰を設けず、AttackSystemで検証されたPlayerの有効打だけを待つ。
        if (berserkStage == 2) return true;
        berserkTimer -= deltaTime_;
        if (berserkTimer > 0.0f) return true;
        const auto pending_ = berserkPending, used_ = berserkUsed;
        const bool final_ = finalPhaseReserved;
        ResetBerserk();
        berserkPending = pending_; berserkUsed = used_; finalPhaseReserved = final_;
        Stop();
        return true;
    }
    if (berserkBlurOwned || specialIntroLocked) return true;
    executor_->UpdateHitProtection(deltaTime_);
    if (berserkAttackInFlight)
    {
        executor_->Tick(deltaTime_);
        if (!executor_->IsActive()) FinishBerserkAttack();
        return true;
    }
    berserkTimer = (std::max)(0.0f, berserkTimer-deltaTime_);
    if (berserkTimer > 0.0f) return true;
    if (berserkStartedCount >= berserkAttackCounts[berserkStage]) { EnterBerserkStun(); return true; }
    if (BeginBerserkAttack()) { berserkFailureTime = 0.0f; return true; }
    // 到達可能性の失敗は回数へ加算せず、既存の歩行と経路検証で間合いを詰めて再試行する。
    berserkFailureTime += deltaTime_;
    executor_->SetLocomotion(walkState, {toPlayer.x*walkSpeed,0,toPlayer.z*walkSpeed}, toPlayer, {0,1});
    if (berserkFailureTime > 8.0f)
    {
        BS_LOG_INFO(LOG_CATEGORY::GAME, "Berserk attacks unavailable: " + executor_->GetValidationError());
        bool hasAttackAsset_ = false;
        for (const auto type_ : {HumanBossActionType::NORMAL_ATTACK_A,HumanBossActionType::NORMAL_ATTACK_B,HumanBossActionType::SHADOW_SLASH})
        {
            std::string error_;
            hasAttackAsset_ = hasAttackAsset_ || SkillExecution::CanExecute(owner_,actions[static_cast<std::size_t>(type_)].skill,error_);
        }
        // 間合いや壁による開始失敗を「実行済み」にしない。全素材欠損時だけ停止を救済する。
        if (!hasAttackAsset_) EnterBerserkStun();
        berserkFailureTime = 0.0f;
    }
    return true;
}

bool HumanBossAIComponent::BeginBerserkAttack()
{
    BS_PROFILE_SCOPE("HumanBossAIComponent::BeginBerserkAttack");
    const auto executor_ = executor.lock();
    if (!executor_) return false;
    constexpr HumanBossActionType PATTERN[] = {HumanBossActionType::SHADOW_SLASH,HumanBossActionType::SHADOW_CHARGE,
        HumanBossActionType::SHADOW_DIVE_01,HumanBossActionType::NORMAL_ATTACK_A,HumanBossActionType::SHADOW_DOMAIN,
        HumanBossActionType::NORMAL_ATTACK_B,HumanBossActionType::SHADOW_DIVE_02,HumanBossActionType::HEAVY_SINGLE_ATTACK};
    for (std::size_t offset_ = 0; offset_ < std::size(PATTERN); ++offset_)
    {
        const auto type_ = PATTERN[(static_cast<std::size_t>(berserkStartedCount)+offset_)%std::size(PATTERN)];
        const auto index_ = static_cast<std::size_t>(type_);
        if (index_ >= actions.size()) continue;
        auto skill_ = actions[index_].skill;
        // 元のSkillをコピーして調整し、コンボ内の判定・正規化イベントの順序は維持する。
        for (auto& phase_ : skill_.phases)
        {
            if (phase_.endCondition == SkillPhaseEnd::DURATION)
            {
                if (phase_.animationState == "SS_Idle") phase_.duration = (std::min)(phase_.duration,0.07f);
                else if (phase_.animationState == "HumanBossAttackTell")
                    phase_.duration = (std::max)(berserkMinimumTell*berserkSpeeds[berserkStage], (std::min)(phase_.duration,0.45f));
            }
            phase_.blendDuration = (std::min)(phase_.blendDuration,0.05f);
        }
        if (!executor_->CanStart(skill_,HumanBossDefense::NONE,target.lock())) continue;
        const bool shadow_ = actions[index_].category == HumanBossState::SHADOW;
        const auto shift_ = static_cast<HumanBossShadowShift>(std::clamp(actions[index_].shadowShiftMode,0,3));
        const bool started_ = shadow_ ? executor_->BeginShadowShift(skill_,shift_,target.lock(),strafeDirection)
            : executor_->Begin(skill_,HumanBossDefense::NONE,{0,0,0},target.lock());
        if (!started_ || !executor_->IsActive()) continue;
        // 回数はAI行動の開始成功時だけ加算する。コンボ内ヒットとParry通知では加算しない。
        ++berserkStartedCount; berserkAttackInFlight = true; currentAction = static_cast<int>(index_);
        return true;
    }
    return false;
}

void HumanBossAIComponent::FinishBerserkAttack()
{
    if (!berserkAttackInFlight) return;
    berserkAttackInFlight = false;
    if (const auto executor_ = executor.lock()) executor_->Stop();
    currentAction = -1;
    if (berserkStartedCount >= berserkAttackCounts[berserkStage]) EnterBerserkStun();
    else berserkTimer = berserkActionGap*berserkSpeeds[berserkStage];
}

void HumanBossAIComponent::OnBerserkJustGuarded()
{
    BS_PROFILE_SCOPE("HumanBossAIComponent::OnBerserkJustGuarded");
    if (berserkStage < 0 || berserkStunned || !berserkAttackInFlight || finishRequested) return;
    // 最初の通知で受付を閉じてからSkillのSequenceと全所有HitBoxを終了し、再入を防ぐ。
    FinishBerserkAttack();
    if (berserkStunned) return;
    berserkTimer = berserkParryDelay*berserkSpeeds[berserkStage];
    if (const auto owner_ = GetGameObject())
        if (const auto animation_ = AnimationGraphBridgeComponent::Acquire(owner_))
            animation_->RequestStateTransition("Block_Hit_Break_Seq",0.03f);
}

void HumanBossAIComponent::EnterBerserkStun()
{
    BS_PROFILE_SCOPE("HumanBossAIComponent::EnterBerserkStun");
    const auto owner_ = GetGameObject();
    if (!owner_ || berserkStage < 0 || berserkStunned) return;
    berserkStunned = true; berserkAttackInFlight = false;
    // スタンへの遷移と同時に無敵表示を消し、有効な反撃時間を視覚的にも区別する。
    StopBerserkAura();
    berserkTimer = berserkStunDurations[berserkStage];
    SetHierarchyPlaybackRate(owner_,1.0f);
    // ダメージは受けるが、被弾遷移でスタン姿勢や待ち状態を上書きしない。
    if (const auto executor_ = executor.lock()) { executor_->Stop(); executor_->SetBerserkProtection(true); }
    if (const auto animation_ = owner_->GetComponent<AnimationControllerComponent>())
    {
        animation_->LoadMissingStateAsset("HumanBossBerserkDownStart","Data/Animation/StateData/HumanBoss/States/HumanBossBerserkDownStart.json");
        animation_->LoadMissingStateAsset("HumanBossBerserkDownLoop","Data/Animation/StateData/HumanBoss/States/HumanBossBerserkDownLoop.json");
        animation_->SetPresentationControlled(true);
        animation_->SetAnimationPaused(false);
        animation_->ChangeState("HumanBossBerserkDownStart",0.06f,true,0.0f);
        berserkDownElapsed = 0.0f; berserkDownLoop = false;
    }
    ParticleSpawnRequest request_;
    request_.assetPath = "Data/Particle/HumanBossDarkAura.json";
    request_.position = owner_->transform->GetWorldPosition(); request_.position.y += 1.6f;
    request_.lifeTime = berserkStage == 2 ? 3600.0f : berserkTimer;
    berserkStunEffect = owner_->GetManager().GetEffectManager().SpawnParticle(request_);
    if (const auto effect_ = berserkStunEffect.lock())
        if (const auto particle_ = effect_->GetComponent<ParticleComponent>()) particle_->SetEmissionScale(0.25f);
    currentAction = -1;
}

void HumanBossAIComponent::DrawBerserkGui()
{
    if (!ImGui::TreeNode("HP Berserk / Final Stun")) return;
    ImGui::DragFloat("Blood eye HP threshold", &bloodEyeHpThreshold, 0.01f, 0.26f, 1.0f);
    ImGui::DragFloat("Blood eye duration", &bloodEyeDuration, 0.5f, 1.0f, 60.0f);
    ImGui::Text("Blood eye: %s / %.1f s", bloodEyeRemaining > 0.0f ? "ACTIVE" : bloodEyeUsed ? "USED" : "READY", bloodEyeRemaining);
    ImGui::Text("Stage %d / actions %d / stun %s / final reserved %s",berserkStage,berserkStartedCount,berserkStunned?"YES":"NO",finalPhaseReserved?"YES":"NO");
    for (int index_ = 0; index_ < 3; ++index_)
    {
        ImGui::PushID(index_);
        ImGui::Text("HP %s / queued %d / used %d",index_==0?"25%":index_==1?"10%":"5%",berserkPending[index_],berserkUsed[index_]);
        ImGui::DragInt("AI actions",&berserkAttackCounts[index_],1,1,30);
        ImGui::DragFloat("Common timeline speed",&berserkSpeeds[index_],0.05f,1.0f,2.5f);
        ImGui::DragFloat("Stun seconds (final waits for hit)",&berserkStunDurations[index_],0.1f,0.5f,20.0f);
        ImGui::DragFloat("Entrance blur multiplier",&berserkEntranceBlurMultipliers[index_],0.05f,0.0f,3.0f);
        ImGui::SliderFloat("Entrance monochrome",&berserkEntranceMonochrome[index_],0.0f,1.0f);
        ImGui::PopID();
    }
    ImGui::DragFloat("Parry deflection seconds",&berserkParryDelay,0.01f,0.05f,0.5f);
    ImGui::DragFloat("Between action seconds",&berserkActionGap,0.01f,0.0f,0.3f);
    ImGui::DragFloat("Minimum tell seconds",&berserkMinimumTell,0.01f,0.12f,0.5f);
    ImGui::DragFloat("Berserk entrance seconds",&berserkEntranceDuration,0.01f,0.2f,1.5f);
    ImGui::DragFloat("Berserk entrance blur",&berserkEntranceBlur,0.001f,0.0f,0.08f);
    ImGui::Text("Invulnerable: %s / aura: %s",IsBerserkInvulnerable()?"YES":"NO",berserkAuraEffect.expired()?"MISSING":"ACTIVE");
    ImGui::TextWrapped("Entrance status: %s",berserkStartReason.c_str());
    ImGui::Text("LateUpdate: %s / entrance remaining: %.2f",ShouldLateUpdate()?"ON":"OFF",berserkEntranceRemaining);
    if (const auto owner_ = GetGameObject(); owner_ && owner_->manager)
    {
        if (const auto health_ = owner_->GetComponent<HealthComponent>())
            ImGui::Text("Actual HP: %.2f / %.2f",health_->GetCurrentHealth(),health_->GetMaxHealth());
        if (const auto mode_ = owner_->GetManager().GetGameModeManager())
            ImGui::Text("Game mode: %s",mode_->GetCurrentGameStateName());
    }
    ImGui::TreePop();
}

void HumanBossAIComponent::SerializeBerserk(nlohmann::json& json_) const
{
    json_["Berserk"] = {{"ActionCounts",berserkAttackCounts},{"Speeds",berserkSpeeds},{"StunDurations",berserkStunDurations},
        {"ParryDelay",berserkParryDelay},{"ActionGap",berserkActionGap},{"MinimumTell",berserkMinimumTell},
        {"EntranceDuration",berserkEntranceDuration},{"EntranceBlur",berserkEntranceBlur},
        {"EntranceBlurMultipliers",berserkEntranceBlurMultipliers},{"EntranceMonochrome",berserkEntranceMonochrome},
        {"BloodEyeHpThreshold",bloodEyeHpThreshold},{"BloodEyeDuration",bloodEyeDuration}};
}

void HumanBossAIComponent::DeserializeBerserk(const nlohmann::json& json_)
{
    if (!json_.contains("Berserk") || !json_["Berserk"].is_object()) return;
    const auto& config_ = json_["Berserk"];
    berserkAttackCounts = config_.value("ActionCounts",berserkAttackCounts);
    berserkSpeeds = config_.value("Speeds",berserkSpeeds);
    berserkStunDurations = config_.value("StunDurations",berserkStunDurations);
    berserkEntranceBlurMultipliers = config_.value("EntranceBlurMultipliers",berserkEntranceBlurMultipliers);
    berserkEntranceMonochrome = config_.value("EntranceMonochrome",berserkEntranceMonochrome);
    for (int index_ = 0; index_ < 3; ++index_)
    {
        berserkAttackCounts[index_] = std::clamp(berserkAttackCounts[index_],1,30);
        berserkSpeeds[index_] = std::clamp(berserkSpeeds[index_],1.0f,2.5f);
        berserkStunDurations[index_] = std::clamp(berserkStunDurations[index_],0.5f,20.0f);
        berserkEntranceBlurMultipliers[index_] = std::clamp(berserkEntranceBlurMultipliers[index_],0.0f,3.0f);
        berserkEntranceMonochrome[index_] = std::clamp(berserkEntranceMonochrome[index_],0.0f,1.0f);
    }
    berserkParryDelay = std::clamp(config_.value("ParryDelay",berserkParryDelay),0.05f,0.5f);
    berserkActionGap = std::clamp(config_.value("ActionGap",berserkActionGap),0.0f,0.3f);
    berserkMinimumTell = std::clamp(config_.value("MinimumTell",berserkMinimumTell),0.12f,0.5f);
    berserkEntranceDuration = std::clamp(config_.value("EntranceDuration",berserkEntranceDuration),0.2f,1.5f);
    berserkEntranceBlur = std::clamp(config_.value("EntranceBlur",berserkEntranceBlur),0.0f,0.08f);
    bloodEyeHpThreshold = std::clamp(config_.value("BloodEyeHpThreshold",bloodEyeHpThreshold),0.26f,1.0f);
    bloodEyeDuration = std::clamp(config_.value("BloodEyeDuration",bloodEyeDuration),1.0f,60.0f);
}

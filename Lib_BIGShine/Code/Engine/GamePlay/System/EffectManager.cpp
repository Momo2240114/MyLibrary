#include "PrecompiledHeader.h"
#include "Engine/GamePlay/Combat/HealthComponent.h"

#include "EffectManager.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Combat/AttackEffectHitComponent.h"
#include "Engine/GamePlay/Combat/AttackTemplateManager.h"
#include "Engine/GamePlay/Transform/ModelAttachmentComponent.h"
#include "Engine/Debug/TracyProfiler.h"
#include "Engine/Graphics/Particle/ParticleComponent.h"
#include "Engine/Graphics/Decal/DecalComponent.h"
#include "Engine/Graphics/Resource/GLTFModel.h"
#include "Engine/Graphics/VFX/MeshVFXComponent.h"
#include "Engine/Physics/HitBox.h"
#include "Engine/Physics/PhysXSystem.h"
#include "Engine/Platform/WindowsDialogHelper.h"

#include <DirectXMath.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace
{
    DirectX::XMFLOAT3 NormalizeOrDefault(
        const DirectX::XMFLOAT3& vector_,
        const DirectX::XMFLOAT3& fallback_)
    {
        BS_PROFILE_SCOPE("EffectManager::NormalizeOrDefault");

        // ヒット方向や補間方向がゼロベクトルになった場合は、呼び出し側の基準方向を使う。
        const float lengthSq_ =
            vector_.x * vector_.x + vector_.y * vector_.y + vector_.z * vector_.z;
        if (lengthSq_ <= 0.000001f) return fallback_;
        const float invLength_ = 1.0f / std::sqrt(lengthSq_);
        return
        {
            vector_.x * invLength_,
            vector_.y * invLength_,
            vector_.z * invLength_,
        };
    }

    DirectX::XMFLOAT3 LerpDirection(
        const DirectX::XMFLOAT3& firstDirection_,
        const DirectX::XMFLOAT3& secondDirection_,
        float amount_)
    {
        BS_PROFILE_SCOPE("EffectManager::LerpDirection");

        // 方向ベクトルは補間後に正規化し、速度や距離の差が向きの計算に混ざらないようにする。
        const float clampedAmount_ = std::clamp(amount_, 0.0f, 1.0f);
        return NormalizeOrDefault(
            {
                firstDirection_.x + (secondDirection_.x - firstDirection_.x) * clampedAmount_,
                firstDirection_.y + (secondDirection_.y - firstDirection_.y) * clampedAmount_,
                firstDirection_.z + (secondDirection_.z - firstDirection_.z) * clampedAmount_,
            },
            firstDirection_);
    }

    constexpr size_t EFFECT_PREWARM_COUNT = 4u;
    constexpr size_t EFFECT_DYNAMIC_POOL_MAX_COUNT = 64u;

    float Clamp01(float value_)
    {
        // エフェクトの進行率やフェード率を、シェーダへ渡しやすい0〜1範囲に丸める。
        return std::clamp(value_, 0.0f, 1.0f);
    }

    DirectX::XMFLOAT3 LerpPosition(
        const DirectX::XMFLOAT3& firstPosition_,
        const DirectX::XMFLOAT3& secondPosition_,
        float amount_)
    {
        // 軌跡補間用の単純な線形補間。方向補間と違い、位置なので正規化しない。
        return
        {
            firstPosition_.x + (secondPosition_.x - firstPosition_.x) * amount_,
            firstPosition_.y + (secondPosition_.y - firstPosition_.y) * amount_,
            firstPosition_.z + (secondPosition_.z - firstPosition_.z) * amount_,
        };
    }
}

void to_json(nlohmann::json& json_, const SlashEffectSpawnSetting& setting_)
{
    // 攻撃演出の生成タイミング、姿勢、当たり判定、ヒット演出を1つの設定として保存する。
    // キー名は既存アセットとの互換を優先して小文字系のまま維持する。
    json_ = {
        { "vfxPath", setting_.vfxPath },
        { "spawnTime", setting_.spawnTime },
        { "lifeTime", setting_.lifeTime },
        { "offset", { setting_.localOffset.x, setting_.localOffset.y, setting_.localOffset.z } },
        { "rotation", { setting_.localRotation.x, setting_.localRotation.y, setting_.localRotation.z } },
        { "scale", { setting_.localScale.x, setting_.localScale.y, setting_.localScale.z } },
        { "attachNodeName", setting_.attachNodeName },
        { "playbackSpeed", setting_.playbackSpeed },
        { "hasHitBox", setting_.hasHitBox },
        { "hitBoxName", setting_.hitBoxName },
        { "hitStartTime", setting_.hitStartTime },
        { "hitEndTime", setting_.hitEndTime },
        { "hitBoxRadius", setting_.hitBoxRadius },
        { "hitBoxHalfHeight", setting_.hitBoxHalfHeight },
        { "damage", setting_.damage },
        { "damageRate", setting_.damageRate },
        { "fixedDamage", setting_.fixedDamage },
        { "damageApplyType", static_cast<int>(setting_.damageApplyType) },
        { "damageHitInterval", setting_.damageHitInterval },
        { "maxHitCount", setting_.maxHitCount },
        { "impactPower", setting_.impactPower },
        { "knockBackPower", setting_.knockBackPower },
        { "targetAirLiftPower", setting_.targetAirLiftPower },
        { "attackerAirLiftPower", setting_.attackerAirLiftPower },
        { "hitStopDuration", setting_.hitStopDuration },
        { "hitStopScale", setting_.hitStopScale },
        { "hitEffectPath", setting_.hitEffectPath },
        { "hitGroupId", setting_.hitGroupId },
        { "allowMultiHit", setting_.allowMultiHit },
        { "canBeGuarded", setting_.canBeGuarded },
        { "guardBreak", setting_.guardBreak },
        { "feedback", setting_.feedback },
        { "cameraShakeEnabled", setting_.cameraShakeEnabled },
        { "cameraShakeReceiveDuration", setting_.cameraShakeReceiveDuration },
        { "cameraShakeReceivePositionStrength", setting_.cameraShakeReceivePositionStrength },
        { "cameraShakeReceiveRotationStrength", setting_.cameraShakeReceiveRotationStrength },
        { "cameraShakeAttackScale", setting_.cameraShakeAttackScale },
        { "cameraShakeFrequency", setting_.cameraShakeFrequency },
        { "cameraShakePriority", setting_.cameraShakePriority },
        { "followOwner", setting_.followOwner },
        { "mirror", setting_.mirror },
    };
}

void from_json(const nlohmann::json& json_, SlashEffectSpawnSetting& setting_)
{
    // 省略された項目は実戦で破綻しにくい初期値に戻し、古いJSONもそのまま読めるようにする。
    setting_.vfxPath = json_.value("vfxPath", std::string{});
    setting_.spawnTime = json_.value("spawnTime", 0.0f);
    setting_.lifeTime = json_.value("lifeTime", 0.18f);
    auto readVector3_ = [&json_](const char* key_, const DirectX::XMFLOAT3& fallback_)
        {
            // 配列で保存されたVector3だけを読み、壊れた形式ならフォールバックを返す。
            const nlohmann::json value_ = json_.value(key_, nlohmann::json::array());
            if (!value_.is_array() || value_.size() < 3u) return fallback_;
            return DirectX::XMFLOAT3{
                value_[0].get<float>(), value_[1].get<float>(), value_[2].get<float>() };
        };
    setting_.localOffset = readVector3_("offset", { 0.0f, 1.0f, 1.2f });
    setting_.localRotation = readVector3_("rotation", { 0.0f, 0.0f, 0.0f });
    setting_.localScale = readVector3_("scale", { 1.0f, 1.0f, 1.0f });
    setting_.attachNodeName = json_.value("attachNodeName", std::string{});
    setting_.playbackSpeed = (std::max)(json_.value("playbackSpeed", 1.0f), 0.0f);
    setting_.hasHitBox = json_.value("hasHitBox", false);
    setting_.hitBoxName = json_.value("hitBoxName", std::string("EffectHitBox"));
    setting_.hitStartTime = json_.value("hitStartTime", 0.0f);
    setting_.hitEndTime = json_.value("hitEndTime", 0.08f);
    setting_.hitBoxRadius = json_.value("hitBoxRadius", 0.5f);
    setting_.hitBoxHalfHeight = json_.value("hitBoxHalfHeight", 1.0f);
    setting_.damage = json_.value("damage", 10.0f);
    setting_.damageRate = (std::max)(json_.value("damageRate", setting_.damage / 100.0f), 0.0f);
    setting_.fixedDamage = json_.value("fixedDamage", 0.0f);
    setting_.damageApplyType = static_cast<DAMAGE_APPLY_TYPE>(
        (std::clamp)(json_.value("damageApplyType", 0), 0, 2));
    setting_.damageHitInterval = (std::max)(json_.value("damageHitInterval", 0.0f), 0.0f);
    setting_.maxHitCount = json_.value("maxHitCount", 1);
    setting_.impactPower = (std::max)(json_.value("impactPower", setting_.damage), 0.0f);
    setting_.knockBackPower = json_.value("knockBackPower", 3.0f);
    setting_.targetAirLiftPower = (std::max)(json_.value("targetAirLiftPower", 0.0f), 0.0f);
    setting_.attackerAirLiftPower = (std::max)(json_.value("attackerAirLiftPower", 0.0f), 0.0f);
    setting_.hitStopDuration = json_.value("hitStopDuration", json_.value("hitStopTime", 0.0f));
    setting_.hitStopScale = json_.value("hitStopScale", 0.0f);
    setting_.hitEffectPath = json_.value("hitEffectPath", std::string{});
    setting_.hitGroupId = json_.value("hitGroupId", 0u);
    setting_.allowMultiHit = json_.value("allowMultiHit", false);
    setting_.canBeGuarded = json_.value("canBeGuarded", true);
    setting_.guardBreak = json_.value("guardBreak", false);
    setting_.feedback = json_.value("feedback", AttackFeedbackSettings{});
    setting_.cameraShakeEnabled = json_.value("cameraShakeEnabled", true);
    setting_.cameraShakeReceiveDuration =
        (std::max)(json_.value("cameraShakeReceiveDuration", 0.12f), 0.0f);
    setting_.cameraShakeReceivePositionStrength =
        (std::max)(json_.value("cameraShakeReceivePositionStrength", 0.025f), 0.0f);
    setting_.cameraShakeReceiveRotationStrength =
        (std::max)(json_.value("cameraShakeReceiveRotationStrength", 0.18f), 0.0f);
    setting_.cameraShakeAttackScale =
        (std::clamp)(json_.value("cameraShakeAttackScale", 0.5f), 0.0f, 1.0f);
    setting_.cameraShakeFrequency =
        (std::max)(json_.value("cameraShakeFrequency", 24.0f), 0.0f);
    setting_.cameraShakePriority = json_.value("cameraShakePriority", 5);
    setting_.followOwner = json_.value("followOwner", false);
    setting_.mirror = json_.value("mirror", false);
}

void EffectManager::Initialize(GameObjectManager* gameObjectManager_, bool preloadEffects_)
{
    gameObjectManager = gameObjectManager_;
    // アセット名の候補と検索マップを作ってから、プール用に使用頻度の高いEffectを事前生成する。
    RefreshAssetCache();
    BuildAssetNameMap();
    incrementalPreloadCursor = 0;
    if (preloadEffects_) PreloadAllEffects();
}

// 遅延中の攻撃演出と生成済みEffectの寿命を更新する
void EffectManager::Update(float deltaTime_)
{
    const float safeDeltaTime_ = (std::max)(deltaTime_, 0.0f);

    // spawnTimeが設定された攻撃演出を生成可能になるまで進める
    for (ScheduledAttackEffect& scheduledEffect_ : scheduledAttackEffects)
    {
        const auto* owner_ = scheduledEffect_.request.ownerObject;
        scheduledEffect_.timeRemaining -= safeDeltaTime_ * (owner_ ? owner_->GetRuntimePlaybackRate() : 1.0f);
    }
    std::vector<AttackEffectSpawnRequest> readyRequests_;
    for (const ScheduledAttackEffect& scheduledEffect_ : scheduledAttackEffects)
    {
        if (scheduledEffect_.timeRemaining <= 0.0f)
        {
            readyRequests_.push_back(scheduledEffect_.request);
        }
    }
    scheduledAttackEffects.erase(
        std::remove_if(
            scheduledAttackEffects.begin(),
            scheduledAttackEffects.end(),
            [](const ScheduledAttackEffect& scheduledEffect_)
            {
                return scheduledEffect_.timeRemaining <= 0.0f;
            }),
        scheduledAttackEffects.end());
    for (AttackEffectSpawnRequest& readyRequest_ : readyRequests_)
    {
        readyRequest_.setting.spawnTime = 0.0f;
        SpawnSlashEffect(readyRequest_);
    }
    ProcessTrailParticleEmissions();

    // 管理中Effectの寿命、Sequence時間、HitBox有効時間を更新する
    for (ManagedEffect& effect_ : managedEffects)
    {
        const auto clockObject_ = effect_.object.lock();
        const float effectDelta_ = safeDeltaTime_ * (clockObject_ ? clockObject_->GetRuntimePlaybackRate() : 1.0f);
        if (effect_.hasLifetimeOwner)
        {
            // 持続演出の所有Componentが削除された場合も、Poolへ返して残留を防ぐ。
            const auto owner_ = effect_.lifetimeOwner.lock();
            if (!owner_ || !owner_->IsEnabled()) effect_.lifeTimer = 0.0f;
        }
        else effect_.lifeTimer -= effectDelta_;
        effect_.elapsedTime += effectDelta_;
        if (effect_.poolType == EFFECT_POOL_TYPE::VFX_SEQUENCE)
        {
            effect_.sequenceTime += effectDelta_;
            UpdateVFXSequence(effect_);
        }
        if (const std::shared_ptr<HitBox> hitBox_ = effect_.hitBox.lock())
        {
            const bool shouldEnable_ =
                effect_.elapsedTime >= effect_.hitStartTime &&
                effect_.elapsedTime <= effect_.hitEndTime;
            if (shouldEnable_ && !hitBox_->IsEnabled()) hitBox_->Activate();
            if (!shouldEnable_ && hitBox_->IsEnabled()) hitBox_->Deactivate();
        }
    }

    // 寿命が終わったEffectをPoolへ戻すかGameObjectとして破棄する
    managedEffects.erase(
        std::remove_if(
            managedEffects.begin(),
            managedEffects.end(),
            [this](ManagedEffect& effect_)
            {
                const std::shared_ptr<GameObject> object_ = effect_.object.lock();
                if (!object_) return true;

                if (effect_.lifeTimer > 0.0f) return false;

                if (effect_.poolType != EFFECT_POOL_TYPE::NONE)
                {
                    ReleasePooledEffect(effect_);
                }
                else if (gameObjectManager)
                {
                    gameObjectManager->Destroy(object_);
                }
                return true;
            }),
        managedEffects.end());
}

// 管理中Effectを解放し、遅延生成待ちの攻撃演出を消去する
void EffectManager::Clear()
{
    for (ManagedEffect& effect_ : managedEffects)
    {
        if (effect_.poolType != EFFECT_POOL_TYPE::NONE)
        {
            ReleasePooledEffect(effect_);
        }
    }
    managedEffects.clear();
    scheduledAttackEffects.clear();
    queuedTrailParticleEmissions.clear();
}

// Effect用AssetフォルダからJSON Asset一覧を収集する
void EffectManager::RefreshAssetCache()
{
    CollectJsonAssets("Data/Particle", particleAssetPaths);
    CollectJsonAssets("Data/VFX", meshVFXAssetPaths);
    CollectJsonAssets("Data/VFXSequence", vfxSequenceAssetPaths);
    CollectJsonAssets("Data/ImpactEffect", impactEffectAssetPaths);
}

// 収集したAssetパスから名前解決用のMapを作成する
void EffectManager::BuildAssetNameMap()
{
    BuildAssetNameMap(
        particleAssetPaths,
        "Data/Particle",
        particleAssets,
        particleAssetKeys,
        particlePathByKey);
    BuildAssetNameMap(
        meshVFXAssetPaths,
        "Data/VFX",
        meshVFXAssets,
        meshVFXAssetKeys,
        meshVFXPathByKey);
    BuildAssetNameMap(
        vfxSequenceAssetPaths,
        "Data/VFXSequence",
        vfxSequenceAssets,
        vfxSequenceAssetKeys,
        vfxSequencePathByKey);
    BuildAssetNameMap(
        impactEffectAssetPaths,
        "Data/ImpactEffect",
        impactEffectAssets,
        impactEffectAssetKeys,
        impactEffectPathByKey);
}

// 登録済みEffect Assetを読み込み、Runtime用CacheとPoolを作成する
void EffectManager::PreloadAllEffects()
{
    Clear();
    particleAssetCache.clear();
    meshVFXAssetCache.clear();
    vfxSequenceAssetCache.clear();
    impactEffectAssetCache.clear();
    particleObjectPool.clear();
    meshVFXObjectPool.clear();
    vfxSequenceObjectPool.clear();

    for (const EffectAssetEntry& entry_ : particleAssets)
    {
        PreloadParticleAsset(entry_.fullPath);
    }

    for (const EffectAssetEntry& entry_ : meshVFXAssets)
    {
        PreloadMeshVFXAsset(entry_.fullPath);
    }
    for (const EffectAssetEntry& entry_ : vfxSequenceAssets)
    {
        PreloadVFXSequenceAsset(entry_.fullPath);
    }
    for (const EffectAssetEntry& entry_ : impactEffectAssets)
    {
        PreloadImpactEffectAsset(entry_.fullPath);
    }
    incrementalPreloadCursor = particleAssets.size() + meshVFXAssets.size() + vfxSequenceAssets.size() + impactEffectAssets.size();
}

// Cache済みAssetからRuntime用Poolを作り直す
void EffectManager::RebuildRuntimePools()
{
    particleObjectPool.clear();
    meshVFXObjectPool.clear();
    vfxSequenceObjectPool.clear();

    for (const EffectAssetEntry& entry_ : particleAssets)
    {
        const std::string assetName_ = BuildAssetNameFromPath(
            entry_.fullPath,
            "Data/Particle");
        const auto assetIterator_ = particleAssetCache.find(assetName_);
        if (assetIterator_ == particleAssetCache.end()) continue;

        std::vector<std::shared_ptr<GameObject>>& objects_ =
            particleObjectPool[assetName_];
        for (size_t index_ = 0; index_ < EFFECT_PREWARM_COUNT; ++index_)
        {
            std::shared_ptr<GameObject> object_ = CreateParticlePoolObject(
                entry_.fullPath,
                assetIterator_->second);
            if (object_) objects_.push_back(object_);
        }
    }

    for (const EffectAssetEntry& entry_ : meshVFXAssets)
    {
        const std::string assetName_ = BuildAssetNameFromPath(
            entry_.fullPath,
            "Data/VFX");
        const auto assetIterator_ = meshVFXAssetCache.find(assetName_);
        if (assetIterator_ == meshVFXAssetCache.end()) continue;

        std::vector<std::shared_ptr<GameObject>>& objects_ =
            meshVFXObjectPool[assetName_];
        for (size_t index_ = 0; index_ < EFFECT_PREWARM_COUNT; ++index_)
        {
            std::shared_ptr<GameObject> object_ = CreateMeshVFXPoolObject(
                entry_.fullPath,
                assetIterator_->second);
            if (object_) objects_.push_back(object_);
        }
    }

    for (const EffectAssetEntry& entry_ : vfxSequenceAssets)
    {
        const std::string assetName_ = BuildAssetNameFromPath(
            entry_.fullPath,
            "Data/VFXSequence");
        const auto assetIterator_ = vfxSequenceAssetCache.find(assetName_);
        if (assetIterator_ == vfxSequenceAssetCache.end()) continue;

        std::vector<std::shared_ptr<GameObject>>& objects_ =
            vfxSequenceObjectPool[assetName_];
        for (size_t index_ = 0; index_ < EFFECT_PREWARM_COUNT; ++index_)
        {
            std::shared_ptr<GameObject> object_ = CreateVFXSequencePoolObject(
                entry_.fullPath,
                assetIterator_->second);
            if (object_) objects_.push_back(object_);
        }
    }

    std::printf(
        "[EffectManager] Runtime pools rebuilt: particle=%zu meshVFX=%zu sequence=%zu\n",
        particleObjectPool.size(),
        meshVFXObjectPool.size(),
        vfxSequenceObjectPool.size());
}

// Particle Asset名またはパスを実際のJSONパスへ解決する
std::string EffectManager::ResolveParticlePath(const std::string& assetNameOrPath_) const
{
    if (assetNameOrPath_.empty()) return {};

    const auto directIterator_ = particlePathByKey.find(assetNameOrPath_);
    if (directIterator_ != particlePathByKey.end()) return directIterator_->second;

    const std::string assetKey_ = BuildAssetNameFromPath(assetNameOrPath_, "Data/Particle");
    const auto keyIterator_ = particlePathByKey.find(assetKey_);
    if (keyIterator_ != particlePathByKey.end()) return keyIterator_->second;

    const std::filesystem::path path_ =
        WindowsDialogHelper::Utf8ToPath(assetNameOrPath_);
    std::error_code error_;
    if (std::filesystem::is_regular_file(path_, error_))
    {
        return WindowsDialogHelper::PathToUtf8(path_.lexically_normal());
    }

    return assetNameOrPath_;
}

// MeshVFX Asset名またはパスを実際のJSONパスへ解決する
std::string EffectManager::ResolveMeshVFXPath(const std::string& assetNameOrPath_) const
{
    if (assetNameOrPath_.empty()) return {};

    const auto directIterator_ = meshVFXPathByKey.find(assetNameOrPath_);
    if (directIterator_ != meshVFXPathByKey.end()) return directIterator_->second;

    const std::string assetKey_ = BuildAssetNameFromPath(assetNameOrPath_, "Data/VFX");
    const auto keyIterator_ = meshVFXPathByKey.find(assetKey_);
    if (keyIterator_ != meshVFXPathByKey.end()) return keyIterator_->second;

    const std::filesystem::path path_ =
        WindowsDialogHelper::Utf8ToPath(assetNameOrPath_);
    std::error_code error_;
    if (std::filesystem::is_regular_file(path_, error_))
    {
        return WindowsDialogHelper::PathToUtf8(path_.lexically_normal());
    }

    return assetNameOrPath_;
}

// VFX Sequence Asset名またはパスを実際のJSONパスへ解決する
std::string EffectManager::ResolveVFXSequencePath(
    const std::string& assetNameOrPath_) const
{
    if (assetNameOrPath_.empty()) return {};

    const auto directIterator_ = vfxSequencePathByKey.find(assetNameOrPath_);
    if (directIterator_ != vfxSequencePathByKey.end()) return directIterator_->second;

    const std::string assetKey_ = BuildAssetNameFromPath(
        assetNameOrPath_,
        "Data/VFXSequence");
    const auto keyIterator_ = vfxSequencePathByKey.find(assetKey_);
    if (keyIterator_ != vfxSequencePathByKey.end()) return keyIterator_->second;

    const std::filesystem::path path_ =
        WindowsDialogHelper::Utf8ToPath(assetNameOrPath_);
    std::error_code error_;
    if (std::filesystem::is_regular_file(path_, error_) &&
        path_.generic_string().find("VFXSequence") != std::string::npos)
    {
        return WindowsDialogHelper::PathToUtf8(path_.lexically_normal());
    }
    return {};
}

// Impact Effect Asset名またはパスを実際のJSONパスへ解決する
std::string EffectManager::ResolveImpactEffectPath(
    const std::string& assetNameOrPath_) const
{
    if (assetNameOrPath_.empty()) return {};
    const auto directIterator_ = impactEffectPathByKey.find(assetNameOrPath_);
    if (directIterator_ != impactEffectPathByKey.end()) return directIterator_->second;

    const std::string assetKey_ = BuildAssetNameFromPath(
        assetNameOrPath_,
        "Data/ImpactEffect");
    const auto keyIterator_ = impactEffectPathByKey.find(assetKey_);
    if (keyIterator_ != impactEffectPathByKey.end()) return keyIterator_->second;

    const std::filesystem::path path_ =
        WindowsDialogHelper::Utf8ToPath(assetNameOrPath_);
    std::error_code error_;
    if (std::filesystem::is_regular_file(path_, error_))
    {
        return WindowsDialogHelper::PathToUtf8(path_.lexically_normal());
    }
    return {};
}

// Particle Assetを取得し、指定位置でEmitterを再生する
std::shared_ptr<GameObject> EffectManager::SpawnParticle(
    const ParticleSpawnRequest& request_)
{
    if (!gameObjectManager || request_.assetPath.empty()) return nullptr;

    const std::string resolvedAssetPath_ =
        ResolveParticlePath(request_.assetPath);
    std::printf(
        "[EffectManager::SpawnParticle] request='%s' resolved='%s' emitter='%s'\n",
        request_.assetPath.c_str(),
        resolvedAssetPath_.c_str(),
        request_.emitterName.c_str());

    std::shared_ptr<GameObject> object_ =
        AcquireParticleObject(resolvedAssetPath_);

    if (!object_) return nullptr;

    const DirectX::XMFLOAT3 spawnPosition_ = ApplyNormalOffset(
        request_.position,
        request_.normal,
        request_.normalOffset);

    Vector3 position_(spawnPosition_.x, spawnPosition_.y, spawnPosition_.z);
    object_->transform->SetLocalPosition(position_);
    object_->UpdateTransform();

    std::shared_ptr<ParticleComponent> particle_ =
        object_->GetComponent<ParticleComponent>();

    if (!particle_) return object_;
    // 演出プレビュー等で抑制されたPool個体を再利用しても、UpdateとRenderへ復帰させる。
    particle_->SetRuntimeSuppressed(false);
    particle_->ResetUpdateTiming();
    particle_->SetEnabled(true);
    particle_->ResetRuntime();
    particle_->SetRuntimeSizeScale(request_.sizeScale);
    for (ParticleComponent::EmitterTrack& track_ : particle_->GetTracks())
    {
        // EffectManager経由のParticleは生成Objectの位置を基準にし、Asset内のWorld原点固定を避ける。
        if (track_.spawnSpace == ParticleSpawnSpace::WORLD)
        {
            track_.spawnSpace = ParticleSpawnSpace::OWNER_RELATIVE;
        }
    }
    if (request_.overrideAxis || request_.overrideTargetAttraction)
    {
        for (ParticleComponent::EmitterTrack& track_ : particle_->GetTracks())
        {
            if (request_.overrideAxis)
            {
                const DirectX::XMFLOAT3 axis_ = NormalizeOrDefault(
                    request_.axis,
                    { 0.0f, 1.0f, 0.0f });
                track_.settings.spawnDirectionMode =
                    static_cast<uint32_t>(PARTICLE_SPAWN_DIRECTION_MODE::AXIS);
                track_.settings.emissionAxis = { axis_.x, axis_.y, axis_.z, 0.0f };
            }
            if (request_.overrideTargetAttraction)
            {
                track_.settings.useTargetAttraction = 1u;
                track_.settings.targetPosition =
                {
                    request_.targetPosition.x,
                    request_.targetPosition.y,
                    request_.targetPosition.z,
                    1.0f
                };
                track_.settings.targetAttractionStrength = request_.targetAttractionStrength;
                track_.settings.targetAttractionRange = request_.targetAttractionRange;
                track_.settings.targetAttractionFalloff =
                    (std::max)(request_.targetAttractionFalloff, 0.001f);
            }
        }
    }

    // Emitter名が指定されている場合は単体Emitterを優先して再生する
    bool played_ = false;
    if (!request_.emitterName.empty())
    {
        played_ = particle_->PlayEmitter(request_.emitterName);
        if (!played_)
        {
            std::printf(
                "[EffectManager::SpawnParticle] emitter '%s' was not found in '%s'; playing all emitters\n",
                request_.emitterName.c_str(),
                resolvedAssetPath_.c_str());
        }
    }
    if (!played_)
    {
        particle_->Play();
    }

    RegisterManagedEffect(
        object_,
        request_.lifeTime,
        nullptr,
        0.0f,
        0.0f,
        EFFECT_POOL_TYPE::PARTICLE,
        resolvedAssetPath_);
    return object_;
}

// 攻撃用MeshVFXを生成し、必要に応じてHitBoxを設定する
void EffectManager::QueueTrailParticleEmission(
    const TrailParticleEmissionRequest& request_)
{
    BS_PROFILE_SCOPE("EffectManager::QueueTrailParticleEmission");
    if (request_.assetPath.empty()) return;
    queuedTrailParticleEmissions.push_back(request_);
}

void EffectManager::ProcessTrailParticleEmissions()
{
    BS_PROFILE_SCOPE("EffectManager::ProcessTrailParticleEmissions");
    if (queuedTrailParticleEmissions.empty()) return;

    std::vector<TrailParticleEmissionRequest> requests_ =
        std::move(queuedTrailParticleEmissions);
    queuedTrailParticleEmissions.clear();

    for (const TrailParticleEmissionRequest& request_ : requests_)
    {
        const int spawnCount_ = std::clamp(request_.spawnCount, 1, 32);
        const DirectX::XMFLOAT3 normal_ = BuildTrailParticleNormal(request_);
        for (int spawnIndex_ = 0; spawnIndex_ < spawnCount_; ++spawnIndex_)
        {
            ParticleSpawnRequest particleRequest_{};
            particleRequest_.assetPath = request_.assetPath;
            particleRequest_.emitterName = request_.emitterName;
            particleRequest_.position = BuildTrailParticlePosition(request_);
            particleRequest_.normal = normal_;
            particleRequest_.lifeTime = request_.lifeTime;
            SpawnParticle(particleRequest_);
        }
    }
}

DirectX::XMFLOAT3 EffectManager::BuildTrailParticlePosition(
    const TrailParticleEmissionRequest& request_)
{
    BS_PROFILE_SCOPE("EffectManager::BuildTrailParticlePosition");
    const float randomWidth_ = GenerateTrailParticleRandom01();
    const float minWidth_ = Clamp01((std::min)(request_.widthMin, request_.widthMax));
    const float maxWidth_ = Clamp01((std::max)(request_.widthMin, request_.widthMax));
    const float biasedWidth_ = Clamp01(randomWidth_ + request_.tipBias);
    const float widthRate_ = minWidth_ + (maxWidth_ - minWidth_) * biasedWidth_;
    DirectX::XMFLOAT3 position_ =
        LerpPosition(request_.leftPosition, request_.rightPosition, widthRate_);
    if (request_.positionJitter > 0.0f)
    {
        position_.x += (GenerateTrailParticleRandom01() * 2.0f - 1.0f) * request_.positionJitter;
        position_.y += (GenerateTrailParticleRandom01() * 2.0f - 1.0f) * request_.positionJitter;
        position_.z += (GenerateTrailParticleRandom01() * 2.0f - 1.0f) * request_.positionJitter;
    }
    return position_;
}

DirectX::XMFLOAT3 EffectManager::BuildTrailParticleNormal(
    const TrailParticleEmissionRequest& request_) const
{
    DirectX::XMFLOAT3 normal_{ 0.0f, 1.0f, 0.0f };
    const DirectX::XMVECTOR widthVector_ = DirectX::XMVectorSubtract(
        DirectX::XMLoadFloat3(&request_.rightPosition),
        DirectX::XMLoadFloat3(&request_.leftPosition));
    if (DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(widthVector_)) > 0.000001f)
    {
        DirectX::XMStoreFloat3(&normal_, DirectX::XMVector3Normalize(widthVector_));
    }
    return normal_;
}

float EffectManager::GenerateTrailParticleRandom01()
{
    trailParticleRandomState = trailParticleRandomState * 1664525u + 1013904223u;
    return static_cast<float>((trailParticleRandomState >> 8) & 0x00FFFFFFu) /
        static_cast<float>(0x01000000u);
}

std::shared_ptr<GameObject> EffectManager::SpawnSlashEffect(
    const AttackEffectSpawnRequest& request_)
{
    if (!gameObjectManager || request_.setting.vfxPath.empty()) return nullptr;
    if (request_.ownerObject)
    {
        const auto health_ = request_.ownerObject->GetComponent<HealthComponent>();
        // 撃破後に古いAnimation Eventが走っても、斬撃VFXや遅延予約を作らない。
        if (request_.ownerObject->IsPendingDestroy() || (health_ && health_->IsDead())) return nullptr;
    }
    if (request_.setting.spawnTime > 0.0f)
    {
        ScheduledAttackEffect scheduledEffect_{};
        scheduledEffect_.request = request_;
        scheduledEffect_.timeRemaining = request_.setting.spawnTime;
        scheduledAttackEffects.push_back(scheduledEffect_);
        return nullptr;
    }

    MeshVFXSpawnRequest vfxRequest_{};
    vfxRequest_.assetPath = request_.setting.vfxPath;
    vfxRequest_.lifeTime = request_.setting.lifeTime;
    std::shared_ptr<GameObject> object_ = SpawnMeshVFX(vfxRequest_);
    if (!object_) return nullptr;
    if (std::shared_ptr<MeshVFXComponent> meshVFX_ = object_->GetComponent<MeshVFXComponent>())
    {
        meshVFX_->SetPlaybackSpeed(request_.setting.playbackSpeed);
    }

    using namespace DirectX;
    DirectX::XMFLOAT3 localScale_ = request_.setting.localScale;
    if (request_.setting.mirror)
    {
        localScale_.x *= -1.0f;
    }
    // 攻撃者の姿勢を基準に、演出用のローカル姿勢をワールドへ反映する
    const XMMATRIX localMatrix_ =
        XMMatrixScaling(
            localScale_.x,
            localScale_.y,
            localScale_.z) *
        XMMatrixRotationRollPitchYaw(
            XMConvertToRadians(request_.setting.localRotation.x),
            XMConvertToRadians(request_.setting.localRotation.y),
            XMConvertToRadians(request_.setting.localRotation.z)) *
        XMMatrixTranslation(
            request_.setting.localOffset.x,
            request_.setting.localOffset.y,
            request_.setting.localOffset.z);
    const XMMATRIX ownerWorldMatrix_ = request_.ownerObject && request_.ownerObject->transform
        ? request_.ownerObject->transform->GetWorldMatrix()
        : XMMatrixIdentity();
    if (request_.setting.followOwner && request_.ownerObject && request_.ownerObject->transform)
    {
        object_->transform->SetParent(request_.ownerObject->transform);
        object_->transform->localPosition = request_.setting.localOffset;
        object_->transform->localRotation = request_.setting.localRotation;
        object_->transform->localScale = localScale_;
        object_->transform->MarkDirty();
        if (!request_.setting.attachNodeName.empty())
        {
            std::shared_ptr<GltfModelCommponennt> model_ =
                request_.ownerObject->GetComponent<GltfModelCommponennt>();
            if (model_)
            {
                std::shared_ptr<ModelAttachmentComponent> attachment_ =
                    object_->GetOrAddComponent<ModelAttachmentComponent>();
                if (attachment_)
                {
                    attachment_->SetEnabled(true);
                    attachment_->SetTargetNode(model_, request_.setting.attachNodeName);
                    attachment_->SyncAttachment();
                }
            }
        }
    }
    else
    {
        object_->transform->ApplyWorldMatrix(localMatrix_ * ownerWorldMatrix_);
    }
    object_->UpdateTransform();

    std::shared_ptr<HitBox> hitBox_;
    if (request_.setting.hasHitBox)
    {
        // 攻撃元と同じAttackInstanceIdを使用して同一攻撃のHit管理へ接続する
        uint32_t attackInstanceId_ = request_.attackInstanceId;
        std::shared_ptr<HitBox> sourceHitBox_;
        if (request_.ownerObject != nullptr)
        {
            sourceHitBox_ = request_.ownerObject->GetComponent<HitBox>();
            if (attackInstanceId_ == 0u && sourceHitBox_)
            {
                attackInstanceId_ = sourceHitBox_->attackInstanceId;
            }
        }
        if (attackInstanceId_ == 0u)
        {
            attackInstanceId_ = gameObjectManager->GetAttackSystem().CreateAttackInstance();
            if (sourceHitBox_) sourceHitBox_->SetAttackInstanceId(attackInstanceId_);
        }

        hitBox_ = object_->GetOrAddComponent<HitBox>();
        if (hitBox_)
        {
            hitBox_->SetEnabled(true);
            hitBox_->actionPrefix = request_.setting.hitBoxName;
            hitBox_->SetOwner(request_.ownerObject);
            hitBox_->SetAttackInstanceId(attackInstanceId_);
            hitBox_->SetHitGroupId(request_.setting.hitGroupId);
            hitBox_->SetDamage(request_.setting.damage);
            hitBox_->SetDamageRate(request_.setting.damageRate);
            hitBox_->SetFixedDamage(request_.setting.fixedDamage);
            hitBox_->SetDamageApplyType(request_.setting.damageApplyType);
            hitBox_->SetDamageHitInterval(request_.setting.damageHitInterval);
            hitBox_->SetMaxHitCount(request_.setting.maxHitCount);
            hitBox_->SetImpactPower(request_.setting.impactPower);
            hitBox_->SetKnockBackPower(request_.setting.knockBackPower);
            hitBox_->SetAirLiftPower(
                request_.setting.targetAirLiftPower,
                request_.setting.attackerAirLiftPower);
            hitBox_->SetHitEffectPath(request_.setting.hitEffectPath);
            hitBox_->SetHitEffectBlackboardOverrideEnabled(false);
            hitBox_->hitStopDuration = request_.setting.hitStopDuration;
            hitBox_->hitStopScale = request_.setting.hitStopScale;
            hitBox_->allowMultiHit = request_.setting.allowMultiHit;
            hitBox_->canBeGuarded = request_.setting.canBeGuarded;
            hitBox_->guardBreak = request_.setting.guardBreak;
            hitBox_->feedback = request_.setting.feedback;
            hitBox_->cameraShakeEnabled = request_.setting.cameraShakeEnabled;
            hitBox_->cameraShakeReceiveDuration =
                request_.setting.cameraShakeReceiveDuration;
            hitBox_->cameraShakeReceivePositionStrength =
                request_.setting.cameraShakeReceivePositionStrength;
            hitBox_->cameraShakeReceiveRotationStrength =
                request_.setting.cameraShakeReceiveRotationStrength;
            hitBox_->cameraShakeAttackScale =
                request_.setting.cameraShakeAttackScale;
            hitBox_->cameraShakeFrequency = request_.setting.cameraShakeFrequency;
            hitBox_->cameraShakePriority = request_.setting.cameraShakePriority;
            hitBox_->radius = request_.setting.hitBoxRadius;
            hitBox_->halfHeight = request_.setting.hitBoxHalfHeight;
            hitBox_->Deactivate();
            hitBox_->RecreatePhysXActor();
        }
    }

    // Effectの寿命管理情報へHitBoxの有効時間を設定する
    for (ManagedEffect& effect_ : managedEffects)
    {
        if (effect_.object.lock() == object_)
        {
            effect_.hitBox = hitBox_;
            effect_.hitStartTime = (std::max)(request_.setting.hitStartTime, 0.0f);
            effect_.hitEndTime = (std::max)(request_.setting.hitEndTime, effect_.hitStartTime);
            break;
        }
    }
    if (request_.ownerObject) RegisterAttackEffectOwner(object_, request_.ownerObject->weak_from_this().lock());
    return object_;
}

std::shared_ptr<GameObject> EffectManager::SpawnAttackEffect(
    const AttackEffectSpawnRequest& request_)
{
    return SpawnSlashEffect(request_);
}

// AttackTemplateとEffectTemplateから攻撃判定付きのRuntime Effectを生成する
std::shared_ptr<GameObject> EffectManager::SpawnAttackEffect(
    const AttackEffectSpawnTemplate& effectTemplate_,
    const AttackTemplate& attackTemplate_,
    GameObject* ownerObject_,
    uint32_t attackInstanceId_)
{
    if (!gameObjectManager || !effectTemplate_.enabled) return nullptr;
    std::shared_ptr<GameObject> object_ = gameObjectManager->Create("RuntimeAttackEffect");
    if (!object_) return nullptr;
    object_->SetNoSaveObject();

    using namespace DirectX;
    // 攻撃者の姿勢を基準にEffectTemplateのローカル姿勢を反映する
    const XMMATRIX localMatrix_ =
        XMMatrixScaling(effectTemplate_.localScale.x, effectTemplate_.localScale.y, effectTemplate_.localScale.z) *
        XMMatrixRotationRollPitchYaw(
            XMConvertToRadians(effectTemplate_.localRotation.x),
            XMConvertToRadians(effectTemplate_.localRotation.y),
            XMConvertToRadians(effectTemplate_.localRotation.z)) *
        XMMatrixTranslation(effectTemplate_.localOffset.x, effectTemplate_.localOffset.y, effectTemplate_.localOffset.z);
    const XMMATRIX ownerMatrix_ = ownerObject_ && ownerObject_->transform
        ? ownerObject_->transform->GetWorldMatrix() : XMMatrixIdentity();
    if (effectTemplate_.followOwner && ownerObject_ && ownerObject_->transform)
    {
        object_->transform->SetParent(ownerObject_->transform);
        object_->transform->localPosition = effectTemplate_.localOffset;
        object_->transform->localRotation = effectTemplate_.localRotation;
        object_->transform->localScale = effectTemplate_.localScale;
        object_->transform->MarkDirty();
    }
    else
    {
        object_->transform->ApplyWorldMatrix(localMatrix_ * ownerMatrix_);
    }
        object_->UpdateTransform();

    if (!effectTemplate_.particlePath.empty())
    {
        // EffectTemplateに設定されたParticle AssetをRuntime Effectへ追加する
        const std::string resolvedParticlePath_ =
            ResolveParticlePath(effectTemplate_.particlePath);
        const ParticleEffectAsset* particleAsset_ =
            FindParticleAssetFromRequest(resolvedParticlePath_);
        std::shared_ptr<ParticleComponent> particle_ =
            object_->AddComponent<ParticleComponent>();
        if (particle_ && particleAsset_)
        {
            particle_->SetAsset(*particleAsset_);
            particle_->ResetRuntime();
            particle_->Play();
        }
        else if (!particleAsset_)
        {
            std::printf(
                "[EffectManager::SpawnAttackEffect] Particle cache miss: %s\n",
                resolvedParticlePath_.c_str());
        }
    }
    if (!effectTemplate_.meshVfxPath.empty())
    {
        // EffectTemplateに設定されたMeshVFX AssetをRuntime Effectへ追加する
        const std::string resolvedMeshVFXPath_ =
            ResolveMeshVFXPath(effectTemplate_.meshVfxPath);
        const MeshVFXAsset* meshVFXAsset_ =
            FindMeshVFXAssetFromRequest(resolvedMeshVFXPath_);
        std::shared_ptr<MeshVFXComponent> meshVFX_ =
            object_->AddComponent<MeshVFXComponent>();
        if (meshVFX_ && meshVFXAsset_)
        {
            meshVFX_->SetAsset(*meshVFXAsset_);
            meshVFX_->Reset();
            meshVFX_->Play();
        }
        else if (!meshVFXAsset_)
        {
            std::printf(
                "[EffectManager::SpawnAttackEffect] MeshVFX cache miss: %s\n",
                resolvedMeshVFXPath_.c_str());
        }
    }

    std::shared_ptr<AttackEffectHitComponent> attackHit_ = object_->AddComponent<AttackEffectHitComponent>();
    if (attackHit_)
    {
        // AttackTemplateの攻撃性能をEffect側のHit処理へ渡す
        attackHit_->SetOwner(ownerObject_);
        attackHit_->SetAttackInstanceId(attackInstanceId_);
        attackHit_->SetHitGroupId(attackTemplate_.hitGroupId);
        attackHit_->SetDamage(attackTemplate_.damage);
        attackHit_->SetDamageRate(attackTemplate_.damageRate);
        attackHit_->SetFixedDamage(attackTemplate_.fixedDamage);
        attackHit_->SetDamageApplyType(attackTemplate_.damageApplyType);
        attackHit_->SetDamageHitInterval(attackTemplate_.damageHitInterval);
        attackHit_->SetMaxHitCount(attackTemplate_.maxHitCount);
        attackHit_->SetImpactPower(attackTemplate_.impactPower);
        attackHit_->SetHitStopDuration(attackTemplate_.hitStopDuration);
        attackHit_->SetHitStopScale(attackTemplate_.hitStopScale);
        attackHit_->SetKnockBackPower(attackTemplate_.knockBackPower);
        attackHit_->SetAirLiftPower(
            attackTemplate_.targetAirLiftPower,
            attackTemplate_.attackerAirLiftPower);
        attackHit_->feedback = attackTemplate_.feedback;
        attackHit_->SetCameraShake(
            attackTemplate_.cameraShake.enabled,
            attackTemplate_.cameraShake.receiveDuration,
            attackTemplate_.cameraShake.receivePositionStrength,
            attackTemplate_.cameraShake.receiveRotationStrength,
            attackTemplate_.cameraShake.attackScale,
            attackTemplate_.cameraShake.frequency,
            attackTemplate_.cameraShake.priority);
        attackHit_->SetHitParticleAssetPath(
            attackTemplate_.hitEffect.spawnOnHit
            ? attackTemplate_.hitEffect.hitParticleAssetPath
            : std::string{});
        attackHit_->SetHitParticleEmitterName(
            attackTemplate_.hitEffect.spawnOnHit
            ? attackTemplate_.hitEffect.hitParticleEmitterName
            : std::string{});
        attackHit_->SetHitParticleLifeTime(attackTemplate_.hitEffect.hitParticleLifeTime);
        attackHit_->SetHitParticleNormalOffset(attackTemplate_.hitEffect.hitParticleNormalOffset);
        attackHit_->SetHitParticleImpactAxis(
            attackTemplate_.hitEffect.hitParticleUseImpactAxis,
            attackTemplate_.hitEffect.hitParticleAxisToAttackerBlend);
        attackHit_->SetHitParticleTargetAttraction(
            attackTemplate_.hitEffect.hitParticleUseTargetAttraction,
            attackTemplate_.hitEffect.hitParticleTargetAttractionStrength,
            attackTemplate_.hitEffect.hitParticleTargetAttractionRange,
            attackTemplate_.hitEffect.hitParticleTargetAttractionFalloff);
        attackHit_->SetHitMeshVFXAssetPath(
            attackTemplate_.hitEffect.spawnOnHit
            ? attackTemplate_.hitEffect.hitMeshVfxAssetPath
            : std::string{});
        attackHit_->SetHitMeshVFXLifeTime(attackTemplate_.hitEffect.hitMeshVfxLifeTime);
        attackHit_->SetHitMeshVFXNormalOffset(attackTemplate_.hitEffect.hitMeshVfxNormalOffset);
        attackHit_->SetAllowMultiHit(
            attackTemplate_.allowMultiHit ||
            attackTemplate_.damageApplyType != DAMAGE_APPLY_TYPE::SINGLE_HIT);
        attackHit_->SetCanHitOwner(attackTemplate_.canHitOwner);
        attackHit_->SetGuardProperties(attackTemplate_.canBeGuarded, attackTemplate_.guardBreak);
        attackHit_->SetEffectType(effectTemplate_.effectType);
        attackHit_->SetLifeTime(effectTemplate_.lifeTime);
        attackHit_->SetMovement(effectTemplate_.localDirection, effectTemplate_.speed);
        attackHit_->SetHitShapes(effectTemplate_.hitShapes);
        attackHit_->Play();
    }
    if (ownerObject_) RegisterAttackEffectOwner(object_, ownerObject_->weak_from_this().lock());
    return object_;
}

std::shared_ptr<GameObject> EffectManager::SpawnEffectWithHitBox(
    const AttackEffectSpawnRequest& request_)
{
    AttackEffectSpawnRequest hitBoxRequest_ = request_;
    hitBoxRequest_.setting.hasHitBox = true;
    return SpawnSlashEffect(hitBoxRequest_);
}

// MeshVFXまたはVFX Sequenceを生成し、寿命管理対象として登録する
std::shared_ptr<GameObject> EffectManager::SpawnMeshVFX(
    const MeshVFXSpawnRequest& request_)
{
    if (!gameObjectManager || request_.assetPath.empty()) return nullptr;
    const std::string resolvedSequencePath_ =
        ResolveVFXSequencePath(request_.assetPath);
    if (!resolvedSequencePath_.empty())
    {
        const auto object_ = SpawnVFXSequence(request_, resolvedSequencePath_);
        if (object_ && !request_.lifetimeOwner.expired())
        {
            const auto effect_ = std::find_if(managedEffects.begin(), managedEffects.end(),
                [&object_](const ManagedEffect& current_) { return current_.object.lock() == object_; });
            if (effect_ != managedEffects.end())
            {
                effect_->lifetimeOwner = request_.lifetimeOwner;
                effect_->hasLifetimeOwner = true;
            }
        }
        return object_;
    }
    const std::string resolvedAssetPath_ = ResolveMeshVFXPath(request_.assetPath);

    std::shared_ptr<GameObject> object_ =
        AcquireMeshVFXObject(resolvedAssetPath_);
    if (!object_) return nullptr;

    const DirectX::XMFLOAT3 spawnPosition_ = ApplyNormalOffset(
        request_.position,
        request_.normal,
        request_.normalOffset);
    Vector3 position_(spawnPosition_.x, spawnPosition_.y, spawnPosition_.z);
    object_->transform->SetLocalPosition(position_);
    object_->UpdateTransform();

    std::shared_ptr<MeshVFXComponent> meshVFX_ =
        object_->GetComponent<MeshVFXComponent>();
    if (!meshVFX_) return object_;

    object_->transform->localScale = request_.scale;
    object_->transform->MarkDirty();

    std::printf(
        "[EffectManager::SpawnMeshVFX] request='%s' resolved='%s'\n",
        request_.assetPath.c_str(),
        resolvedAssetPath_.c_str());
    meshVFX_->SetEnabled(true);
    meshVFX_->Reset();
    meshVFX_->Play();

    RegisterManagedEffect(
        object_,
        request_.lifeTime,
        nullptr,
        0.0f,
        0.0f,
        EFFECT_POOL_TYPE::MESH_VFX,
        resolvedAssetPath_);
    if (!request_.lifetimeOwner.expired())
    {
        managedEffects.back().lifetimeOwner = request_.lifetimeOwner;
        managedEffects.back().hasLifetimeOwner = true;
    }
    return object_;
}

void EffectManager::SpawnForHit(const AttackHitEvent& event_)
{
    if (!event_.hasHitContact) return;

    std::printf(
        "[EffectManager::SpawnForHit] particle='%s' emitter='%s' meshVFX='%s' "
        "position=(%.3f, %.3f, %.3f) normal=(%.3f, %.3f, %.3f)\n",
        event_.hitParticleAssetPath.c_str(),
        event_.hitParticleEmitterName.c_str(),
        event_.hitMeshVFXAssetPath.c_str(),
        event_.hitPosition.x,
        event_.hitPosition.y,
        event_.hitPosition.z,
        event_.hitNormal.x,
        event_.hitNormal.y,
        event_.hitNormal.z);

    if (!event_.hitParticleAssetPath.empty())
    {
        ParticleSpawnRequest request_{};
        request_.assetPath = event_.hitParticleAssetPath;
        request_.emitterName = event_.hitParticleEmitterName;
        request_.position = event_.hitPosition;
        request_.normal = event_.hitNormal;
        request_.lifeTime = event_.hitParticleLifeTime;
        request_.normalOffset = event_.hitParticleNormalOffset;
        const auto attacker_ = event_.attacker.lock();
        if (attacker_ && attacker_->GetLayer() == LayerType::Player)
            request_.sizeScale = 2.0f;
        if (event_.hitParticleUseImpactAxis)
        {
            DirectX::XMFLOAT3 attackerDirection_ = event_.hitNormal;
            if (attacker_ && attacker_->transform)
            {
                const DirectX::XMFLOAT3 attackerPosition_ = attacker_->transform->GetWorldPosition();
                attackerDirection_ =
                {
                    attackerPosition_.x - event_.hitPosition.x,
                    attackerPosition_.y - event_.hitPosition.y,
                    attackerPosition_.z - event_.hitPosition.z,
                };
            }
            request_.overrideAxis = true;
            request_.axis = LerpDirection(
                NormalizeOrDefault(event_.hitNormal, { 0.0f, 1.0f, 0.0f }),
                NormalizeOrDefault(attackerDirection_, { 0.0f, 1.0f, 0.0f }),
                event_.hitParticleAxisToAttackerBlend);
        }
        if (event_.hitParticleUseTargetAttraction)
        {
            request_.overrideTargetAttraction = true;
            request_.targetPosition = event_.hitPosition;
            if (const std::shared_ptr<GameObject> target_ = event_.target.lock())
            {
                if (target_->transform)
                {
                    request_.targetPosition = target_->transform->GetWorldPosition();
                }
            }
            request_.targetAttractionStrength = event_.hitParticleTargetAttractionStrength;
            request_.targetAttractionRange = event_.hitParticleTargetAttractionRange;
            request_.targetAttractionFalloff = event_.hitParticleTargetAttractionFalloff;
        }
        RegisterAttackEffectOwner(SpawnParticle(request_), attacker_);
    }

    if (!event_.hitMeshVFXAssetPath.empty())
    {
        MeshVFXSpawnRequest request_{};
        request_.assetPath = event_.hitMeshVFXAssetPath;
        request_.position = event_.hitPosition;
        request_.normal = event_.hitNormal;
        request_.lifeTime = event_.hitMeshVFXLifeTime;
        request_.normalOffset = event_.hitMeshVFXNormalOffset;
        RegisterAttackEffectOwner(SpawnMeshVFX(request_), event_.attacker.lock());
    }
    if (!event_.hitImpactEffectAssetPath.empty())
    {
        // 複数のParticle/Mesh/DecalをまとめたImpactEffectを、Hit位置と法線に合わせて再生する。
        SpawnImpactEffectAsset(event_.hitImpactEffectAssetPath, event_.hitPosition, event_.hitNormal, event_.attacker.lock());
    }
}

// 指定TextureのDecalを生成し、寿命とフェード時間を設定する
std::shared_ptr<GameObject> EffectManager::SpawnDecal(
    const DecalSpawnRequest& request_)
{
    if (!gameObjectManager || request_.texturePath.empty()) return nullptr;

    std::shared_ptr<GameObject> object_ =
        gameObjectManager->Create("RuntimeImpactDecal");
    if (!object_) return nullptr;
    object_->SetNoSaveObject();

    const DirectX::XMFLOAT3 position_ = ApplyNormalOffset(
        request_.position,
        request_.normal,
        request_.normalOffset);
    Vector3 localPosition_(position_.x, position_.y, position_.z);
    Vector3 localScale_(request_.scale.x, request_.scale.y, request_.scale.z);
    object_->transform->SetLocalPosition(localPosition_);
    object_->transform->SetLocalScale(localScale_);

    DirectX::XMFLOAT3 normal_ = request_.normal;
    DirectX::XMVECTOR normalVector_ = DirectX::XMLoadFloat3(&normal_);
    if (DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(normalVector_)) <= 0.000001f)
    {
        normal_ = { 0.0f, 1.0f, 0.0f };
    }
    const float yaw_ = DirectX::XMConvertToDegrees(std::atan2(normal_.x, normal_.z));
    const float horizontalLength_ = std::sqrt(
        normal_.x * normal_.x + normal_.z * normal_.z);
    const float pitch_ = DirectX::XMConvertToDegrees(
        std::atan2(-normal_.y, horizontalLength_));
    object_->transform->SetRotation(Vector3(pitch_, yaw_, 0.0f));
    object_->UpdateTransform();

    const std::shared_ptr<DecalComponent> decal_ =
        object_->AddComponent<DecalComponent>();
    if (!decal_)
    {
        gameObjectManager->Destroy(object_);
        return nullptr;
    }
    decal_->color = request_.color;
    decal_->SetTexture(request_.texturePath);
    if (request_.groundSurface) decal_->SetGroundSurface(request_.emissiveStrength, request_.groundMaskPath);
    decal_->SetLifeTime(request_.lifeTime, request_.fadeOutDuration);
    decal_->SetDestroyOnLifeEnd(true);
    return object_;
}

void EffectManager::SpawnGroundMark(GroundMarkShape shape_, const DirectX::XMFLOAT3& center_,
    float halfWidth_, float halfLength_, float yaw_)
{
    BS_PROFILE_SCOPE("EffectManager::SpawnGroundMark");
    auto* scene_ = PhysXSystem::GetScene();
    if (!scene_ || halfWidth_ <= 0.0f || halfLength_ <= 0.0f) return;

    physx::PxRaycastBuffer ground_;
    physx::PxQueryFilterData filter_;
    filter_.data.word0 = static_cast<physx::PxU32>(LayerType::Terrain) |
        static_cast<physx::PxU32>(LayerType::Environment);
    filter_.flags = physx::PxQueryFlag::eSTATIC | physx::PxQueryFlag::eDYNAMIC;
    if (!scene_->raycast({ center_.x, center_.y + 2.0f, center_.z }, { 0.0f, -1.0f, 0.0f },
        32.0f, ground_, physx::PxHitFlag::eDEFAULT, filter_) ||
        !ground_.hasBlock || ground_.block.normal.y < 0.5f) return;

    using namespace DirectX;
    const XMFLOAT3 position_{ ground_.block.position.x, ground_.block.position.y, ground_.block.position.z };
    const XMFLOAT3 normal_{ ground_.block.normal.x, ground_.block.normal.y, ground_.block.normal.z };
    const auto up_ = XMLoadFloat3(&normal_);
    const auto heading_ = XMVectorSet(std::sin(yaw_), 0.0f, std::cos(yaw_), 0.0f);
    const auto right_ = XMVector3Normalize(XMVector3Cross(up_, heading_));
    const auto forward_ = XMVector3Cross(right_, up_);
    const XMMATRIX surface_(right_, up_, forward_, XMVectorSet(position_.x, position_.y, position_.z, 1.0f));
    // デカールはローカルXYで投影するため、画像の上を攻撃の前方へ向ける。
    const XMMATRIX projection_(right_, forward_, -up_, surface_.r[3]);

    constexpr float GROUND_MARK_LIFETIME = 8.0f;
    DecalSpawnRequest decal_;
    decal_.position = position_;
    decal_.normal = normal_;
    decal_.scale = { halfWidth_, halfLength_, 0.9f };
    decal_.normalOffset = 0.0f;
    decal_.groundSurface = true;
    decal_.lifeTime = GROUND_MARK_LIFETIME;
    decal_.fadeOutDuration = 1.25f;
    const auto placeDecal_ = [&]()
    {
        if (const auto object_ = SpawnDecal(decal_))
        {
            object_->transform->SetRotation(Vector3(0.0f, 0.0f, 0.0f));
            object_->transform->localPosition = {};
            object_->transform->SetParentOverride(true, projection_);
            object_->UpdateTransform();
        }
    };

    const char* scorch_ = "Data/Texture/Decal/ScorchRound.png";
    if (shape_ == GroundMarkShape::FIRE_BEAM) scorch_ = "Data/Texture/Decal/ScorchBeam.png";
    if (shape_ == GroundMarkShape::FIRE_FAN) scorch_ = "Data/Texture/Decal/ScorchFan.png";
    if (shape_ == GroundMarkShape::LIGHTNING_STRIP) scorch_ = "Data/Texture/Decal/ScorchLightning.png";

    decal_.texturePath = "Data/Texture/Decal/GroundCrater.png";
    decal_.groundMaskPath = scorch_;
    decal_.color.w = 0.72f;
    placeDecal_();

    decal_.texturePath = scorch_;
    decal_.groundMaskPath.clear();
    decal_.color.w = shape_ == GroundMarkShape::IMPACT ? 0.42f : 0.88f;
    placeDecal_();

    if (shape_ != GroundMarkShape::IMPACT)
    {
        const bool lightning_ = shape_ == GroundMarkShape::LIGHTNING_STRIP;
        decal_.texturePath = lightning_ ? "Data/Texture/Decal/ScorchViolet.png" : "Data/Texture/Decal/ScorchLava.png";
        decal_.groundMaskPath = scorch_;
        decal_.color.w = 0.8f;
        decal_.emissiveStrength = lightning_ ? 1.35f : 1.75f;
        decal_.lifeTime = lightning_ ? 4.0f : 2.8f;
        decal_.fadeOutDuration = 1.3f;
        placeDecal_();

        if (!lightning_)
        {
            ParticleSpawnRequest embers_;
            embers_.assetPath = "DragonBreathProjectile_TrailResidue";
            embers_.normal = normal_;
            embers_.normalOffset = 0.18f;
            embers_.lifeTime = 1.0f;
            const int emberCount_ = shape_ == GroundMarkShape::FIRE_IMPACT ? 1 : 3;
            for (int index_ = 0; index_ < emberCount_; ++index_)
            {
                const float across_ = shape_ == GroundMarkShape::FIRE_FAN
                    ? (index_ - 1) * halfWidth_ * 0.46f : 0.0f;
                const float along_ = shape_ == GroundMarkShape::FIRE_BEAM
                    ? (index_ - 1) * halfLength_ * 0.55f
                    : shape_ == GroundMarkShape::FIRE_FAN ? halfLength_ * 0.28f : 0.0f;
                const auto sample_ = XMVectorMultiplyAdd(forward_, XMVectorReplicate(along_),
                    XMVectorMultiplyAdd(right_, XMVectorReplicate(across_), surface_.r[3]));
                XMStoreFloat3(&embers_.position, sample_);
                SpawnParticle(embers_);
            }
        }
    }

    MeshVFXSpawnRequest rim_;
    rim_.assetPath = shape_ == GroundMarkShape::FIRE_FAN ? "GroundScarRimFan" :
        (shape_ == GroundMarkShape::FIRE_BEAM || shape_ == GroundMarkShape::LIGHTNING_STRIP) ?
        "GroundScarRimStrip" : "GroundScarRim";
    rim_.position = position_;
    rim_.lifeTime = GROUND_MARK_LIFETIME;
    if (const auto object_ = SpawnMeshVFX(rim_))
    {
        const float rise_ = std::clamp(halfWidth_ * 0.065f, 0.12f, 0.4f);
        object_->transform->localPosition = {};
        object_->transform->localScale = { halfWidth_, rise_, halfLength_ };
        object_->transform->SetParentOverride(true, surface_);
        object_->UpdateTransform();
    }
}

// 指定された有効フラグに従ってParticle、MeshVFX、Decalをまとめて生成する
void EffectManager::SpawnImpactEffect(const ImpactEffectSpawnRequest& request_)
{
    if (request_.spawnParticle && !request_.particle.assetPath.empty())
    {
        ParticleSpawnRequest particle_ = request_.particle;
        particle_.position = request_.position;
        particle_.normal = request_.normal;
        RegisterAttackEffectOwner(SpawnParticle(particle_), request_.attackOwner.lock());
    }
    if (request_.spawnMeshVFX && !request_.meshVFX.assetPath.empty())
    {
        MeshVFXSpawnRequest meshVFX_ = request_.meshVFX;
        meshVFX_.position = request_.position;
        meshVFX_.normal = request_.normal;
        RegisterAttackEffectOwner(SpawnMeshVFX(meshVFX_), request_.attackOwner.lock());
    }
    if (request_.spawnDecal && !request_.decal.texturePath.empty())
    {
        DecalSpawnRequest decal_ = request_.decal;
        decal_.position = request_.position;
        decal_.normal = request_.normal;
        RegisterAttackEffectOwner(SpawnDecal(decal_), request_.attackOwner.lock());
    }
}

// Impact Effect Assetを読み込み済みCacheから取得し、ヒット位置へ生成する
void EffectManager::SpawnImpactEffectAsset(
    const std::string& assetNameOrPath_,
    const DirectX::XMFLOAT3& position_,
    const DirectX::XMFLOAT3& normal_,
    const std::shared_ptr<GameObject>& attackOwner_)
{
    const std::string resolvedPath_ = ResolveImpactEffectPath(assetNameOrPath_);
    const std::string assetKey_ = BuildAssetNameFromPath(
        resolvedPath_,
        "Data/ImpactEffect");
    const auto iterator_ = impactEffectAssetCache.find(assetKey_);
    if (iterator_ == impactEffectAssetCache.end())
    {
        std::printf(
            "[EffectManager] Impact effect cache miss: %s\n",
            assetNameOrPath_.c_str());
        return;
    }

    for (const ImpactEffectEntry& entry_ : iterator_->second.entries)
    {
        if (!entry_.enabled || entry_.assetPath.empty()) continue;
        ImpactEffectSpawnRequest request_{};
        request_.attackOwner = attackOwner_;
        request_.position = position_;
        request_.normal = normal_;
        if (entry_.type == IMPACT_EFFECT_ENTRY_TYPE::PARTICLE)
        {
            request_.spawnParticle = true;
            request_.particle.assetPath = entry_.assetPath;
            request_.particle.emitterName = entry_.emitterName;
            request_.particle.lifeTime = entry_.lifeTime;
            request_.particle.sizeScale = (std::max)(0.01f, entry_.scale.x);
            request_.particle.normalOffset = entry_.normalOffset;
        }
        else if (entry_.type == IMPACT_EFFECT_ENTRY_TYPE::MESH_VFX)
        {
            request_.spawnMeshVFX = true;
            request_.meshVFX.assetPath = entry_.assetPath;
            request_.meshVFX.scale = entry_.scale;
            request_.meshVFX.lifeTime = entry_.lifeTime;
            request_.meshVFX.normalOffset = entry_.normalOffset;
        }
        else
        {
            request_.spawnDecal = true;
            request_.decal.texturePath = entry_.assetPath;
            request_.decal.scale = entry_.scale;
            request_.decal.color = entry_.color;
            request_.decal.lifeTime = entry_.lifeTime;
            request_.decal.fadeOutDuration = entry_.fadeOutDuration;
            request_.decal.normalOffset = entry_.normalOffset;
        }
        SpawnImpactEffect(request_);
    }
}

// 管理中Effectを停止し、Pool対象の場合は再利用状態へ戻す
bool EffectManager::IsEffectOwnedBy(const std::shared_ptr<GameObject>& object_, const Component* lifetimeOwner_) const
{
    return object_ && lifetimeOwner_ && std::any_of(managedEffects.begin(), managedEffects.end(),
        [&object_, lifetimeOwner_](const ManagedEffect& effect_)
        {
            return effect_.object.lock() == object_ && effect_.lifetimeOwner.lock().get() == lifetimeOwner_;
        });
}

void EffectManager::RegisterAttackEffectOwner(const std::shared_ptr<GameObject>& object_, const std::shared_ptr<GameObject>& owner_)
{
    // Animation・Sequence・独立判定・VFX寿命を同じ時計倍率へそろえる。
    if (object_) object_->SetRuntimePlaybackRate(owner_ ? owner_->GetRuntimePlaybackRate() : 1.0f);
    if (!object_ || !owner_) return;
    for (auto& effect_ : managedEffects)
        if (effect_.object.lock() == object_) { effect_.attackOwner = owner_; return; }
}

void EffectManager::StopAttackEffects(const std::shared_ptr<GameObject>& owner_,
    const std::vector<std::weak_ptr<GameObject>>& preservedEffects_)
{
    BS_PROFILE_SCOPE("EffectManager::StopAttackEffects");
    if (!owner_ || !gameObjectManager) return;
    scheduledAttackEffects.erase(std::remove_if(scheduledAttackEffects.begin(), scheduledAttackEffects.end(),
        [&owner_](const ScheduledAttackEffect& effect_) { return effect_.request.ownerObject == owner_.get(); }), scheduledAttackEffects.end());
    std::vector<std::shared_ptr<GameObject>> managedObjects_;
    std::vector<std::shared_ptr<GameObject>> attackObjects_;
    // 独立して発射を完了する弾は、見た目と判定を同じObject単位で停止対象から外す。
    const auto isPreserved_ = [&preservedEffects_](const std::shared_ptr<GameObject>& object_)
    {
        return object_ && std::any_of(preservedEffects_.begin(), preservedEffects_.end(),
            [&object_](const std::weak_ptr<GameObject>& effect_) { return effect_.lock() == object_; });
    };
    for (const auto& effect_ : managedEffects)
    {
        const auto object_ = effect_.object.lock();
        if (isPreserved_(object_)) continue;
        const auto hitBox_ = effect_.hitBox.lock();
        const auto attack_ = object_ ? object_->GetComponent<AttackEffectHitComponent>() : nullptr;
        if (object_ && (effect_.attackOwner.lock() == owner_ || (hitBox_ && hitBox_->GetAttackOwner() == owner_) || (attack_ && attack_->IsOwnedBy(owner_.get()))))
            managedObjects_.push_back(object_);
    }
    for (const auto& object_ : gameObjectManager->GetObjects())
    {
        if (isPreserved_(object_)) continue;
        const auto attack_ = object_ ? object_->GetComponent<AttackEffectHitComponent>() : nullptr;
        if (!attack_ || !attack_->IsOwnedBy(owner_.get())) continue;
        attack_->Stop();
        if (std::find(managedObjects_.begin(), managedObjects_.end(), object_) == managedObjects_.end()) attackObjects_.push_back(object_);
    }
    // 検索中にはManagerの配列を変更せず、Pool返却と通常Object削除を分けて行う。
    for (const auto& object_ : managedObjects_) StopEffect(object_);
    for (const auto& object_ : attackObjects_) gameObjectManager->Destroy(object_);
}

void EffectManager::StopEffect(const std::shared_ptr<GameObject>& object_, const Component* lifetimeOwner_)
{
    if (!object_) return;

    const auto iterator_ = std::find_if(
        managedEffects.begin(),
        managedEffects.end(),
        [&object_, lifetimeOwner_](const ManagedEffect& effect_)
        {
            return effect_.object.lock() == object_ &&
                (!lifetimeOwner_ || effect_.lifetimeOwner.lock().get() == lifetimeOwner_);
        });
    if (iterator_ == managedEffects.end()) return;

    if (iterator_->poolType != EFFECT_POOL_TYPE::NONE)
    {
        ReleasePooledEffect(*iterator_);
    }
    else if (gameObjectManager)
    {
        gameObjectManager->Destroy(object_);
    }
    managedEffects.erase(iterator_);
}

// 位置を法線方向へずらし、Effectが面から浮くようにする
DirectX::XMFLOAT3 EffectManager::ApplyNormalOffset(
    const DirectX::XMFLOAT3& position_,
    const DirectX::XMFLOAT3& normal_,
    float normalOffset_) const
{
    using namespace DirectX;

    XMVECTOR positionVector_ = XMLoadFloat3(&position_);
    XMVECTOR normalVector_ = XMLoadFloat3(&normal_);
    if (XMVectorGetX(XMVector3LengthSq(normalVector_)) <= 0.000001f)
    {
        normalVector_ = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    }
    normalVector_ = XMVector3Normalize(normalVector_);
    positionVector_ = XMVectorAdd(
        positionVector_,
        XMVectorScale(normalVector_, normalOffset_));

    XMFLOAT3 result_{};
    XMStoreFloat3(&result_, positionVector_);
    return result_;
}

// 生成済みEffectを寿命、HitBox時間、Pool情報と一緒に管理リストへ登録する
void EffectManager::RegisterManagedEffect(
    const std::shared_ptr<GameObject>& object_,
    float lifeTime_,
    const std::shared_ptr<HitBox>& hitBox_,
    float hitStartTime_,
    float hitEndTime_,
    EFFECT_POOL_TYPE poolType_,
    const std::string& assetPath_)
{
    if (!object_) return;

    ManagedEffect managedEffect_{};
    managedEffect_.object = object_;
    managedEffect_.hitBox = hitBox_;
    managedEffect_.assetPath = assetPath_;
    managedEffect_.poolType = poolType_;
    managedEffect_.lifeTimer = (std::max)(lifeTime_, 0.0f);
    if (managedEffect_.lifeTimer <= 0.0f)
    {
        managedEffect_.lifeTimer = 1.0f;
    }
    managedEffect_.hitStartTime = (std::max)(hitStartTime_, 0.0f);
    managedEffect_.hitEndTime = (std::max)(hitEndTime_, managedEffect_.hitStartTime);
    managedEffects.push_back(managedEffect_);
}

// Particle用GameObjectをPoolから取り出す
std::shared_ptr<GameObject> EffectManager::AcquireParticleObject(
    const std::string& resolvedAssetPath_)
{
    const std::string assetName_ = BuildAssetNameFromPath(
        resolvedAssetPath_,
        "Data/Particle");
    // 起動後に追加された演出Assetも生成できるようにする。初期一覧に無いだけで
    // 「Pool枯渇」として生成を諦めると、呼出側でPlayしても描画対象自体が存在しない。
    if (particleAssetCache.find(assetName_) == particleAssetCache.end())
    {
        PreloadParticleAsset(resolvedAssetPath_);
        if (particleAssetCache.find(assetName_) == particleAssetCache.end()) return nullptr;
    }
    std::vector<std::shared_ptr<GameObject>>& objects_ =
        particleObjectPool[assetName_];
    for (const std::shared_ptr<GameObject>& object_ : objects_)
    {
        const std::shared_ptr<ParticleComponent> particle_ =
            object_ ? object_->GetComponent<ParticleComponent>() : nullptr;
        if (particle_ && !particle_->IsEnabled())
        {
            gameObjectManager->AddObject(object_);
            return object_;
        }
    }

    if (objects_.size() < EFFECT_DYNAMIC_POOL_MAX_COUNT)
    {
        const auto assetIterator_ = particleAssetCache.find(assetName_);
        if (assetIterator_ != particleAssetCache.end())
        {
            std::shared_ptr<GameObject> object_ =
                CreateParticlePoolObject(resolvedAssetPath_, assetIterator_->second);
            if (object_)
            {
                objects_.push_back(object_);
                gameObjectManager->AddObject(object_);
                return object_;
            }
        }
    }

    std::printf(
        "[EffectManager] Particle pool exhausted: %s size=%zu\n",
        resolvedAssetPath_.c_str(),
        objects_.size());
    return nullptr;
}

// MeshVFX用GameObjectをPoolから取り出す
std::shared_ptr<GameObject> EffectManager::AcquireMeshVFXObject(
    const std::string& resolvedAssetPath_)
{
    const std::string assetName_ = BuildAssetNameFromPath(
        resolvedAssetPath_,
        "Data/VFX");
    std::vector<std::shared_ptr<GameObject>>& objects_ =
        meshVFXObjectPool[assetName_];
    for (const std::shared_ptr<GameObject>& object_ : objects_)
    {
        const std::shared_ptr<MeshVFXComponent> meshVFX_ =
            object_ ? object_->GetComponent<MeshVFXComponent>() : nullptr;
        if (meshVFX_ && !meshVFX_->IsEnabled())
        {
            gameObjectManager->AddObject(object_);
            return object_;
        }
    }

    if (objects_.size() < EFFECT_DYNAMIC_POOL_MAX_COUNT)
    {
        const auto assetIterator_ = meshVFXAssetCache.find(assetName_);
        if (assetIterator_ != meshVFXAssetCache.end())
        {
            std::shared_ptr<GameObject> object_ =
                CreateMeshVFXPoolObject(resolvedAssetPath_, assetIterator_->second);
            if (object_)
            {
                objects_.push_back(object_);
                gameObjectManager->AddObject(object_);
                return object_;
            }
        }
    }

    std::printf(
        "[EffectManager] MeshVFX pool exhausted: %s size=%zu\n",
        resolvedAssetPath_.c_str(),
        objects_.size());
    return nullptr;
}

// ParticleComponentにAssetを設定した非保存GameObjectをPool用に作成する
std::shared_ptr<GameObject> EffectManager::CreateParticlePoolObject(
    const std::string& resolvedAssetPath_,
    const ParticleEffectAsset& asset_)
{
    (void)resolvedAssetPath_;
    if (!gameObjectManager) return nullptr;
    std::shared_ptr<GameObject> object_ =
        gameObjectManager->Create("PooledParticle");
    if (!object_) return nullptr;
    object_->SetNoSaveObject();

    std::shared_ptr<ParticleComponent> particle_ =
        object_->AddComponent<ParticleComponent>();
    if (!particle_) return nullptr;
    particle_->SetAsset(asset_);
    particle_->ResetRuntime();
    if (!particle_->HasRuntimeResources())
    {
        std::printf(
            "[EffectManager] Particle pool resource build failed: %s\n",
            resolvedAssetPath_.c_str());
        gameObjectManager->Destroy(object_);
        return nullptr;
    }
    particle_->SetEnabled(false);
    gameObjectManager->DetachObject(object_);
    return object_;
}

// MeshVFXComponentにAssetを設定した非保存GameObjectをPool用に作成する
std::shared_ptr<GameObject> EffectManager::CreateMeshVFXPoolObject(
    const std::string& resolvedAssetPath_,
    const MeshVFXAsset& asset_)
{
    (void)resolvedAssetPath_;
    if (!gameObjectManager) return nullptr;
    std::shared_ptr<GameObject> object_ =
        gameObjectManager->Create("PooledMeshVFX");
    if (!object_) return nullptr;
    object_->SetNoSaveObject();

    std::shared_ptr<MeshVFXComponent> meshVFX_ =
        object_->AddComponent<MeshVFXComponent>();
    if (!meshVFX_) return nullptr;
    meshVFX_->SetAsset(asset_);
    if (!meshVFX_->HasRuntimeResources())
    {
        std::printf(
            "[EffectManager] MeshVFX pool resource build failed: %s\n",
            resolvedAssetPath_.c_str());
        gameObjectManager->Destroy(object_);
        return nullptr;
    }
    meshVFX_->Stop();
    meshVFX_->Reset();
    meshVFX_->SetEnabled(false);
    gameObjectManager->DetachObject(object_);
    return object_;
}

// VFX Sequence用GameObjectをPoolから取り出す
std::shared_ptr<GameObject> EffectManager::AcquireVFXSequenceObject(
    const std::string& resolvedAssetPath_)
{
    const std::string assetName_ = BuildAssetNameFromPath(
        resolvedAssetPath_,
        "Data/VFXSequence");
    std::vector<std::shared_ptr<GameObject>>& objects_ =
        vfxSequenceObjectPool[assetName_];
    for (const std::shared_ptr<GameObject>& object_ : objects_)
    {
        if (!object_) continue;
        const std::shared_ptr<ParticleComponent> particle_ =
            object_->GetComponent<ParticleComponent>();
        const std::shared_ptr<MeshVFXComponent> meshVFX_ =
            object_->GetComponent<MeshVFXComponent>();
        const bool particleIdle_ = !particle_ || !particle_->IsEnabled();
        const bool meshIdle_ = !meshVFX_ || !meshVFX_->IsEnabled();
        if (particleIdle_ && meshIdle_)
        {
            gameObjectManager->AddObject(object_);
            return object_;
        }
    }

    if (objects_.size() < EFFECT_DYNAMIC_POOL_MAX_COUNT)
    {
        const auto assetIterator_ = vfxSequenceAssetCache.find(assetName_);
        if (assetIterator_ != vfxSequenceAssetCache.end())
        {
            std::shared_ptr<GameObject> object_ =
                CreateVFXSequencePoolObject(resolvedAssetPath_, assetIterator_->second);
            if (object_)
            {
                objects_.push_back(object_);
                gameObjectManager->AddObject(object_);
                return object_;
            }
        }
    }

    std::printf(
        "[EffectManager] VFX sequence pool exhausted: %s size=%zu\n",
        resolvedAssetPath_.c_str(),
        objects_.size());
    return nullptr;
}

// VFX Sequence用にParticleComponentとMeshVFXComponentを持つGameObjectを作成する
std::shared_ptr<GameObject> EffectManager::CreateVFXSequencePoolObject(
    const std::string& resolvedAssetPath_,
    const VFXSequenceAsset& asset_)
{
    if (!gameObjectManager) return nullptr;
    std::shared_ptr<GameObject> object_ =
        gameObjectManager->Create("PooledVFXSequence");
    if (!object_) return nullptr;
    object_->SetNoSaveObject();

    if (asset_.hasParticle)
    {
        const std::shared_ptr<ParticleComponent> particle_ =
            object_->AddComponent<ParticleComponent>();
        if (!particle_)
        {
            gameObjectManager->Destroy(object_);
            return nullptr;
        }
        particle_->SetAsset(asset_.particleAsset);
        particle_->ResetRuntime();
        if (!particle_->HasRuntimeResources())
        {
            std::printf(
                "[EffectManager] VFX sequence particle build failed: %s\n",
                resolvedAssetPath_.c_str());
            gameObjectManager->Destroy(object_);
            return nullptr;
        }
        particle_->SetEnabled(false);
    }

    if (asset_.hasMeshVFX)
    {
        const std::shared_ptr<MeshVFXComponent> meshVFX_ =
            object_->AddComponent<MeshVFXComponent>();
        if (!meshVFX_)
        {
            gameObjectManager->Destroy(object_);
            return nullptr;
        }
        meshVFX_->SetAsset(asset_.meshVFXAsset);
        meshVFX_->Stop();
        meshVFX_->Reset();
        if (!meshVFX_->HasRuntimeResources())
        {
            std::printf(
                "[EffectManager] VFX sequence mesh build failed: %s\n",
                resolvedAssetPath_.c_str());
            gameObjectManager->Destroy(object_);
            return nullptr;
        }
        meshVFX_->SetEnabled(false);
    }

    gameObjectManager->DetachObject(object_);
    return object_;
}

// VFX Sequenceを生成位置へ配置し、Track情報を寿命管理へ登録する
std::shared_ptr<GameObject> EffectManager::SpawnVFXSequence(
    const MeshVFXSpawnRequest& request_,
    const std::string& resolvedAssetPath_)
{
    const std::string assetName_ = BuildAssetNameFromPath(
        resolvedAssetPath_,
        "Data/VFXSequence");
    const auto assetIterator_ = vfxSequenceAssetCache.find(assetName_);
    if (assetIterator_ == vfxSequenceAssetCache.end()) return nullptr;

    std::shared_ptr<GameObject> object_ =
        AcquireVFXSequenceObject(resolvedAssetPath_);
    if (!object_) return nullptr;

    const DirectX::XMFLOAT3 spawnPosition_ = ApplyNormalOffset(
        request_.position,
        request_.normal,
        request_.normalOffset);
    Vector3 position_(spawnPosition_.x, spawnPosition_.y, spawnPosition_.z);
    object_->transform->SetLocalPosition(position_);
    object_->UpdateTransform();

    if (const std::shared_ptr<ParticleComponent> particle_ =
        object_->GetComponent<ParticleComponent>())
    {
        particle_->ResetRuntime();
        particle_->Stop();
        particle_->SetEnabled(true);
    }
    if (const std::shared_ptr<MeshVFXComponent> meshVFX_ =
        object_->GetComponent<MeshVFXComponent>())
    {
        meshVFX_->Stop();
        meshVFX_->Reset();
        meshVFX_->SetEnabled(true);
    }

    const VFXSequenceAsset& asset_ = assetIterator_->second;
    ManagedEffect effect_{};
    effect_.object = object_;
    effect_.assetPath = resolvedAssetPath_;
    effect_.poolType = EFFECT_POOL_TYPE::VFX_SEQUENCE;
    effect_.sequenceDuration = asset_.duration;
    effect_.sequenceLoop = asset_.loop;
    effect_.sequenceTracks = asset_.tracks;
    effect_.lifeTimer = request_.lifeTime > 0.0f
        ? request_.lifeTime
        : asset_.duration;
    effect_.lifeTimer = (std::max)(effect_.lifeTimer, 0.001f);
    managedEffects.push_back(std::move(effect_));
    UpdateVFXSequence(managedEffects.back());
    return object_;
}

// Sequence時間に入ったTrackを再生し、時間外になったParticle Trackを停止する
void EffectManager::UpdateVFXSequence(ManagedEffect& effect_)
{
    const std::shared_ptr<GameObject> object_ = effect_.object.lock();
    if (!object_ || effect_.sequenceDuration <= 0.0f) return;

    if (effect_.sequenceLoop && effect_.sequenceTime >= effect_.sequenceDuration)
    {
        effect_.sequenceTime = std::fmod(
            effect_.sequenceTime,
            effect_.sequenceDuration);
        for (VFXSequenceTrack& track_ : effect_.sequenceTracks) track_.active = false;
        if (const std::shared_ptr<ParticleComponent> particle_ =
            object_->GetComponent<ParticleComponent>())
        {
            particle_->Stop();
            particle_->ResetRuntime();
        }
        if (const std::shared_ptr<MeshVFXComponent> meshVFX_ =
            object_->GetComponent<MeshVFXComponent>())
        {
            meshVFX_->Stop();
            meshVFX_->Reset();
        }
    }

    // 各Trackの開始時間とDurationを使用して再生状態を切り替える
    bool meshTrackActive_ = false;
    for (VFXSequenceTrack& track_ : effect_.sequenceTracks)
    {
        const bool shouldBeActive_ =
            effect_.sequenceTime >= track_.startTime &&
            effect_.sequenceTime < track_.startTime + track_.duration;
        if (track_.type == 1 && shouldBeActive_) meshTrackActive_ = true;

        if (shouldBeActive_ && !track_.active)
        {
            track_.active = true;
            if (track_.type == 0)
            {
                if (const std::shared_ptr<ParticleComponent> particle_ =
                    object_->GetComponent<ParticleComponent>())
                {
                    if (track_.targetName.empty() ||
                        !particle_->TriggerEmitter(track_.targetName))
                    {
                        particle_->Play();
                    }
                }
            }
            else if (track_.type == 1)
            {
                if (const std::shared_ptr<MeshVFXComponent> meshVFX_ =
                    object_->GetComponent<MeshVFXComponent>())
                {
                    meshVFX_->Reset();
                    meshVFX_->Play();
                }
            }
        }
        else if (!shouldBeActive_ && track_.active)
        {
            track_.active = false;
            if (track_.type == 0)
            {
                if (const std::shared_ptr<ParticleComponent> particle_ =
                    object_->GetComponent<ParticleComponent>())
                {
                    if (track_.targetName.empty()) particle_->Stop();
                    else particle_->StopEmitter(track_.targetName);
                }
            }
        }
    }

    if (!meshTrackActive_)
    {
        if (const std::shared_ptr<MeshVFXComponent> meshVFX_ =
            object_->GetComponent<MeshVFXComponent>())
        {
            meshVFX_->Stop();
        }
    }
}

// Poolへ戻すEffectのComponentとTransformを初期状態へ戻す
void EffectManager::ReleasePooledEffect(ManagedEffect& effect_)
{
    const std::shared_ptr<GameObject> object_ = effect_.object.lock();
    if (!object_) return;
    object_->SetRuntimePlaybackRate(1.0f);

    if (const std::shared_ptr<ParticleComponent> particle_ =
        object_->GetComponent<ParticleComponent>())
    {
        particle_->Stop();
        particle_->ResetRuntime();
        particle_->SetEnabled(false);
    }
    if (const std::shared_ptr<MeshVFXComponent> meshVFX_ =
        object_->GetComponent<MeshVFXComponent>())
    {
        meshVFX_->Stop();
        meshVFX_->Reset();
        meshVFX_->SetEnabled(false);
    }
    if (const std::shared_ptr<HitBox> hitBox_ = object_->GetComponent<HitBox>())
    {
        hitBox_->Deactivate();
        hitBox_->SetEnabled(false);
    }
    if (const std::shared_ptr<ModelAttachmentComponent> attachment_ =
        object_->GetComponent<ModelAttachmentComponent>())
    {
        attachment_->SetEnabled(false);
    }
    if (object_->transform)
    {
        object_->transform->SetParent(nullptr);
        object_->transform->SetParentOverride(false, DirectX::XMMatrixIdentity());
        object_->transform->localPosition = { 0.0f, 0.0f, 0.0f };
        object_->transform->localRotation = { 0.0f, 0.0f, 0.0f };
        object_->transform->localScale = { 1.0f, 1.0f, 1.0f };
        object_->transform->MarkDirty();
    }
    if (gameObjectManager) gameObjectManager->DetachObject(object_);
}

// 指定フォルダ以下のJSONファイルをAsset候補として収集する
void EffectManager::CollectJsonAssets(
    const std::string& rootPath_,
    std::vector<std::string>& paths_) const
{
    paths_.clear();

    const std::filesystem::path rootDirectory_ =
        WindowsDialogHelper::Utf8ToPath(rootPath_);
    std::error_code error_;
    if (!std::filesystem::is_directory(rootDirectory_, error_)) return;

    for (const std::filesystem::directory_entry& entry_ :
        std::filesystem::recursive_directory_iterator(rootDirectory_, error_))
    {
        if (error_) break;
        if (!entry_.is_regular_file(error_)) continue;
        if (entry_.path().extension() != ".json") continue;

        paths_.push_back(
            WindowsDialogHelper::PathToUtf8(entry_.path().lexically_normal()));
    }

    std::sort(paths_.begin(), paths_.end());
}

// Asset一覧表示用Entryと、複数形式の名前から引けるPath Mapを構築する
void EffectManager::BuildAssetNameMap(
    const std::vector<std::string>& paths_,
    const std::string& rootPath_,
    std::vector<EffectAssetEntry>& entries_,
    std::vector<std::string>& keys_,
    std::unordered_map<std::string, std::string>& pathByKey_) const
{
    entries_.clear();
    keys_.clear();
    pathByKey_.clear();

    for (const std::string& fullPath_ : paths_)
    {
        const std::filesystem::path filePath_ =
            WindowsDialogHelper::Utf8ToPath(fullPath_);
        const std::string fileName_ =
            WindowsDialogHelper::PathToUtf8(filePath_.filename());
        const std::string fileKey_ =
            WindowsDialogHelper::PathToUtf8(filePath_.stem());
        const std::string relativeKey_ = BuildAssetNameFromPath(
            fullPath_,
            rootPath_);

        std::string key_ = fileKey_;
        const auto duplicateIterator_ = pathByKey_.find(key_);
        if (duplicateIterator_ != pathByKey_.end() &&
            duplicateIterator_->second != fullPath_)
        {
            std::printf(
                "[EffectManager] Duplicate effect key '%s': %s / %s\n",
                key_.c_str(),
                duplicateIterator_->second.c_str(),
                fullPath_.c_str());
            key_ = relativeKey_;
        }

        EffectAssetEntry entry_{};
        entry_.key = key_;
        entry_.fileName = fileName_;
        entry_.fullPath = fullPath_;
        entries_.push_back(entry_);
        keys_.push_back(key_);

        RegisterAssetKey(key_, fullPath_, pathByKey_);
        RegisterAssetKey(relativeKey_, fullPath_, pathByKey_);
        RegisterAssetKey(fileName_, fullPath_, pathByKey_);
        RegisterAssetKey(fullPath_, fullPath_, pathByKey_);
    }
}

// Asset解決用Mapにキーを登録し、重複時は既存の解決先を保持する
void EffectManager::RegisterAssetKey(
    const std::string& key_,
    const std::string& fullPath_,
    std::unordered_map<std::string, std::string>& pathByKey_) const
{
    if (key_.empty() || fullPath_.empty()) return;

    const auto iterator_ = pathByKey_.find(key_);
    if (iterator_ == pathByKey_.end())
    {
        pathByKey_[key_] = fullPath_;
        return;
    }

    if (iterator_->second != fullPath_)
    {
        std::printf(
            "[EffectManager] Effect key alias '%s' already points to %s, ignored %s\n",
            key_.c_str(),
            iterator_->second.c_str(),
            fullPath_.c_str());
    }
}

// Particle AssetをCacheへ読み込み、初期Poolを作成する
void EffectManager::PreloadParticleAsset(const std::string& path_)
{
    ParticleEffectAsset asset_{};
    if (!ParticleComponent::LoadAssetDataOnly(path_, asset_))
    {
        std::printf("[EffectManager] Failed to preload particle asset: %s\n", path_.c_str());
        return;
    }

    const std::string assetName_ = BuildAssetNameFromPath(path_, "Data/Particle");
    if (assetName_.empty()) return;

    particleAssetCache[assetName_] = asset_;
    std::vector<std::shared_ptr<GameObject>>& objects_ =
        particleObjectPool[assetName_];
    for (size_t index_ = 0; index_ < EFFECT_PREWARM_COUNT; ++index_)
    {
        std::shared_ptr<GameObject> object_ =
            CreateParticlePoolObject(path_, asset_);
        if (object_) objects_.push_back(object_);
    }
}

// MeshVFX AssetをCacheへ読み込み、初期Poolを作成する
void EffectManager::PreloadMeshVFXAsset(const std::string& path_)
{
    MeshVFXComponent component_;
    if (!component_.LoadAsset(path_))
    {
        std::printf("[EffectManager] Failed to preload mesh vfx asset: %s\n", path_.c_str());
        return;
    }

    const std::string assetName_ = BuildAssetNameFromPath(path_, "Data/VFX");
    if (assetName_.empty()) return;

    meshVFXAssetCache[assetName_] = component_.GetAsset();
    const MeshVFXAsset& asset_ = meshVFXAssetCache[assetName_];
    std::vector<std::shared_ptr<GameObject>>& objects_ =
        meshVFXObjectPool[assetName_];
    for (size_t index_ = 0; index_ < EFFECT_PREWARM_COUNT; ++index_)
    {
        std::shared_ptr<GameObject> object_ =
            CreateMeshVFXPoolObject(path_, asset_);
        if (object_) objects_.push_back(object_);
    }
}

// VFX Sequence Assetを読み込み、Track情報と初期Poolを作成する
void EffectManager::PreloadVFXSequenceAsset(const std::string& path_)
{
    std::ifstream file_(WindowsDialogHelper::Utf8ToPath(path_));
    if (!file_.is_open())
    {
        std::printf("[EffectManager] Failed to open VFX sequence: %s\n", path_.c_str());
        return;
    }

    nlohmann::json json_;
    try
    {
        file_ >> json_;
    }
    catch (const nlohmann::json::exception&)
    {
        std::printf("[EffectManager] Failed to parse VFX sequence: %s\n", path_.c_str());
        return;
    }
    if (json_.value("AssetType", std::string{}) != "VFXSequence") return;

    VFXSequenceAsset asset_{};
    asset_.duration = (std::max)(json_.value("Duration", 1.0f), 0.001f);
    asset_.loop = json_.value("Loop", false);

    // Sequence内に埋め込まれたParticleとMeshVFXの設定を復元する
    if (json_.contains("Particle") && json_["Particle"].is_object())
    {
        ParticleComponent component_;
        component_.Deserialize(json_["Particle"]);
        asset_.particleAsset = component_.GetAsset();
        asset_.hasParticle = !asset_.particleAsset.emitters.empty();
    }
    if (json_.contains("MeshVFX") && json_["MeshVFX"].is_object())
    {
        MeshVFXComponent component_;
        component_.Deserialize(json_["MeshVFX"]);
        asset_.meshVFXAsset = component_.GetAsset();
        asset_.hasMeshVFX = !asset_.meshVFXAsset.parts.empty();
    }
    if (json_.contains("Tracks") && json_["Tracks"].is_array())
    {
        // Editorで設定されたTrack時間をRuntime用Trackへ変換する
        for (const nlohmann::json& trackJson_ : json_["Tracks"])
        {
            VFXSequenceTrack track_{};
            track_.type = std::clamp(trackJson_.value("Type", 0), 0, 1);
            track_.targetName = trackJson_.value("Target", std::string{});
            track_.startTime = (std::max)(trackJson_.value("StartTime", 0.0f), 0.0f);
            track_.duration = (std::max)(trackJson_.value("Duration", 1.0f), 0.001f);
            asset_.tracks.push_back(std::move(track_));
        }
    }
    if (!asset_.hasParticle && !asset_.hasMeshVFX) return;
    if (asset_.tracks.empty())
    {
        // Track未設定のAssetは保持しているEffect種別を全時間再生対象にする
        if (asset_.hasParticle)
        {
            VFXSequenceTrack track_{};
            track_.type = 0;
            track_.duration = asset_.duration;
            asset_.tracks.push_back(track_);
        }
        if (asset_.hasMeshVFX)
        {
            VFXSequenceTrack track_{};
            track_.type = 1;
            track_.duration = asset_.duration;
            asset_.tracks.push_back(track_);
        }
    }

    const std::string assetName_ = BuildAssetNameFromPath(path_, "Data/VFXSequence");
    if (assetName_.empty()) return;
    vfxSequenceAssetCache[assetName_] = asset_;
    std::vector<std::shared_ptr<GameObject>>& objects_ =
        vfxSequenceObjectPool[assetName_];
    for (size_t index_ = 0; index_ < EFFECT_PREWARM_COUNT; ++index_)
    {
        std::shared_ptr<GameObject> object_ = CreateVFXSequencePoolObject(
            path_,
            asset_);
        if (object_) objects_.push_back(object_);
    }
}

// Impact Effect AssetをCacheへ読み込む
void EffectManager::PreloadImpactEffectAsset(const std::string& path_)
{
    std::ifstream file_(WindowsDialogHelper::Utf8ToPath(path_));
    if (!file_.is_open())
    {
        std::printf("[EffectManager] Failed to open impact effect: %s\n", path_.c_str());
        return;
    }
    try
    {
        nlohmann::json json_;
        file_ >> json_;
        if (json_.value("AssetType", std::string{}) != "ImpactEffect") return;
        const std::string assetKey_ = BuildAssetNameFromPath(path_, "Data/ImpactEffect");
        if (!assetKey_.empty()) impactEffectAssetCache[assetKey_] = json_.get<ImpactEffectAsset>();
    }
    catch (const nlohmann::json::exception&)
    {
        std::printf("[EffectManager] Failed to parse impact effect: %s\n", path_.c_str());
    }
}

// Data配下のAssetパスを名前解決に使う相対キーへ変換する
std::string EffectManager::BuildAssetNameFromPath(
    const std::string& path_,
    const std::string& rootPath_) const
{
    if (path_.empty()) return {};

    std::string normalizedPath_ = path_;
    std::replace(
        normalizedPath_.begin(),
        normalizedPath_.end(),
        '\\',
        '/');

    while (!normalizedPath_.empty() && normalizedPath_.front() == '/')
    {
        normalizedPath_.erase(normalizedPath_.begin());
    }

    std::string normalizedRootPath_ = rootPath_;
    std::replace(
        normalizedRootPath_.begin(),
        normalizedRootPath_.end(),
        '\\',
        '/');
    while (!normalizedRootPath_.empty() && normalizedRootPath_.front() == '/')
    {
        normalizedRootPath_.erase(normalizedRootPath_.begin());
    }

    const std::string rootPrefix_ = normalizedRootPath_ + "/";
    if (normalizedPath_.rfind(rootPrefix_, 0) == 0)
    {
        normalizedPath_.erase(0, rootPrefix_.size());
    }
    else
    {
        const std::string rootMarker_ = "/" + rootPrefix_;
        const size_t rootPosition_ = normalizedPath_.find(rootMarker_);
        if (rootPosition_ != std::string::npos)
        {
            normalizedPath_.erase(0, rootPosition_ + rootMarker_.size());
        }

        const std::string dataPrefix_ = "Data/";
        if (normalizedPath_.rfind(dataPrefix_, 0) == 0)
        {
            const size_t slashPosition_ = normalizedPath_.find('/', dataPrefix_.size());
            if (slashPosition_ != std::string::npos)
            {
                normalizedPath_.erase(0, slashPosition_ + 1);
            }
        }
    }

    const std::string extension_ = ".json";
    if (normalizedPath_.size() >= extension_.size())
    {
        std::string tail_ = normalizedPath_.substr(normalizedPath_.size() - extension_.size());
        std::transform(
            tail_.begin(),
            tail_.end(),
            tail_.begin(),
            [](unsigned char character_)
            {
                return static_cast<char>(std::tolower(character_));
            });

        if (tail_ == extension_)
        {
            normalizedPath_.erase(normalizedPath_.size() - extension_.size());
        }
    }

    return normalizedPath_;
}

const ParticleEffectAsset* EffectManager::FindParticleAssetFromRequest(
    const std::string& assetPath_) const
{
    const auto directIterator_ = particleAssetCache.find(assetPath_);
    if (directIterator_ != particleAssetCache.end()) return &directIterator_->second;

    const std::string assetName_ = BuildAssetNameFromPath(assetPath_, "Data/Particle");
    const auto iterator_ = particleAssetCache.find(assetName_);
    if (iterator_ == particleAssetCache.end()) return nullptr;

    return &iterator_->second;
}

const MeshVFXAsset* EffectManager::FindMeshVFXAssetFromRequest(
    const std::string& assetPath_) const
{
    const auto directIterator_ = meshVFXAssetCache.find(assetPath_);
    if (directIterator_ != meshVFXAssetCache.end()) return &directIterator_->second;

    const std::string assetName_ = BuildAssetNameFromPath(assetPath_, "Data/VFX");
    const auto iterator_ = meshVFXAssetCache.find(assetName_);
    if (iterator_ == meshVFXAssetCache.end()) return nullptr;

    return &iterator_->second;
}


// キャッシュ・Pool生成処理そのものは既存のPreload関数へ委譲する。
bool EffectManager::AdvancePreload(float budgetMilliseconds_)
{
    BS_PROFILE_SCOPE("EffectManager::AdvancePreload");
    const auto total_ = particleAssets.size() + meshVFXAssets.size() + vfxSequenceAssets.size() + impactEffectAssets.size();
    const auto start_ = std::chrono::steady_clock::now();
    while (incrementalPreloadCursor < total_)
    {
        std::size_t index_ = incrementalPreloadCursor++;
        if (index_ < particleAssets.size()) PreloadParticleAsset(particleAssets[index_].fullPath);
        else
        {
            index_ -= particleAssets.size();
            if (index_ < meshVFXAssets.size()) PreloadMeshVFXAsset(meshVFXAssets[index_].fullPath);
            else
            {
                index_ -= meshVFXAssets.size();
                if (index_ < vfxSequenceAssets.size()) PreloadVFXSequenceAsset(vfxSequenceAssets[index_].fullPath);
                else
                {
                    index_ -= vfxSequenceAssets.size();
                    PreloadImpactEffectAsset(impactEffectAssets[index_].fullPath);
                }
            }
        }
        if (std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - start_).count() >= budgetMilliseconds_)
            return incrementalPreloadCursor >= total_;
    }
    return true;
}

float EffectManager::GetPreloadProgress() const
{
    const auto total_ = particleAssets.size() + meshVFXAssets.size() + vfxSequenceAssets.size() + impactEffectAssets.size();
    return total_ == 0 ? 1.0f : (std::min)(1.0f, static_cast<float>(incrementalPreloadCursor) / static_cast<float>(total_));
}

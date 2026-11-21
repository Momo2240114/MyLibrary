#include "PrecompiledHeader.h"

#include "AttackEmitterComponent.h"

#include "Editor/AssetSlotGui.h"
#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Combat/ProjectileMotionComponent.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
#include "Engine/GamePlay/System/AttackSystem.h"
#include "Engine/GamePlay/System/EffectManager.h"
#include "Engine/Graphics/Particle/ParticleComponent.h"
#include "Engine/Graphics/Resource/GlTFModel.h"
#include "Engine/Graphics/VFX/MeshVFXComponent.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <filesystem>
#include <fstream>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <nlohmann/json.hpp>

namespace
{
    DirectX::XMFLOAT3 ReadVector3(
        const nlohmann::json& json_,
        const char* key_,
        const DirectX::XMFLOAT3& fallback_)
    {
        // JSON側のVector3は配列で保存されるため、形式が違う場合は既定値を使う。
        const nlohmann::json value_ = json_.value(key_, nlohmann::json::array());
        if (!value_.is_array() || value_.size() < 3u)
        {
            return fallback_;
        }
        return { value_[0].get<float>(), value_[1].get<float>(), value_[2].get<float>() };
    }

    nlohmann::json WriteVector3(const DirectX::XMFLOAT3& value_)
    {
        return { value_.x, value_.y, value_.z };
    }

    DirectX::XMFLOAT3 NormalizeDirection(const DirectX::XMFLOAT3& direction_)
    {
        // エフェクトの発生方向が未設定でも前方へ飛ぶよう、ゼロベクトルにはZ+を使う。
        DirectX::XMVECTOR vector_ = DirectX::XMLoadFloat3(&direction_);
        if (DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(vector_)) <= 0.000001f)
        {
            return { 0.0f, 0.0f, 1.0f };
        }
        DirectX::XMFLOAT3 result_{};
        DirectX::XMStoreFloat3(&result_, DirectX::XMVector3Normalize(vector_));
        return result_;
    }

    DirectX::XMFLOAT3 TransformForward(const DirectX::XMMATRIX& matrix_)
    {
        // 発生元行列の前方向を、ParticleやMeshVFXの法線/進行方向として使う。
        DirectX::XMFLOAT3 direction_{};
        DirectX::XMStoreFloat3(
            &direction_,
            DirectX::XMVector3TransformNormal(
                DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f),
                matrix_));
        return NormalizeDirection(direction_);
    }

    DirectX::XMFLOAT3 ExtractPosition(const DirectX::XMMATRIX& matrix_)
    {
        DirectX::XMFLOAT3 position_{};
        DirectX::XMStoreFloat3(&position_, matrix_.r[3]);
        return position_;
    }

    DirectX::XMMATRIX BuildDirectionMatrix(
        const DirectX::XMFLOAT3& position_,
        const DirectX::XMFLOAT3& direction_)
    {
        // 任意の発生位置から撃つProjectile用に、前方向ベクトルを姿勢行列へ変換する。
        const DirectX::XMFLOAT3 normalizedDirection_ = NormalizeDirection(direction_);
        const float yaw_ = std::atan2(normalizedDirection_.x, normalizedDirection_.z);
        const float horizontalLength_ = std::sqrt(
            normalizedDirection_.x * normalizedDirection_.x +
            normalizedDirection_.z * normalizedDirection_.z);
        const float pitch_ = -std::atan2(normalizedDirection_.y, horizontalLength_);
        return DirectX::XMMatrixRotationRollPitchYaw(pitch_, yaw_, 0.0f) *
            DirectX::XMMatrixTranslation(position_.x, position_.y, position_.z);
    }

    const char* GetEntryTypeName(ATTACK_EMITTER_ENTRY_TYPE type_)
    {
        static const char* names_[] = { "Particle", "Mesh VFX / Sequence", "Impact Effect" };
        return names_[std::clamp(static_cast<int>(type_), 0, 2)];
    }

    const char* GetShapeTypeName(ATTACK_EMITTER_SHAPE_TYPE type_)
    {
        static const char* names_[] = { "Area", "Projectile", "Beam", "Fan" };
        return names_[std::clamp(static_cast<int>(type_), 0, 3)];
    }
}

void to_json(nlohmann::json& json_, const AttackEmitterEntryTemplate& entry_)
{
    // 1つのEmitterテンプレート内に並べる、Particle/MeshVFX/ImpactEffectの個別設定を保存する。
    json_ =
    {
        { "Type", static_cast<int>(entry_.type) },
        { "Enabled", entry_.enabled },
        { "AssetPath", entry_.assetPath },
        { "EmitterName", entry_.emitterName },
        { "LocalOffset", WriteVector3(entry_.localOffset) },
        { "LocalRotation", WriteVector3(entry_.localRotation) },
        { "LocalScale", WriteVector3(entry_.localScale) },
        { "LifeTime", entry_.lifeTime },
        { "NormalOffset", entry_.normalOffset },
        { "PlaybackSpeed", entry_.playbackSpeed },
        { "FollowOwner", entry_.followOwner },
        { "OverrideMeshVFXColorRamp", entry_.overrideMeshVFXColorRamp },
        { "ColorRampTexturePath", entry_.colorRampTexturePath },
        { "ForceAdditiveBlend", entry_.forceAdditiveBlend },
        { "GlowMultiplier", entry_.glowMultiplier },
    };
}

void from_json(const nlohmann::json& json_, AttackEmitterEntryTemplate& entry_)
{
    // エントリは単体の寿命や再生速度を持つため、読み込み時に最低値を丸めて扱いやすくする。
    entry_.type = static_cast<ATTACK_EMITTER_ENTRY_TYPE>(std::clamp(json_.value("Type", 0), 0, 2));
    entry_.enabled = json_.value("Enabled", true);
    entry_.assetPath = json_.value("AssetPath", std::string{});
    entry_.emitterName = json_.value("EmitterName", std::string{});
    entry_.localOffset = ReadVector3(json_, "LocalOffset", entry_.localOffset);
    entry_.localRotation = ReadVector3(json_, "LocalRotation", entry_.localRotation);
    entry_.localScale = ReadVector3(json_, "LocalScale", entry_.localScale);
    entry_.lifeTime = (std::max)(json_.value("LifeTime", 1.0f), 0.01f);
    entry_.normalOffset = json_.value("NormalOffset", 0.0f);
    entry_.playbackSpeed = (std::max)(json_.value("PlaybackSpeed", 1.0f), 0.0f);
    entry_.followOwner = json_.value("FollowOwner", false);
    entry_.overrideMeshVFXColorRamp = json_.value("OverrideMeshVFXColorRamp", false);
    entry_.colorRampTexturePath = json_.value("ColorRampTexturePath", std::string{});
    entry_.forceAdditiveBlend = json_.value("ForceAdditiveBlend", false);
    entry_.glowMultiplier = (std::max)(json_.value("GlowMultiplier", 1.0f), 0.0f);
}

void to_json(nlohmann::json& json_, const AttackEmitterHitTemplate& hit_)
{
    // 見た目の発生と同時に攻撃判定を出す場合の、HitBoxへ転写する攻撃情報を保存する。
    json_ =
    {
        { "Enabled", hit_.enabled },
        { "Damage", hit_.damage },
        { "ImpactPower", hit_.impactPower },
        { "KnockBackPower", hit_.knockBackPower },
        { "TargetAirLiftPower", hit_.targetAirLiftPower },
        { "AttackerAirLiftPower", hit_.attackerAirLiftPower },
        { "HitStopDuration", hit_.hitStopDuration },
        { "HitStopScale", hit_.hitStopScale },
        { "HitGroupId", hit_.hitGroupId },
        { "AllowMultiHit", hit_.allowMultiHit },
        { "CanHitOwner", hit_.canHitOwner },
        { "DestroyOnHit", hit_.destroyOnHit },
        { "JustGuardedActionName", hit_.justGuardedActionName },
        { "HitParticleAssetPath", hit_.hitParticleAssetPath },
        { "HitParticleEmitterName", hit_.hitParticleEmitterName },
        { "HitParticleLifeTime", hit_.hitParticleLifeTime },
        { "HitParticleNormalOffset", hit_.hitParticleNormalOffset },
        { "HitParticleUseImpactAxis", hit_.hitParticleUseImpactAxis },
        { "HitParticleAxisToAttackerBlend", hit_.hitParticleAxisToAttackerBlend },
        { "HitParticleUseTargetAttraction", hit_.hitParticleUseTargetAttraction },
        { "HitParticleTargetAttractionStrength", hit_.hitParticleTargetAttractionStrength },
        { "HitParticleTargetAttractionRange", hit_.hitParticleTargetAttractionRange },
        { "HitParticleTargetAttractionFalloff", hit_.hitParticleTargetAttractionFalloff },
        { "HitMeshVFXAssetPath", hit_.hitMeshVFXAssetPath },
        { "HitMeshVFXLifeTime", hit_.hitMeshVFXLifeTime },
        { "HitMeshVFXNormalOffset", hit_.hitMeshVFXNormalOffset },
        { "HitImpactEffectAssetPath", hit_.hitImpactEffectAssetPath },
        { "CameraShakeEnabled", hit_.cameraShakeEnabled },
        { "CameraShakeReceiveDuration", hit_.cameraShakeReceiveDuration },
        { "CameraShakeReceivePositionStrength", hit_.cameraShakeReceivePositionStrength },
        { "CameraShakeReceiveRotationStrength", hit_.cameraShakeReceiveRotationStrength },
        { "CameraShakeAttackScale", hit_.cameraShakeAttackScale },
        { "CameraShakeFrequency", hit_.cameraShakeFrequency },
        { "CameraShakePriority", hit_.cameraShakePriority },
    };
}

void from_json(const nlohmann::json& json_, AttackEmitterHitTemplate& hit_)
{
    // ヒット情報は戦闘処理へ直結するため、負になって困る値は読み込み時に補正する。
    hit_.enabled = json_.value("Enabled", true);
    hit_.damage = json_.value("Damage", 10.0f);
    hit_.impactPower = (std::max)(json_.value("ImpactPower", 10.0f), 0.0f);
    hit_.knockBackPower = json_.value("KnockBackPower", 3.0f);
    hit_.targetAirLiftPower = json_.value("TargetAirLiftPower", 0.0f);
    hit_.attackerAirLiftPower = json_.value("AttackerAirLiftPower", 0.0f);
    hit_.hitStopDuration = (std::max)(json_.value("HitStopDuration", 0.0f), 0.0f);
    hit_.hitStopScale = std::clamp(json_.value("HitStopScale", 0.0f), 0.0f, 1.0f);
    hit_.hitGroupId = json_.value("HitGroupId", 0u);
    hit_.allowMultiHit = json_.value("AllowMultiHit", false);
    hit_.canHitOwner = json_.value("CanHitOwner", false);
    hit_.destroyOnHit = json_.value("DestroyOnHit", false);
    hit_.justGuardedActionName = json_.value("JustGuardedActionName", std::string{});
    hit_.hitParticleAssetPath = json_.value("HitParticleAssetPath", std::string{});
    hit_.hitParticleEmitterName = json_.value("HitParticleEmitterName", std::string{});
    hit_.hitParticleLifeTime = (std::max)(json_.value("HitParticleLifeTime", 1.0f), 0.01f);
    hit_.hitParticleNormalOffset = json_.value("HitParticleNormalOffset", 0.0f);
    hit_.hitParticleUseImpactAxis = json_.value("HitParticleUseImpactAxis", false);
    hit_.hitParticleAxisToAttackerBlend =
        std::clamp(json_.value("HitParticleAxisToAttackerBlend", 0.0f), 0.0f, 1.0f);
    hit_.hitParticleUseTargetAttraction = json_.value("HitParticleUseTargetAttraction", false);
    hit_.hitParticleTargetAttractionStrength =
        json_.value("HitParticleTargetAttractionStrength", 0.0f);
    hit_.hitParticleTargetAttractionRange =
        (std::max)(json_.value("HitParticleTargetAttractionRange", 0.0f), 0.0f);
    hit_.hitParticleTargetAttractionFalloff =
        (std::max)(json_.value("HitParticleTargetAttractionFalloff", 1.0f), 0.001f);
    hit_.hitMeshVFXAssetPath = json_.value("HitMeshVFXAssetPath", std::string{});
    hit_.hitMeshVFXLifeTime = (std::max)(json_.value("HitMeshVFXLifeTime", 1.0f), 0.01f);
    hit_.hitMeshVFXNormalOffset = json_.value("HitMeshVFXNormalOffset", 0.0f);
    hit_.hitImpactEffectAssetPath = json_.value("HitImpactEffectAssetPath", std::string{});
    hit_.cameraShakeEnabled = json_.value("CameraShakeEnabled", true);
    hit_.cameraShakeReceiveDuration = (std::max)(json_.value("CameraShakeReceiveDuration", 0.12f), 0.0f);
    hit_.cameraShakeReceivePositionStrength = (std::max)(json_.value("CameraShakeReceivePositionStrength", 0.025f), 0.0f);
    hit_.cameraShakeReceiveRotationStrength = (std::max)(json_.value("CameraShakeReceiveRotationStrength", 0.18f), 0.0f);
    hit_.cameraShakeAttackScale = (std::max)(json_.value("CameraShakeAttackScale", 0.5f), 0.0f);
    hit_.cameraShakeFrequency = (std::max)(json_.value("CameraShakeFrequency", 24.0f), 0.0f);
    hit_.cameraShakePriority = json_.value("CameraShakePriority", 5);
}

void to_json(nlohmann::json& json_, const AttackEmitterTemplate& template_)
{
    // 1回のEmitで使う発生位置、形状、複数エントリ、攻撃判定をまとめて保存する。
    json_ =
    {
        { "Name", template_.name },
        { "ActionName", template_.actionName },
        { "ShapeType", static_cast<int>(template_.shapeType) },
        { "SourceNodeName", template_.sourceNodeName },
        { "LocalOffset", WriteVector3(template_.localOffset) },
        { "LocalRotation", WriteVector3(template_.localRotation) },
        { "LocalScale", WriteVector3(template_.localScale) },
        { "Range", template_.range },
        { "Radius", template_.radius },
        { "Speed", template_.speed },
        { "LifeTime", template_.lifeTime },
        { "FanAngleDegrees", template_.fanAngleDegrees },
        { "ProjectileMotionEnabled", template_.projectileMotionEnabled },
        { "ProjectileAcceleration", template_.projectileAcceleration },
        { "ProjectileGravity", template_.projectileGravity },
        { "ProjectileMaxSpeed", template_.projectileMaxSpeed },
        { "FanSampleCount", template_.fanSampleCount },
        { "FanRangeSteps", template_.fanRangeSteps },
        { "FanVisualSampleCount", template_.fanVisualSampleCount },
        { "SpawnHit", template_.spawnHit },
        { "FitBeamVisualToHitShape", template_.fitBeamVisualToHitShape },
        { "LeaveFireGroundMark", template_.leaveFireGroundMark },
        { "Entries", template_.entries },
        { "Hit", template_.hit },
    };
}

void from_json(const nlohmann::json& json_, AttackEmitterTemplate& template_)
{
    // テンプレートの形状値は、判定生成やProjectile移動で破綻しない範囲に丸める。
    template_.name = json_.value("Name", std::string{ "AttackEmitter" });
    template_.actionName = json_.value("ActionName", std::string{ "AttackEmitter_Fire" });
    template_.shapeType = static_cast<ATTACK_EMITTER_SHAPE_TYPE>(std::clamp(json_.value("ShapeType", 1), 0, 3));
    template_.sourceNodeName = json_.value("SourceNodeName", std::string{});
    template_.localOffset = ReadVector3(json_, "LocalOffset", template_.localOffset);
    template_.localRotation = ReadVector3(json_, "LocalRotation", template_.localRotation);
    template_.localScale = ReadVector3(json_, "LocalScale", template_.localScale);
    template_.range = (std::max)(json_.value("Range", 8.0f), 0.01f);
    template_.radius = (std::max)(json_.value("Radius", 0.5f), 0.01f);
    template_.speed = (std::max)(json_.value("Speed", 8.0f), 0.0f);
    template_.lifeTime = (std::max)(json_.value("LifeTime", 1.0f), 0.01f);
    template_.fanAngleDegrees = std::clamp(json_.value("FanAngleDegrees", 45.0f), 0.0f, 180.0f);
    template_.projectileMotionEnabled = json_.value("ProjectileMotionEnabled", true);
    template_.projectileAcceleration = json_.value("ProjectileAcceleration", 0.0f);
    template_.projectileGravity = (std::max)(json_.value("ProjectileGravity", 0.0f), 0.0f);
    template_.projectileMaxSpeed = (std::max)(json_.value("ProjectileMaxSpeed", 0.0f), 0.0f);
    template_.fanSampleCount = std::clamp(json_.value("FanSampleCount", 5), 1, 31);
    template_.fanRangeSteps = std::clamp(json_.value("FanRangeSteps", 3), 1, 16);
    template_.fanVisualSampleCount = std::clamp(json_.value("FanVisualSampleCount", 1), 1, 31);
    template_.spawnHit = json_.value("SpawnHit", true);
    template_.fitBeamVisualToHitShape = json_.value("FitBeamVisualToHitShape", false);
    template_.leaveFireGroundMark = json_.value("LeaveFireGroundMark", false);
    template_.entries = json_.value("Entries", std::vector<AttackEmitterEntryTemplate>{});
    template_.hit = json_.value("Hit", AttackEmitterHitTemplate{});
}

void AttackEmitterComponent::Start()
{
    BS_PROFILE_SCOPE("AttackEmitterComponent::Start");
    // 空のまま置かれてもエディタですぐ試せるよう、最低1つのテンプレートとエントリを用意する。
    if (templates.empty())
    {
        AttackEmitterTemplate template_{};
        template_.entries.push_back({});
        templates.push_back(template_);
    }
    RegisterActions();
}

void AttackEmitterComponent::RegisterActions()
{
    BS_PROFILE_SCOPE("AttackEmitterComponent::RegisterActions");
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_)
    {
        return;
    }

    // インデックス、ActionName、Nameの3種類で登録し、アニメイベントから呼びやすくする。
    for (size_t index_ = 0; index_ < templates.size(); ++index_)
    {
        const std::string indexActionName_ = "AttackEmitter_" + std::to_string(index_);
        owner_->RegisterAction("AttackEmitter", indexActionName_, [this, index_]()
        {
            Emit(index_);
        });

        if (!templates[index_].actionName.empty())
        {
            owner_->RegisterAction(
                "AttackEmitter",
                templates[index_].actionName,
                [this, index_]()
            {
                Emit(index_);
            });
        }
        if (!templates[index_].name.empty())
        {
            owner_->RegisterAction(
                "AttackEmitter",
                templates[index_].name,
                [this, index_]()
            {
                Emit(index_);
            });
        }
    }
}

void AttackEmitterComponent::OnDestroy()
{
    BS_PROFILE_SCOPE("AttackEmitterComponent::OnDestroy");
    const auto owner_ = GetGameObject();
    if (owner_ && owner_->manager)
    {
        owner_->GetManager().GetEffectManager().StopAttackEffects(owner_);
    }
}

void AttackEmitterComponent::Emit(size_t templateIndex_)
{
    const auto owner_ = GetGameObject();
    if (!IsEnabled() || !owner_ || !owner_->manager || !owner_->IsCombatReady())
    {
        return;
    }
    BS_PROFILE_SCOPE("AttackEmitterComponent::Emit");
    if (templateIndex_ >= templates.size())
    {
        return;
    }
    const AttackEmitterTemplate& template_ = templates[templateIndex_];
    // 発生元のワールド位置/方向を一度求め、判定と見た目エントリで同じ基準を使う。
    const EmitFrame frame_ = BuildEmitFrame(template_);
    EmitWithFrame(template_, frame_);
}

void AttackEmitterComponent::EmitWithFrame(
    const AttackEmitterTemplate& template_,
    const EmitFrame& frame_)
{
    BS_PROFILE_SCOPE("AttackEmitterComponent::EmitWithFrame");
    BS_PROFILE_PLOT("AttackEmitterComponent::EntryCount", template_.entries.size());

    std::shared_ptr<GameObject> hitObject_;
    if (template_.spawnHit && template_.hit.enabled)
    {
        hitObject_ = SpawnHitObject(template_, frame_);
    }

    for (const AttackEmitterEntryTemplate& entry_ : template_.entries)
    {
        SpawnEntry(template_, entry_, frame_, hitObject_);
    }

    if (!hitObject_ || !template_.leaveFireGroundMark)
    {
        return;
    }

    const float horizontal_ = std::hypot(frame_.direction.x, frame_.direction.z);
    if (horizontal_ <= 0.001f)
    {
        return;
    }
    const float yaw_ = std::atan2(frame_.direction.x, frame_.direction.z);
    if (template_.shapeType == ATTACK_EMITTER_SHAPE_TYPE::BEAM ||
        template_.shapeType == ATTACK_EMITTER_SHAPE_TYPE::FAN)
    {
        const bool fan_ = template_.shapeType == ATTACK_EMITTER_SHAPE_TYPE::FAN;
        const float length_ = template_.range * std::abs(template_.localScale.z);
        const float radius_ = template_.radius * (std::max)(std::abs(template_.localScale.x), std::abs(template_.localScale.y));
        const float halfAngle_ = DirectX::XMConvertToRadians(template_.fanAngleDegrees * 0.5f);
        const float width_ = fan_ ? template_.range * std::abs(template_.localScale.x) * std::sin(halfAngle_) +
            radius_ * 1.1f : radius_;
        const float halfLength_ = length_ * horizontal_ * 0.5f + radius_;
        const DirectX::XMFLOAT3 center_{
            frame_.position.x + frame_.direction.x * length_ * 0.5f,
            frame_.position.y + frame_.direction.y * length_ * 0.5f,
            frame_.position.z + frame_.direction.z * length_ * 0.5f
        };
        GetManager().GetEffectManager().SpawnGroundMark(fan_ ? GroundMarkShape::FIRE_FAN :
            GroundMarkShape::FIRE_BEAM, center_, width_, halfLength_, yaw_);
    }
}

void AttackEmitterComponent::EmitByName(const std::string& templateName_)
{
    const auto owner_ = GetGameObject();
    if (!IsEnabled() || !owner_ || !owner_->manager || !owner_->IsCombatReady())
    {
        return;
    }
    for (size_t index_ = 0; index_ < templates.size(); ++index_)
    {
        if (templates[index_].name == templateName_ ||
            templates[index_].actionName == templateName_)
        {
            Emit(index_);
            return;
        }
    }
}

void AttackEmitterComponent::EmitByNameAt(
    const std::string& templateName_,
    const DirectX::XMFLOAT3& position_,
    const DirectX::XMFLOAT3& direction_)
{
    BS_PROFILE_SCOPE("AttackEmitterComponent::EmitByNameAt");
    const auto owner_ = GetGameObject();
    if (!IsEnabled() || !owner_ || !owner_->manager || !owner_->IsCombatReady())
    {
        return;
    }

    const EmitFrame frame_ = BuildEmitFrameAt(position_, direction_);
    for (size_t index_ = 0; index_ < templates.size(); ++index_)
    {
        if (templates[index_].name == templateName_ ||
            templates[index_].actionName == templateName_)
        {
            const AttackEmitterTemplate& template_ = templates[index_];
            const DirectX::XMMATRIX localMatrix_ = BuildLocalMatrix(
                template_.localOffset,
                template_.localRotation,
                template_.localScale);

            EmitFrame adjustedFrame_ = frame_;
            // 外部で解決済みの口元/頭部位置に、Template単位の発射補正を重ねる。
            adjustedFrame_.sourceMatrix = localMatrix_ * frame_.sourceMatrix;
            adjustedFrame_.position = ExtractPosition(adjustedFrame_.sourceMatrix);
            adjustedFrame_.direction = TransformForward(adjustedFrame_.sourceMatrix);
            EmitWithFrame(template_, adjustedFrame_);
            return;
        }
    }
}

bool AttackEmitterComponent::ResolveBeamGeometryAt(
    const std::string& templateName_,
    const DirectX::XMFLOAT3& position_,
    const DirectX::XMFLOAT3& direction_,
    DirectX::XMFLOAT3& start_,
    DirectX::XMFLOAT3& end_,
    float& radius_) const
{
    BS_PROFILE_SCOPE("AttackEmitterComponent::ResolveBeamGeometryAt");
    for (const AttackEmitterTemplate& template_ : templates)
    {
        if ((template_.name != templateName_ && template_.actionName != templateName_) ||
            template_.shapeType != ATTACK_EMITTER_SHAPE_TYPE::BEAM)
        {
            continue;
        }
        const EmitFrame frame_ = BuildEmitFrameAt(position_, direction_);
        const DirectX::XMMATRIX matrix_ = BuildLocalMatrix(
            template_.localOffset, template_.localRotation, template_.localScale) * frame_.sourceMatrix;
        start_ = ExtractPosition(matrix_);
        DirectX::XMStoreFloat3(&end_, DirectX::XMVector3TransformCoord(
            DirectX::XMVectorSet(0.0f, 0.0f, template_.range, 1.0f), matrix_));
        radius_ = template_.radius * (std::max)(std::abs(template_.localScale.x), std::abs(template_.localScale.y));
        return true;
    }
    return false;
}

bool AttackEmitterComponent::LoadAsset(const std::string& path_, bool forceReload_)
{
    BS_PROFILE_SCOPE("AttackEmitterComponent::LoadAsset");

    // 外部アセットはTemplates配列だけを差し替え、Action登録は読み込み後に作り直す。
    if (path_.empty())
    {
        return false;
    }
    if (!forceReload_ && loadedAssetPath == path_ && !templates.empty())
    {
        return true;
    }

    std::ifstream file_(path_);
    if (!file_.is_open())
    {
        return false;
    }

    try
    {
        nlohmann::json json_;
        file_ >> json_;
        const std::string assetType_ = json_.value("AssetType", std::string{});
        if (assetType_ != "AttackEmitter" && assetType_ != "EffectEmitter")
        {
            return false;
        }

        templates = json_.value("Templates", std::vector<AttackEmitterTemplate>{});
        assetPath = path_;
        loadedAssetPath = path_;
        if (templates.empty())
        {
            AttackEmitterTemplate template_{};
            template_.entries.push_back({});
            templates.push_back(template_);
        }
        RegisterActions();
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

bool AttackEmitterComponent::SaveAsset(const std::string& path_) const
{
    BS_PROFILE_SCOPE("AttackEmitterComponent::SaveAsset");

    // Componentで調整した複数Templateを、別Objectでも使えるAttackEmitter Assetとして保存する。
    if (path_.empty())
    {
        return false;
    }

    const std::filesystem::path outputPath_(path_);
    if (!outputPath_.parent_path().empty())
    {
        std::filesystem::create_directories(outputPath_.parent_path());
    }
    std::ofstream file_(outputPath_, std::ios::out | std::ios::trunc);
    if (!file_.is_open())
    {
        return false;
    }

    nlohmann::json json_;
    json_["AssetType"] = "AttackEmitter";
    json_["AssetVersion"] = 1;
    json_["Templates"] = templates;
    file_ << json_.dump(4);
    return file_.good();
}

AttackEmitterComponent::EmitFrame AttackEmitterComponent::BuildEmitFrame(
    const AttackEmitterTemplate& template_) const
{
    const std::shared_ptr<GameObject> owner_ = gameObject.lock();
    if (!owner_ || !owner_->transform)
    {
        return {};
    }

    // SourceNodeNameが指定されていれば、キャラクターのボーン/ノード位置を発生元にする。
    DirectX::XMMATRIX sourceMatrix_ = owner_->transform->GetWorldMatrix();
    if (!template_.sourceNodeName.empty())
    {
        std::shared_ptr<GltfModelCommponennt> model_ = owner_->GetComponent<GltfModelCommponennt>();
        if (model_ && model_->IsModelReady())
        {
            const int nodeIndex_ = model_->GetNodeIndex(template_.sourceNodeName.c_str());
            if (nodeIndex_ >= 0 && nodeIndex_ < static_cast<int>(model_->GetNodes().size()))
            {
                sourceMatrix_ = DirectX::XMLoadFloat4x4(&model_->GetNodes()[nodeIndex_].worldTransform);
            }
        }
    }

    const DirectX::XMMATRIX localMatrix_ = BuildLocalMatrix(
        template_.localOffset,
        template_.localRotation,
        template_.localScale);
    EmitFrame frame_{};
    frame_.sourceMatrix = localMatrix_ * sourceMatrix_;
    // 後続のSpawn処理で使いやすいよう、行列から位置と前方向を取り出しておく。
    frame_.position = ExtractPosition(frame_.sourceMatrix);
    frame_.direction = TransformForward(frame_.sourceMatrix);
    return frame_;
}

AttackEmitterComponent::EmitFrame AttackEmitterComponent::BuildEmitFrameAt(
    const DirectX::XMFLOAT3& position_,
    const DirectX::XMFLOAT3& direction_) const
{
    EmitFrame frame_{};
    frame_.position = position_;
    frame_.direction = NormalizeDirection(direction_);
    frame_.sourceMatrix = BuildDirectionMatrix(frame_.position, frame_.direction);
    return frame_;
}

DirectX::XMMATRIX AttackEmitterComponent::BuildLocalMatrix(
    const DirectX::XMFLOAT3& position_,
    const DirectX::XMFLOAT3& rotation_,
    const DirectX::XMFLOAT3& scale_) const
{
    return DirectX::XMMatrixScaling(scale_.x, scale_.y, scale_.z) *
        DirectX::XMMatrixRotationRollPitchYaw(
            DirectX::XMConvertToRadians(rotation_.x),
            DirectX::XMConvertToRadians(rotation_.y),
            DirectX::XMConvertToRadians(rotation_.z)) *
        DirectX::XMMatrixTranslation(position_.x, position_.y, position_.z);
}

void AttackEmitterComponent::SpawnEntry(
    const AttackEmitterTemplate& template_,
    const AttackEmitterEntryTemplate& entry_,
    const EmitFrame& frame_,
    const std::shared_ptr<GameObject>& followTarget_)
{
    BS_PROFILE_SCOPE("AttackEmitterComponent::SpawnEntry");

    if (!entry_.enabled || entry_.assetPath.empty())
    {
        return;
    }

    EffectManager& effectManager_ = GetManager().GetEffectManager();

    auto spawnWithMatrix_ = [&](const DirectX::XMMATRIX& entryMatrix_)
    {
        const bool fitBeamParticle_ = template_.shapeType == ATTACK_EMITTER_SHAPE_TYPE::BEAM &&
            template_.fitBeamVisualToHitShape && entry_.type == ATTACK_EMITTER_ENTRY_TYPE::PARTICLE;
        const DirectX::XMFLOAT3 position_ = ExtractPosition(entryMatrix_);
        const DirectX::XMFLOAT3 direction_ = TransformForward(entryMatrix_);
        std::shared_ptr<GameObject> spawnedObject_;

        if (entry_.type == ATTACK_EMITTER_ENTRY_TYPE::PARTICLE)
        {
            // ParticleはVFXManager経由で一時GameObjectとして生成し、Emitter指定があれば個別再生する。
            ParticleSpawnRequest request_{};
            request_.assetPath = entry_.assetPath;
            request_.emitterName = entry_.emitterName;
            request_.position = position_;
            request_.normal = direction_;
            // AttackEmitterの向きに合わせて、Particleの初速軸も発射方向へそろえる。
            request_.axis = direction_;
            request_.overrideAxis = true;
            request_.lifeTime = fitBeamParticle_ ? template_.lifeTime : entry_.lifeTime;
            request_.normalOffset = entry_.normalOffset;
            spawnedObject_ = effectManager_.SpawnParticle(request_);
        }
        else if (entry_.type == ATTACK_EMITTER_ENTRY_TYPE::MESH_VFX)
        {
            // MeshVFXは生成後に色ランプやブレンドなど、Emitter側の一時上書きを適用できる。
            MeshVFXSpawnRequest request_{};
            request_.assetPath = entry_.assetPath;
            request_.position = position_;
            request_.normal = direction_;
            request_.lifeTime = entry_.lifeTime;
            request_.normalOffset = entry_.normalOffset;
            spawnedObject_ = effectManager_.SpawnMeshVFX(request_);
            ApplyMeshVFXOverrides(spawnedObject_, entry_);
        }
        else if (entry_.type == ATTACK_EMITTER_ENTRY_TYPE::IMPACT_EFFECT)
        {
            effectManager_.SpawnImpactEffectAsset(entry_.assetPath, position_, direction_, GetGameObject());
        }

        if (!spawnedObject_ || !spawnedObject_->transform)
        {
            return;
        }
        // Particleや溜め炎など、HitBoxのない見た目にも発生元を紐付ける。
        effectManager_.RegisterAttackEffectOwner(spawnedObject_, GetGameObject());

        if (template_.shapeType == ATTACK_EMITTER_SHAPE_TYPE::PROJECTILE &&
            followTarget_ &&
            followTarget_->transform)
        {
            // Projectile判定と見た目を同じ親へぶら下げ、移動する判定に追従させる。
            spawnedObject_->transform->SetParent(followTarget_->transform);
        }
        // 補正対象の直線炎はEntry独自の移動・拡大を使わず、判定と同じ発射基準に置く。
        spawnedObject_->transform->ApplyWorldMatrix(fitBeamParticle_ ? frame_.sourceMatrix : entryMatrix_);
        spawnedObject_->UpdateTransform();
        if (std::shared_ptr<MeshVFXComponent> meshVFX_ = spawnedObject_->GetComponent<MeshVFXComponent>())
        {
            meshVFX_->SetPlaybackSpeed(entry_.playbackSpeed);
        }
        if (std::shared_ptr<ParticleComponent> particle_ = spawnedObject_->GetComponent<ParticleComponent>())
        {
            if (fitBeamParticle_)
            {
                BS_PROFILE_SCOPE("AttackEmitterComponent::FitBeamParticleVisual");
                DirectX::XMFLOAT3 end_{};
                DirectX::XMStoreFloat3(&end_, DirectX::XMVector3TransformNormal(
                    DirectX::XMVectorSet(0.0f, 0.0f, template_.range, 0.0f), frame_.sourceMatrix));
                const float radius_ = template_.radius *
                    (std::max)(std::abs(template_.localScale.x), std::abs(template_.localScale.y));
                float maxSize_ = 0.01f;
                for (const auto& track_ : particle_->GetTracks())
                {
                    maxSize_ = (std::max)({ maxSize_, track_.settings.emissionSize.x, track_.settings.emissionSize.y });
                }
                for (auto& track_ : particle_->GetTracks())
                {
                    if (!track_.hasBeamVisualOverride)
                    {
                        track_.beamVisualSettingsBackup = track_.settings;
                        track_.beamVisualSpawnSpaceBackup = track_.spawnSpace;
                        track_.beamVisualDelayBackup = track_.delay;
                        track_.hasBeamVisualOverride = true;
                    }
                    auto& settings_ = track_.settings;
                    // ParticleのLine座標はワールド軸のオフセット。判定の両端を直接指定する。
                    track_.spawnSpace = ParticleSpawnSpace::OWNER_RELATIVE;
                    settings_.emissionPosition = { 0.0f, 0.0f, 0.0f, 1.0f };
                    settings_.spawnShape = 4u;
                    settings_.shapeStart = { 0.0f, 0.0f, 0.0f, 1.0f };
                    settings_.shapeEnd = { end_.x, end_.y, end_.z, 1.0f };
                    // 判定は発射時から全長が有効なので、炎も全長へ一度に発生させる。
                    settings_.emissionMode = 1u;
                    settings_.burstTrigger = 1.0f;
                    track_.delay = 0.0f;
                    // 板ポリゴンの四隅まで半径内に収め、炎が判定外へ流れ続ける力を解除する。
                    const float sizeScale_ = radius_ * 1.41421356f / maxSize_;
                    settings_.emissionSize.x *= sizeScale_;
                    settings_.emissionSize.y *= sizeScale_;
                    settings_.useRandomSize = 0u;
                    settings_.emissionSpeed = { 0.0f, 0.0f };
                    settings_.gravity = 0.0f;
                    settings_.windVelocity = { 0.0f, 0.0f, 0.0f, 0.0f };
                    settings_.noiseScale = 0.0f;
                    settings_.noiseStrength = 0.0f;
                    settings_.radialVelocity = 0.0f;
                    settings_.orbitVelocity = 0.0f;
                    settings_.useTargetAttraction = 0u;
                    settings_.verticalSpawnRange = 0.0f;
                    settings_.lifespan = { template_.lifeTime, template_.lifeTime };
                    settings_.type = 0u;
                }
            }
            if (!entry_.emitterName.empty())
            {
                particle_->PlayEmitter(entry_.emitterName);
            }
        }
    };

    if (template_.shapeType == ATTACK_EMITTER_SHAPE_TYPE::FAN)
    {
        const int sampleCount_ = std::clamp(
            template_.fanVisualSampleCount,
            1,
            (std::max)(template_.fanSampleCount, 1));
        BS_PROFILE_PLOT("AttackEmitterComponent::FanVisualSampleCount", sampleCount_);
        const float halfAngle_ = DirectX::XMConvertToRadians(template_.fanAngleDegrees * 0.5f);
        for (int sampleIndex_ = 0; sampleIndex_ < sampleCount_; ++sampleIndex_)
        {
            const float sampleRate_ = sampleCount_ == 1
                ? 0.5f
                : static_cast<float>(sampleIndex_) / static_cast<float>(sampleCount_ - 1);
            const float angle_ = -halfAngle_ + halfAngle_ * 2.0f * sampleRate_;
            const DirectX::XMMATRIX fanMatrix_ =
                DirectX::XMMatrixRotationY(angle_) * frame_.sourceMatrix;
            const DirectX::XMMATRIX entryMatrix_ = BuildLocalMatrix(
                entry_.localOffset,
                entry_.localRotation,
                entry_.localScale) * fanMatrix_;
            spawnWithMatrix_(entryMatrix_);
        }
        return;
    }

    // テンプレート基準にEntryごとのローカルTransformを重ね、見た目の発生位置を決める。
    const DirectX::XMMATRIX entryMatrix_ = BuildLocalMatrix(
        entry_.localOffset,
        entry_.localRotation,
        entry_.localScale) * frame_.sourceMatrix;
    spawnWithMatrix_(entryMatrix_);
}

std::shared_ptr<GameObject> AttackEmitterComponent::SpawnHitObject(
    const AttackEmitterTemplate& template_,
    const EmitFrame& frame_)
{
    BS_PROFILE_SCOPE("AttackEmitterComponent::SpawnHitObject");
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_)
    {
        return nullptr;
    }

    std::shared_ptr<GameObject> hitObject_ = GetManager().Create("RuntimeAttackEmitterHit");
    if (!hitObject_)
    {
        return nullptr;
    }
    hitObject_->SetNoSaveObject();
    // HitObject自体を発生元の向きへ置き、AttackEffectHitComponent側のローカル形状をその上に作る。
    hitObject_->transform->ApplyWorldMatrix(frame_.sourceMatrix);
    hitObject_->UpdateTransform();

    std::shared_ptr<AttackEffectHitComponent> attackHit_ =
        hitObject_->AddComponent<AttackEffectHitComponent>();
    if (!attackHit_)
    {
        return hitObject_;
    }

    const AttackEmitterHitTemplate& hit_ = template_.hit;
    // Emitter用のHit設定をAttackEffectHitComponentへ転写し、既存の戦闘判定システムに乗せる。
    attackHit_->SetOwner(owner_.get());
    attackHit_->SetAttackInstanceId(GetManager().GetAttackSystem().CreateAttackInstance());
    attackHit_->SetHitGroupId(hit_.hitGroupId);
    attackHit_->SetDamage(hit_.damage);
    attackHit_->SetImpactPower(hit_.impactPower);
    attackHit_->SetHitStopDuration(hit_.hitStopDuration);
    attackHit_->SetHitStopScale(hit_.hitStopScale);
    attackHit_->SetKnockBackPower(hit_.knockBackPower);
    attackHit_->SetAirLiftPower(hit_.targetAirLiftPower, hit_.attackerAirLiftPower);
    attackHit_->SetCameraShake(
        hit_.cameraShakeEnabled,
        hit_.cameraShakeReceiveDuration,
        hit_.cameraShakeReceivePositionStrength,
        hit_.cameraShakeReceiveRotationStrength,
        hit_.cameraShakeAttackScale,
        hit_.cameraShakeFrequency,
        hit_.cameraShakePriority);
    attackHit_->SetHitParticleAssetPath(hit_.hitParticleAssetPath);
    attackHit_->SetHitParticleEmitterName(hit_.hitParticleEmitterName);
    attackHit_->SetHitParticleLifeTime(hit_.hitParticleLifeTime);
    attackHit_->SetHitParticleNormalOffset(hit_.hitParticleNormalOffset);
    attackHit_->SetHitParticleImpactAxis(
        hit_.hitParticleUseImpactAxis,
        hit_.hitParticleAxisToAttackerBlend);
    attackHit_->SetHitParticleTargetAttraction(
        hit_.hitParticleUseTargetAttraction,
        hit_.hitParticleTargetAttractionStrength,
        hit_.hitParticleTargetAttractionRange,
        hit_.hitParticleTargetAttractionFalloff);
    attackHit_->SetHitMeshVFXAssetPath(hit_.hitMeshVFXAssetPath);
    attackHit_->SetHitMeshVFXLifeTime(hit_.hitMeshVFXLifeTime);
    attackHit_->SetHitMeshVFXNormalOffset(hit_.hitMeshVFXNormalOffset);
    attackHit_->SetHitImpactEffectAssetPath(hit_.hitImpactEffectAssetPath);
    attackHit_->SetAllowMultiHit(hit_.allowMultiHit);
    attackHit_->SetCanHitOwner(hit_.canHitOwner);
    attackHit_->SetSourceAction(template_.name);
    attackHit_->SetJustGuardedActionName(hit_.justGuardedActionName);
    attackHit_->SetEffectType(template_.shapeType == ATTACK_EMITTER_SHAPE_TYPE::PROJECTILE
        ? ATTACK_EFFECT_TYPE::PROJECTILE
        : template_.shapeType == ATTACK_EMITTER_SHAPE_TYPE::BEAM
        ? ATTACK_EFFECT_TYPE::BEAM
        : template_.shapeType == ATTACK_EMITTER_SHAPE_TYPE::AREA
        ? ATTACK_EFFECT_TYPE::AREA_OVER_TIME
        : ATTACK_EFFECT_TYPE::BREATH);
    attackHit_->SetLifeTime(template_.lifeTime);
    attackHit_->SetDestroyOnLifeEnd(true);
    attackHit_->SetMovement(frame_.direction, 0.0f);
    attackHit_->SetHitShapes(BuildHitShapes(template_));
    if (template_.shapeType == ATTACK_EMITTER_SHAPE_TYPE::PROJECTILE &&
        template_.projectileMotionEnabled)
    {
        // 弾道制御は専用Componentへ分離し、HitBoxと見た目を同じProjectile Objectへ追従させる。
        std::shared_ptr<ProjectileMotionComponent> motion_ =
            hitObject_->AddComponent<ProjectileMotionComponent>();
        if (motion_)
        {
            motion_->Configure(
                frame_.direction,
                template_.speed,
                template_.projectileAcceleration,
                template_.projectileGravity,
                template_.projectileMaxSpeed);
            if (template_.leaveFireGroundMark)
            {
                motion_->SetFireGroundMarkRadius(template_.radius *
                    (std::max)(std::abs(template_.localScale.x), std::abs(template_.localScale.z)));
            }
        }
    }
    if (!hit_.justGuardedActionName.empty())
    {
        std::weak_ptr<GameObject> weakHitObject_ = hitObject_;
        std::weak_ptr<GameObject> weakOwner_ = owner_;
        std::weak_ptr<AttackEffectHitComponent> weakAttackHit_ = attackHit_;
        const float projectileSpeed_ = template_.speed;
        const float projectileAcceleration_ = template_.projectileAcceleration;
        const float projectileGravity_ = template_.projectileGravity;
        const float projectileMaxSpeed_ = template_.projectileMaxSpeed;

        hitObject_->RegisterAction(
            "AttackEmitter",
            hit_.justGuardedActionName,
            [weakHitObject_, weakOwner_, weakAttackHit_, projectileSpeed_, projectileAcceleration_, projectileGravity_, projectileMaxSpeed_]()
            {
                const std::shared_ptr<GameObject> hitObject_ = weakHitObject_.lock();
                const std::shared_ptr<AttackEffectHitComponent> attackHit_ = weakAttackHit_.lock();
                if (!hitObject_ || !hitObject_->transform || !attackHit_)
            {
                return;
            }

            DirectX::SimpleMath::Vector3 targetPosition_{ 0.0f, 0.0f, 0.0f };
                std::string reflectOwnerName_;
                if (const std::shared_ptr<BlackboardComponent> blackboard_ =
                    hitObject_->GetComponent<BlackboardComponent>())
                {
                    blackboard_->GetVector3("GuardReaction", "ReflectTargetPosition", targetPosition_);
                    blackboard_->GetString("GuardReaction", "ReflectOwnerName", reflectOwnerName_);
                }
                if (targetPosition_.LengthSquared() <= 0.000001f)
                {
                    if (const std::shared_ptr<GameObject> owner_ = weakOwner_.lock())
                    {
                        if (owner_->transform)
                    {
                        targetPosition_ = owner_->transform->GetWorldPosition();
                    }
                }
                }

                const DirectX::SimpleMath::Vector3 currentPosition_ =
                    hitObject_->transform->GetWorldPosition();
                DirectX::SimpleMath::Vector3 direction_ = targetPosition_ - currentPosition_;
                if (direction_.LengthSquared() <= 0.000001f)
                {
                    direction_ = { 0.0f, 0.0f, -1.0f };
                }
                direction_.Normalize();

                if (std::shared_ptr<ProjectileMotionComponent> motion_ =
                    hitObject_->GetComponent<ProjectileMotionComponent>())
                {
                    // ジャストガードされたProjectileを、攻撃元へ向けて同じ見た目のまま飛ばし直す。
                    motion_->Configure(
                        { direction_.x, direction_.y, direction_.z },
                        projectileSpeed_,
                        projectileAcceleration_,
                        projectileGravity_,
                        projectileMaxSpeed_);
                }

                if (!reflectOwnerName_.empty())
                {
                    if (const std::shared_ptr<GameObject> reflectOwner_ =
                        hitObject_->GetManager().Find(reflectOwnerName_))
                    {
                        attackHit_->SetRuntimeHitOwner(reflectOwner_.get());
                    }
                }
                attackHit_->SetRuntimeHitTeamId(0);
                attackHit_->ResetRuntime();
                hitObject_->SetLayer(LayerType::PlayerBullet);
            });
    }
    attackHit_->Play();
    return hitObject_;
}

std::vector<AttackEffectHitShapeTemplate> AttackEmitterComponent::BuildHitShapes(
    const AttackEmitterTemplate& template_) const
{
    BS_PROFILE_SCOPE("AttackEmitterComponent::BuildHitShapes");

    // Area/Projectile/Beam/Fanの抽象形状を、AttackEffectHitComponentが扱えるHitShapeへ変換する。
    std::vector<AttackEffectHitShapeTemplate> shapes_;
    AttackEffectHitShapeTemplate shape_{};
    shape_.enabled = true;
    shape_.hitBoxName = template_.name.empty() ? "AttackEmitterHit" : template_.name;
    shape_.radius = (std::max)(template_.radius, 0.01f);
    shape_.activeStartTime = 0.0f;
    shape_.activeEndTime = template_.lifeTime;
    shape_.destroyOnHit = template_.hit.destroyOnHit;
    shape_.allowMultiHit = template_.hit.allowMultiHit;

    if (template_.shapeType == ATTACK_EMITTER_SHAPE_TYPE::AREA)
    {
        shape_.shape = ATTACK_HIT_SHAPE::SPHERE;
        shapes_.push_back(shape_);
    }
    else if (template_.shapeType == ATTACK_EMITTER_SHAPE_TYPE::PROJECTILE)
    {
        shape_.shape = ATTACK_HIT_SHAPE::SPHERE;
        shape_.localOffset = { 0.0f, 0.0f, 0.0f };
        shapes_.push_back(shape_);
    }
    else if (template_.shapeType == ATTACK_EMITTER_SHAPE_TYPE::BEAM)
    {
        shape_.shape = ATTACK_HIT_SHAPE::CAPSULE;
        shape_.localOffset = { 0.0f, 0.0f, template_.range * 0.5f };
        shape_.localRotation = { 90.0f, 0.0f, 0.0f };
        shape_.halfHeight = template_.range * 0.5f;
        shapes_.push_back(shape_);
    }
    else
    {
        // Fanは根元から先端へ向かって複数Sphereを時間差で有効化し、広がるブレス判定にする。
        const int sampleCount_ = std::clamp(template_.fanSampleCount, 1, 31);
        const int rangeSteps_ = std::clamp(template_.fanRangeSteps, 1, 16);
        BS_PROFILE_PLOT("AttackEmitterComponent::FanHitSampleCount", sampleCount_);
        BS_PROFILE_PLOT("AttackEmitterComponent::FanHitRangeSteps", rangeSteps_);
        const float halfAngle_ = DirectX::XMConvertToRadians(template_.fanAngleDegrees * 0.5f);
        for (int rangeIndex_ = 1; rangeIndex_ <= rangeSteps_; ++rangeIndex_)
        {
            const float rangeRate_ = static_cast<float>(rangeIndex_) / static_cast<float>(rangeSteps_);
            const float distance_ = template_.range * rangeRate_;
            const float activeStartTime_ = template_.lifeTime * 0.35f *
                static_cast<float>(rangeIndex_ - 1) / static_cast<float>(rangeSteps_);
            for (int sampleIndex_ = 0; sampleIndex_ < sampleCount_; ++sampleIndex_)
            {
                const float sampleRate_ = sampleCount_ == 1
                    ? 0.5f
                    : static_cast<float>(sampleIndex_) / static_cast<float>(sampleCount_ - 1);
                const float angle_ = -halfAngle_ + halfAngle_ * 2.0f * sampleRate_;
                AttackEffectHitShapeTemplate fanShape_ = shape_;
                fanShape_.shape = ATTACK_HIT_SHAPE::SPHERE;
                fanShape_.localOffset =
                {
                    std::sin(angle_) * distance_,
                    0.0f,
                    std::cos(angle_) * distance_,
                };
                fanShape_.radius = template_.radius * (0.75f + 0.35f * rangeRate_);
                fanShape_.activeStartTime = activeStartTime_;
                fanShape_.activeEndTime = template_.lifeTime;
                fanShape_.allowMultiHit = template_.hit.allowMultiHit;
                fanShape_.hitBoxName = shape_.hitBoxName + "_Fan";
                shapes_.push_back(fanShape_);
            }
        }
    }
    BS_PROFILE_PLOT("AttackEmitterComponent::BuiltHitShapeCount", shapes_.size());
    return shapes_;
}

void AttackEmitterComponent::ApplyMeshVFXOverrides(
    const std::shared_ptr<GameObject>& object_,
    const AttackEmitterEntryTemplate& entry_) const
{
    if (!object_)
    {
        return;
    }
    std::shared_ptr<MeshVFXComponent> meshVFX_ = object_->GetComponent<MeshVFXComponent>();
    if (!meshVFX_)
    {
        return;
    }
    if (!entry_.overrideMeshVFXColorRamp && !entry_.forceAdditiveBlend && entry_.glowMultiplier == 1.0f)
    {
        return;
    }

    MeshVFXAsset asset_ = meshVFX_->GetAsset();
    // 元アセットは変更せず、生成済みコンポーネントのコピーにだけ一時的な見た目変更を適用する。
    for (MeshVFXPart& part_ : asset_.parts)
    {
        if (entry_.overrideMeshVFXColorRamp)
        {
            part_.gradientTexturePath = entry_.colorRampTexturePath;
            part_.useGradientTexture = !entry_.colorRampTexturePath.empty();
        }
        if (entry_.forceAdditiveBlend)
        {
            part_.blendMode = MeshVFXBlendMode::ADDITIVE;
        }
        part_.emissiveStrength *= (std::max)(entry_.glowMultiplier, 0.0f);
    }
    meshVFX_->SetAsset(asset_);
    meshVFX_->Reset();
    meshVFX_->Play();
}

void AttackEmitterComponent::Gui()
{
    BS_PROFILE_SCOPE("AttackEmitterComponent::Gui");
    // テンプレート、発生エントリ、攻撃判定をまとめて編集し、その場でEmitして確認できるようにする。
    std::string selectedAssetPath_ = assetPath;
    if (AssetSlotGui::Draw("AttackEmitter Asset", AssetSlotType::JSON, selectedAssetPath_, nullptr, "Data/AttackEmitter"))
    {
        assetPath = selectedAssetPath_;
        if (!assetPath.empty())
        {
            LoadAsset(assetPath);
        }
    }
    if (!assetPath.empty())
    {
        if (ImGui::Button("Reload AttackEmitter Asset"))
        {
            LoadAsset(assetPath, true);
        }
        ImGui::SameLine();
        if (ImGui::Button("Save AttackEmitter Asset"))
        {
            SaveAsset(assetPath);
        }
    }

    if (ImGui::Button("Register Actions"))
    {
        RegisterActions();
    }
    ImGui::SameLine();
    if (ImGui::Button("Add Template"))
    {
        AttackEmitterTemplate template_{};
        template_.name = "AttackEmitter" + std::to_string(templates.size());
        template_.actionName = "AttackEmitter_" + std::to_string(templates.size());
        template_.entries.push_back({});
        templates.push_back(template_);
        RegisterActions();
    }

    int removeTemplateIndex_ = -1;
    static const char* shapeNames_[] = { "Area", "Projectile", "Beam", "Fan" };
    static const char* entryTypeNames_[] = { "Particle", "Mesh VFX / Sequence", "Impact Effect" };
    for (int templateIndex_ = 0; templateIndex_ < static_cast<int>(templates.size()); ++templateIndex_)
    {
        AttackEmitterTemplate& template_ = templates[templateIndex_];
        ImGui::PushID(templateIndex_);
        if (ImGui::TreeNode("Template", "%d: %s [%s]", templateIndex_, template_.name.c_str(), GetShapeTypeName(template_.shapeType)))
        {
            if (ImGui::Button("Emit"))
            {
                Emit(static_cast<size_t>(templateIndex_));
            }
            ImGui::SameLine();
            if (ImGui::Button("Remove"))
            {
                removeTemplateIndex_ = templateIndex_;
            }
            ImGui::InputText("Name", &template_.name);
            if (ImGui::InputText("Action Name", &template_.actionName))
            {
                RegisterActions();
            }
            int shapeType_ = static_cast<int>(template_.shapeType);
            if (ImGui::Combo("Shape Type", &shapeType_, shapeNames_, IM_ARRAYSIZE(shapeNames_)))
            {
                template_.shapeType = static_cast<ATTACK_EMITTER_SHAPE_TYPE>(shapeType_);
            }
            ImGui::InputText("Source Node Name", &template_.sourceNodeName);
            ImGui::DragFloat3("Local Offset", &template_.localOffset.x, 0.01f);
            ImGui::DragFloat3("Local Rotation", &template_.localRotation.x, 0.1f);
            ImGui::DragFloat3("Local Scale", &template_.localScale.x, 0.01f, 0.001f, 100.0f);
            ImGui::Checkbox("Leave Fire Ground Mark", &template_.leaveFireGroundMark);
            ImGui::DragFloat("Range", &template_.range, 0.1f, 0.01f, 1000.0f);
            ImGui::DragFloat("Radius", &template_.radius, 0.01f, 0.01f, 100.0f);
            ImGui::DragFloat("Speed", &template_.speed, 0.1f, 0.0f, 1000.0f);
            ImGui::DragFloat("Life Time", &template_.lifeTime, 0.01f, 0.01f, 120.0f);
            if (template_.shapeType == ATTACK_EMITTER_SHAPE_TYPE::BEAM)
            {
                ImGui::Checkbox("Fit Beam Visual To Hit Shape", &template_.fitBeamVisualToHitShape);
            }
            if (template_.shapeType == ATTACK_EMITTER_SHAPE_TYPE::PROJECTILE)
            {
                ImGui::Checkbox("Projectile Motion", &template_.projectileMotionEnabled);
                ImGui::DragFloat("Projectile Acceleration", &template_.projectileAcceleration, 0.1f, -1000.0f, 1000.0f);
                ImGui::DragFloat("Projectile Gravity", &template_.projectileGravity, 0.1f, 0.0f, 1000.0f);
                ImGui::DragFloat("Projectile Max Speed", &template_.projectileMaxSpeed, 0.1f, 0.0f, 1000.0f);
            }
            if (template_.shapeType == ATTACK_EMITTER_SHAPE_TYPE::FAN)
            {
                ImGui::DragFloat("Fan Angle", &template_.fanAngleDegrees, 0.5f, 0.0f, 180.0f);
                ImGui::SliderInt("Fan Samples", &template_.fanSampleCount, 1, 31);
                ImGui::SliderInt("Fan Range Steps", &template_.fanRangeSteps, 1, 16);
                ImGui::SliderInt("Fan Visual Samples", &template_.fanVisualSampleCount, 1, 31);
            }

            if (ImGui::TreeNode("Entries"))
            {
                int removeEntryIndex_ = -1;
                for (int entryIndex_ = 0; entryIndex_ < static_cast<int>(template_.entries.size()); ++entryIndex_)
                {
                    AttackEmitterEntryTemplate& entry_ = template_.entries[entryIndex_];
                    ImGui::PushID(entryIndex_);
                    if (ImGui::TreeNode("Entry", "%d: %s", entryIndex_, GetEntryTypeName(entry_.type)))
                    {
                        ImGui::Checkbox("Enabled", &entry_.enabled);
                        int entryType_ = static_cast<int>(entry_.type);
                        if (ImGui::Combo("Type", &entryType_, entryTypeNames_, IM_ARRAYSIZE(entryTypeNames_)))
                        {
                            entry_.type = static_cast<ATTACK_EMITTER_ENTRY_TYPE>(entryType_);
                            entry_.assetPath.clear();
                        }
                        AssetSlotGui::Draw("Effect JSON", AssetSlotType::JSON, entry_.assetPath);
                        if (entry_.type == ATTACK_EMITTER_ENTRY_TYPE::PARTICLE)
                        {
                            ImGui::InputText("Emitter Name", &entry_.emitterName);
                        }
                        ImGui::DragFloat3("Local Offset", &entry_.localOffset.x, 0.01f);
                        ImGui::DragFloat3("Local Rotation", &entry_.localRotation.x, 0.1f);
                        ImGui::DragFloat3("Local Scale", &entry_.localScale.x, 0.01f, 0.001f, 100.0f);
                        ImGui::DragFloat("Life Time", &entry_.lifeTime, 0.01f, 0.01f, 120.0f);
                        ImGui::DragFloat("Normal Offset", &entry_.normalOffset, 0.001f, -100.0f, 100.0f);
                        ImGui::DragFloat("Playback Speed", &entry_.playbackSpeed, 0.01f, 0.0f, 10.0f);
                        if (entry_.type == ATTACK_EMITTER_ENTRY_TYPE::MESH_VFX)
                        {
                            ImGui::Checkbox("Override Color Ramp", &entry_.overrideMeshVFXColorRamp);
                            if (entry_.overrideMeshVFXColorRamp)
                            {
                                AssetSlotGui::Draw("Color Ramp", AssetSlotType::TEXTURE, entry_.colorRampTexturePath);
                            }
                            ImGui::Checkbox("Force Additive Blend", &entry_.forceAdditiveBlend);
                            ImGui::DragFloat("Glow Multiplier", &entry_.glowMultiplier, 0.05f, 0.0f, 20.0f);
                        }
                        if (ImGui::Button("Remove Entry"))
                        {
                            removeEntryIndex_ = entryIndex_;
                        }
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }
                if (removeEntryIndex_ >= 0)
                {
                    template_.entries.erase(template_.entries.begin() + removeEntryIndex_);
                }
                if (ImGui::Button("Add Particle"))
                {
                    template_.entries.push_back({});
                }
                ImGui::SameLine();
                if (ImGui::Button("Add Mesh VFX"))
                {
                    AttackEmitterEntryTemplate entry_{};
                    entry_.type = ATTACK_EMITTER_ENTRY_TYPE::MESH_VFX;
                    template_.entries.push_back(entry_);
                }
                ImGui::SameLine();
                if (ImGui::Button("Add Impact Effect"))
                {
                    AttackEmitterEntryTemplate entry_{};
                    entry_.type = ATTACK_EMITTER_ENTRY_TYPE::IMPACT_EFFECT;
                    template_.entries.push_back(entry_);
                }
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Hit"))
            {
                ImGui::Checkbox("Spawn Hit", &template_.spawnHit);
                ImGui::Checkbox("Enabled", &template_.hit.enabled);
                ImGui::DragFloat("Damage", &template_.hit.damage, 0.1f, 0.0f, 10000.0f);
                ImGui::DragFloat("Impact Power", &template_.hit.impactPower, 0.1f, 0.0f, 10000.0f);
                ImGui::DragFloat("Knock Back Power", &template_.hit.knockBackPower, 0.1f, 0.0f, 10000.0f);
                ImGui::DragFloat("Target Air Lift", &template_.hit.targetAirLiftPower, 0.1f, -1000.0f, 1000.0f);
                ImGui::DragFloat("Attacker Air Lift", &template_.hit.attackerAirLiftPower, 0.1f, -1000.0f, 1000.0f);
                ImGui::DragFloat("Hit Stop Duration", &template_.hit.hitStopDuration, 0.001f, 0.0f, 10.0f);
                ImGui::DragFloat("Hit Stop Scale", &template_.hit.hitStopScale, 0.01f, 0.0f, 1.0f);
                ImGui::InputScalar("Hit Group ID", ImGuiDataType_U32, &template_.hit.hitGroupId);
                ImGui::Checkbox("Allow Multi Hit", &template_.hit.allowMultiHit);
                ImGui::Checkbox("Can Hit Owner", &template_.hit.canHitOwner);
                ImGui::Checkbox("Destroy On Hit", &template_.hit.destroyOnHit);
                ImGui::InputText("Just Guarded Action", &template_.hit.justGuardedActionName);
                AssetSlotGui::Draw("Hit Particle", AssetSlotType::JSON, template_.hit.hitParticleAssetPath);
                ImGui::InputText("Hit Emitter Name", &template_.hit.hitParticleEmitterName);
                ImGui::DragFloat("Hit Particle Life Time", &template_.hit.hitParticleLifeTime, 0.01f, 0.0f, 30.0f);
                ImGui::DragFloat("Hit Particle Normal Offset", &template_.hit.hitParticleNormalOffset, 0.01f, -10.0f, 10.0f);
                ImGui::Checkbox("Use Particle Impact Axis", &template_.hit.hitParticleUseImpactAxis);
                ImGui::BeginDisabled(!template_.hit.hitParticleUseImpactAxis);
                ImGui::SliderFloat("Axis To Attacker Blend", &template_.hit.hitParticleAxisToAttackerBlend, 0.0f, 1.0f);
                ImGui::EndDisabled();
                ImGui::Checkbox("Use Particle Target Pull", &template_.hit.hitParticleUseTargetAttraction);
                ImGui::BeginDisabled(!template_.hit.hitParticleUseTargetAttraction);
                ImGui::DragFloat("Target Pull Strength", &template_.hit.hitParticleTargetAttractionStrength, 0.01f, -100.0f, 100.0f);
                ImGui::DragFloat("Target Pull Range", &template_.hit.hitParticleTargetAttractionRange, 0.01f, 0.0f, 100.0f);
                ImGui::DragFloat("Target Pull Falloff", &template_.hit.hitParticleTargetAttractionFalloff, 0.01f, 0.001f, 8.0f);
                ImGui::EndDisabled();
                AssetSlotGui::Draw("Hit Mesh VFX", AssetSlotType::JSON, template_.hit.hitMeshVFXAssetPath);
                AssetSlotGui::Draw(
                    "Hit Impact Effect",
                    AssetSlotType::JSON,
                    template_.hit.hitImpactEffectAssetPath,
                    nullptr,
                    "Data/ImpactEffect");
                ImGui::Checkbox("Camera Shake", &template_.hit.cameraShakeEnabled);
                ImGui::DragFloat("Shake Duration", &template_.hit.cameraShakeReceiveDuration, 0.001f, 0.0f, 10.0f);
                ImGui::DragFloat("Shake Position", &template_.hit.cameraShakeReceivePositionStrength, 0.001f, 0.0f, 10.0f);
                ImGui::DragFloat("Shake Rotation", &template_.hit.cameraShakeReceiveRotationStrength, 0.001f, 0.0f, 10.0f);
                ImGui::DragFloat("Shake Attack Scale", &template_.hit.cameraShakeAttackScale, 0.01f, 0.0f, 10.0f);
                ImGui::DragFloat("Shake Frequency", &template_.hit.cameraShakeFrequency, 0.1f, 0.0f, 240.0f);
                ImGui::InputInt("Shake Priority", &template_.hit.cameraShakePriority);
                ImGui::TreePop();
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    if (removeTemplateIndex_ >= 0)
    {
        templates.erase(templates.begin() + removeTemplateIndex_);
        RegisterActions();
    }
}

void AttackEmitterComponent::Serialize(nlohmann::json& json_) const
{
    // 複数テンプレートをコンポーネントに持たせ、1つのObjectから複数種類の発生を扱えるようにする。
    json_["ComponentName"] = componentName;
    json_["AssetPath"] = assetPath;
    json_["Templates"] = templates;
}

void AttackEmitterComponent::Deserialize(const nlohmann::json& json_)
{
    assetPath = json_.value("AssetPath", std::string{});
    templates = json_.value("Templates", std::vector<AttackEmitterTemplate>{});
    if (!assetPath.empty() && LoadAsset(assetPath))
    {
        return;
    }
    // 古い/空データでもエディタで編集を続けられるよう、最低1つのテンプレートを補う。
    if (templates.empty())
    {
        AttackEmitterTemplate template_{};
        template_.entries.push_back({});
        templates.push_back(template_);
    }
    RegisterActions();
}

REGISTER_COMPONENT(AttackEmitterComponent, "AttackEmitterComponent")

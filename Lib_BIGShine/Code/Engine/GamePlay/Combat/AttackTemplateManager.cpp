#include "PrecompiledHeader.h"

#include "AttackTemplateManager.h"

#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Serialize/ActionRegistry.h"
#include "Engine/Platform/WindowsDialogHelper.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <nlohmann/json.hpp>
#include <type_traits>

namespace
{
    using Json_ = nlohmann::json;

    Json_ ToVectorJson(const DirectX::XMFLOAT3& value_)
    {
        return { value_.x, value_.y, value_.z };
    }

    DirectX::XMFLOAT3 ReadVector3(
        const Json_& json_, const char* key_, const DirectX::XMFLOAT3& fallback_)
    {
        const Json_ value_ = json_.value(key_, Json_::array());
        if (!value_.is_array() || value_.size() < 3u) return fallback_;
        return { value_[0].get<float>(), value_[1].get<float>(), value_[2].get<float>() };
    }

    template<typename Enum_>
    Enum_ ReadEnum(const Json_& json_, const char* key_, Enum_ fallback_)
    {
        if (!json_.contains(key_)) return fallback_;
        const Json_& value_ = json_[key_];
        if (value_.is_number_integer()) return static_cast<Enum_>(value_.get<int>());
        if (!value_.is_string()) return fallback_;
        const std::string name_ = value_.get<std::string>();
        if constexpr (std::is_same_v<Enum_, ATTACK_TEMPLATE_OWNER_TARGET>)
        {
            if (name_ == "OWNER") return Enum_::OWNER;
            if (name_ == "WEAPON") return Enum_::WEAPON;
            if (name_ == "TARGET") return Enum_::TARGET;
            if (name_ == "LOCK_ON_TARGET") return Enum_::LOCK_ON_TARGET;
            return Enum_::SELF;
        }
        if constexpr (std::is_same_v<Enum_, EFFECT_HIT_SHAPE>)
        {
            if (name_ == "NONE") return Enum_::NONE;
            if (name_ == "CAPSULE") return Enum_::CAPSULE;
            if (name_ == "SPHERE_GROUP") return Enum_::SPHERE_GROUP;
            if (name_ == "FAN") return Enum_::FAN;
            return Enum_::BOX;
        }
        if constexpr (std::is_same_v<Enum_, KNOCK_BACK_TYPE>)
        {
            if (name_ == "EFFECT_FORWARD") return Enum_::EFFECT_FORWARD;
            if (name_ == "FROM_OWNER_TO_TARGET") return Enum_::FROM_OWNER_TO_TARGET;
            if (name_ == "FROM_EFFECT_TO_TARGET") return Enum_::FROM_EFFECT_TO_TARGET;
            return Enum_::OWNER_FORWARD;
        }
        if constexpr (std::is_same_v<Enum_, CAMERA_SHAKE_POWER_LEVEL>)
        {
            if (name_ == "MEDIUM") return Enum_::MEDIUM;
            if (name_ == "HEAVY") return Enum_::HEAVY;
            return Enum_::MINIMUM;
        }
        if constexpr (std::is_same_v<Enum_, ATTACK_EFFECT_TYPE>)
        {
            if (name_ == "PROJECTILE") return Enum_::PROJECTILE;
            if (name_ == "AREA_OVER_TIME") return Enum_::AREA_OVER_TIME;
            if (name_ == "BEAM") return Enum_::BEAM;
            if (name_ == "BREATH") return Enum_::BREATH;
            return Enum_::INSTANT_AREA;
        }
        if constexpr (std::is_same_v<Enum_, ATTACK_HIT_SHAPE>)
        {
            if (name_ == "NONE") return Enum_::NONE;
            if (name_ == "BOX") return Enum_::BOX;
            if (name_ == "CAPSULE") return Enum_::CAPSULE;
            if (name_ == "SPHERE_GROUP") return Enum_::SPHERE_GROUP;
            if (name_ == "FAN") return Enum_::FAN;
            if (name_ == "CONE") return Enum_::CONE;
            if (name_ == "RAY") return Enum_::RAY;
            return Enum_::SPHERE;
        }
        return fallback_;
    }

    void DrawVector3(const char* label_, DirectX::XMFLOAT3& value_)
    {
        ImGui::DragFloat3(label_, &value_.x, 0.01f);
    }

    void ApplyCameraShakePowerLevel(AttackCameraShakeTemplate& cameraShake_)
    {
        switch (cameraShake_.powerLevel)
        {
        case CAMERA_SHAKE_POWER_LEVEL::MEDIUM:
            cameraShake_.receiveDuration = 0.18f;
            cameraShake_.receivePositionStrength = 0.055f;
            cameraShake_.receiveRotationStrength = 0.45f;
            cameraShake_.priority = 8;
            break;
        case CAMERA_SHAKE_POWER_LEVEL::HEAVY:
            cameraShake_.receiveDuration = 0.24f;
            cameraShake_.receivePositionStrength = 0.085f;
            cameraShake_.receiveRotationStrength = 0.75f;
            cameraShake_.priority = 12;
            break;
        case CAMERA_SHAKE_POWER_LEVEL::MINIMUM:
        default:
            cameraShake_.receiveDuration = 0.12f;
            cameraShake_.receivePositionStrength = 0.025f;
            cameraShake_.receiveRotationStrength = 0.18f;
            cameraShake_.priority = 5;
            break;
        }
        cameraShake_.attackScale = 0.5f;
        cameraShake_.frequency = 24.0f;
    }

    void DrawAssetPathCombo(
        const char* label_,
        std::string& value_,
        const std::vector<std::string>& assets_,
        EffectManager* effectManager_,
        bool isParticleAsset_)
    {
        ImGui::SetNextItemWidth(320.0f);
        std::string previewValue_ = value_;
        if (effectManager_)
        {
            const std::string resolvedValue_ = isParticleAsset_
                ? effectManager_->ResolveParticlePath(value_)
                : effectManager_->ResolveMeshVFXPath(value_);
            for (const std::string& assetKey_ : assets_)
            {
                const std::string resolvedKey_ = isParticleAsset_
                    ? effectManager_->ResolveParticlePath(assetKey_)
                    : effectManager_->ResolveMeshVFXPath(assetKey_);
                if (!resolvedValue_.empty() && resolvedValue_ == resolvedKey_)
                {
                    previewValue_ = assetKey_;
                    break;
                }
            }
        }
        const char* preview_ = previewValue_.empty() ? "(None)" : previewValue_.c_str();
        if (ImGui::BeginCombo(label_, preview_))
        {
            if (ImGui::Selectable("(None)", value_.empty())) value_.clear();
            for (const std::string& assetKey_ : assets_)
            {
                const bool selected_ = previewValue_ == assetKey_;
                if (ImGui::Selectable(assetKey_.c_str(), selected_))
                {
                    value_ = assetKey_;
                    std::printf(
                        "[AttackTemplateManager::GUI] %s selected='%s'\n",
                        label_,
                        value_.c_str());
                }
                if (selected_) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        const std::string clearLabel_ = std::string("Clear##") + label_;
        if (ImGui::SmallButton(clearLabel_.c_str())) value_.clear();
        ImGui::SetNextItemWidth(-1.0f);
        const std::string pathLabel_ = std::string("Path##") + label_;
        ImGui::InputText(pathLabel_.c_str(), &value_);
    }

    std::vector<std::string> CollectHitBoxActionPrefixes(GameObjectManager* gameObjectManager_)
    {
        std::vector<std::string> prefixes_;
        auto collectRegistry_ = [&prefixes_](ActionRegistry& registry_)
            {
                for (const std::string& category_ : registry_.GetCategories())
                {
                    for (const std::string& actionName_ : registry_.GetActionNames(category_))
                    {
                        const std::string enableSuffix_ = "_EnableHitBox";
                        const std::string disableSuffix_ = "_DisableHitBox";
                        if (actionName_.size() > enableSuffix_.size() &&
                            actionName_.ends_with(enableSuffix_))
                            prefixes_.push_back(actionName_.substr(0, actionName_.size() - enableSuffix_.size()));
                        else if (actionName_.size() > disableSuffix_.size() &&
                            actionName_.ends_with(disableSuffix_))
                            prefixes_.push_back(actionName_.substr(0, actionName_.size() - disableSuffix_.size()));
                    }
                }
            };
        if (gameObjectManager_)
        {
            for (const std::shared_ptr<GameObject>& object_ : gameObjectManager_->GetObjects())
                if (object_) collectRegistry_(object_->localRegistry);
        }
        collectRegistry_(ActionRegistry::Instance());
        std::sort(prefixes_.begin(), prefixes_.end());
        prefixes_.erase(std::unique(prefixes_.begin(), prefixes_.end()), prefixes_.end());
        return prefixes_;
    }

    void DrawHitBoxActionPrefixCombo(
        const char* label_,
        std::string& value_,
        const std::vector<std::string>& prefixes_)
    {
        const char* preview_ = value_.empty() ? "(Select HitBox Action Prefix)" : value_.c_str();
        ImGui::SetNextItemWidth(320.0f);
        if (ImGui::BeginCombo(label_, preview_))
        {
            for (const std::string& prefix_ : prefixes_)
            {
                const bool selected_ = value_ == prefix_;
                if (ImGui::Selectable(prefix_.c_str(), selected_)) value_ = prefix_;
                if (selected_) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::SetNextItemWidth(-1.0f);
        const std::string inputLabel_ = std::string("Action Prefix##") + label_;
        ImGui::InputText(inputLabel_.c_str(), &value_);
    }

    AttackEffectHitShapeTemplate ReadAttackHitShape(const Json_& json_)
    {
        AttackEffectHitShapeTemplate shape_{};
        shape_.enabled = json_.value("enabled", true);
        shape_.hitBoxName = json_.value("hitBoxName", std::string("AttackEffectHitBox"));
        shape_.shape = NormalizeAttackHitShape(ReadEnum(json_, "shape", ATTACK_HIT_SHAPE::SPHERE));
        shape_.localOffset = ReadVector3(json_, "localOffset", shape_.localOffset);
        shape_.localRotation = ReadVector3(json_, "localRotation", shape_.localRotation);
        shape_.localScale = ReadVector3(json_, "localScale", shape_.localScale);
        shape_.radius = json_.value("radius", 0.5f);
        shape_.halfHeight = json_.value("halfHeight", 1.0f);
        shape_.range = json_.value("range", 5.0f);
        shape_.angle = json_.value("angle", 45.0f);
        shape_.activeStartTime = json_.value("activeStartTime", 0.0f);
        shape_.activeEndTime = json_.value("activeEndTime", 0.1f);
        shape_.hitInterval = json_.value("hitInterval", 0.0f);
        shape_.destroyOnHit = json_.value("destroyOnHit", false);
        shape_.allowMultiHit = json_.value("allowMultiHit", false);
        for (const Json_& sphereJson_ : json_.value("spheres", Json_::array()))
        {
            AttackEffectSphereTemplate sphere_{};
            sphere_.localOffset = ReadVector3(sphereJson_, "localOffset", sphere_.localOffset);
            sphere_.radius = sphereJson_.value("radius", 0.5f);
            shape_.spheres.push_back(sphere_);
        }
        return shape_;
    }

    Json_ WriteAttackHitShape(const AttackEffectHitShapeTemplate& shape_)
    {
        Json_ spheres_ = Json_::array();
        for (const AttackEffectSphereTemplate& sphere_ : shape_.spheres)
            spheres_.push_back({ { "localOffset", ToVectorJson(sphere_.localOffset) }, { "radius", sphere_.radius } });
        return {
            { "enabled", shape_.enabled }, { "hitBoxName", shape_.hitBoxName },
            { "shape", static_cast<int>(NormalizeAttackHitShape(shape_.shape)) },
            { "localOffset", ToVectorJson(shape_.localOffset) },
            { "localRotation", ToVectorJson(shape_.localRotation) },
            { "localScale", ToVectorJson(shape_.localScale) },
            { "radius", shape_.radius }, { "halfHeight", shape_.halfHeight },
            { "range", shape_.range }, { "angle", shape_.angle },
            { "activeStartTime", shape_.activeStartTime }, { "activeEndTime", shape_.activeEndTime },
            { "hitInterval", shape_.hitInterval }, { "destroyOnHit", shape_.destroyOnHit },
            { "allowMultiHit", shape_.allowMultiHit }, { "spheres", spheres_ }
        };
    }

    void ReplaceAll(
        std::string& value_,
        const std::string& oldValue_,
        const std::string& newValue_)
    {
        size_t position_ = 0u;
        while ((position_ = value_.find(oldValue_, position_)) != std::string::npos)
        {
            value_.replace(position_, oldValue_.size(), newValue_);
            position_ += newValue_.size();
        }
    }

    void ReplaceAttackTemplateReferences(
        const std::string& oldId_,
        const std::string& newId_)
    {
        const std::filesystem::path roots_[] =
        {
            "Data/Animation/StateData",
            "Data/Animation/GraphData",
            "Data/Scenes",
        };
        const std::string oldPretty_ =
            "\"attackTemplateId\": \"" + oldId_ + "\"";
        const std::string newPretty_ =
            "\"attackTemplateId\": \"" + newId_ + "\"";
        const std::string oldCompact_ =
            "\"attackTemplateId\":\"" + oldId_ + "\"";
        const std::string newCompact_ =
            "\"attackTemplateId\":\"" + newId_ + "\"";

        for (const std::filesystem::path& root_ : roots_)
        {
            std::error_code error_;
            if (!std::filesystem::exists(root_, error_)) continue;
            for (const std::filesystem::directory_entry& entry_ :
                std::filesystem::recursive_directory_iterator(root_, error_))
            {
                if (error_) break;
                if (!entry_.is_regular_file() || entry_.path().extension() != ".json") continue;

                std::ifstream input_(entry_.path(), std::ios::binary);
                if (!input_) continue;
                std::string contents_(
                    (std::istreambuf_iterator<char>(input_)),
                    std::istreambuf_iterator<char>());
                input_.close();
                if (contents_.find(oldPretty_) == std::string::npos &&
                    contents_.find(oldCompact_) == std::string::npos)
                {
                    continue;
                }
                ReplaceAll(contents_, oldPretty_, newPretty_);
                ReplaceAll(contents_, oldCompact_, newCompact_);
                std::ofstream output_(entry_.path(), std::ios::binary | std::ios::trunc);
                if (output_) output_.write(contents_.data(), contents_.size());
            }
        }
    }
}

AttackTemplateManager& AttackTemplateManager::Instance()
{
    static AttackTemplateManager instance_;
    return instance_;
}

void AttackTemplateManager::LoadAll()
{
    // Data/AttackTemplate配下を走査し、攻撃テンプレートJSONを一覧として読み直す。
    templates.clear();
    const std::filesystem::path root_ = WindowsDialogHelper::Utf8ToPath("Data/AttackTemplate");
    std::error_code error_;
    std::filesystem::create_directories(root_, error_);
    for (const std::filesystem::directory_entry& entry_ :
        std::filesystem::directory_iterator(root_, error_))
    {
        if (error_) break;
        if (entry_.is_regular_file() && entry_.path().extension() == ".json")
        {
            LoadTemplate(WindowsDialogHelper::PathToUtf8(entry_.path()));
        }
    }
    std::sort(templates.begin(), templates.end(), [](const AttackTemplate& left_, const AttackTemplate& right_)
        {
            return left_.id < right_.id;
        });
    std::erase_if(
        renamedTemplateIds,
        [this](const auto& pair_)
        {
            return std::none_of(
                templates.begin(),
                templates.end(),
                [&pair_](const AttackTemplate& value_)
                {
                    return value_.id == pair_.second;
                });
        });
    loaded = true;
}

bool AttackTemplateManager::LoadTemplate(const std::string& path_)
{
    // 旧キーも読み取れるようfallbackを多めに持ち、保存済みAssetを壊さず復元する。
    std::ifstream stream_(WindowsDialogHelper::Utf8ToPath(NormalizeAttackTemplatePath(path_)));
    if (!stream_) return false;
    Json_ json_ = Json_::parse(stream_, nullptr, false);
    if (json_.is_discarded()) return false;
    if (json_.value("AssetType", std::string("AttackTemplate")) != "AttackTemplate") return false;

    AttackTemplate result_{};
    result_.id = json_.value("id", std::filesystem::path(path_).stem().string());
    result_.displayName = json_.value("displayName", result_.id);
    const Json_ owner_ = json_.value("owner", Json_::object());
    result_.ownerTarget = ReadEnum(owner_, "ownerTarget", ATTACK_TEMPLATE_OWNER_TARGET::SELF);
    result_.canHitOwner = owner_.value("canHitOwner", false);
    const Json_ attack_ = json_.value("attack", Json_::object());
    result_.damage = attack_.value("damage", 5.0f);
    result_.impactPower = (std::max)(attack_.value("impactPower", result_.damage), 0.0f);
    result_.hitStopDuration = attack_.value("hitStopDuration", attack_.value("hitStopTime", 0.05f));
    result_.hitStopScale = attack_.value("hitStopScale", 0.0f);
    result_.knockBackPower = attack_.value("knockBackPower", 3.0f);
    result_.targetAirLiftPower = (std::max)(attack_.value("targetAirLiftPower", 0.0f), 0.0f);
    result_.attackerAirLiftPower = (std::max)(attack_.value("attackerAirLiftPower", 0.0f), 0.0f);
    result_.knockBackType = ReadEnum(attack_, "knockBackType", KNOCK_BACK_TYPE::OWNER_FORWARD);
    result_.hitGroupId = attack_.value("hitGroupId", 0u);
    result_.allowMultiHit = attack_.value("allowMultiHit", false);
    const Json_ cameraShake_ = json_.value("cameraShake", Json_::object());
    result_.cameraShake.enabled = cameraShake_.value("enabled", true);
    result_.cameraShake.powerLevel =
        ReadEnum(cameraShake_, "powerLevel", CAMERA_SHAKE_POWER_LEVEL::MINIMUM);
    ApplyCameraShakePowerLevel(result_.cameraShake);
    result_.cameraShake.receiveDuration =
        (std::max)(cameraShake_.value("receiveDuration", result_.cameraShake.receiveDuration), 0.0f);
    result_.cameraShake.receivePositionStrength =
        (std::max)(cameraShake_.value("receivePositionStrength", result_.cameraShake.receivePositionStrength), 0.0f);
    result_.cameraShake.receiveRotationStrength =
        (std::max)(cameraShake_.value("receiveRotationStrength", result_.cameraShake.receiveRotationStrength), 0.0f);
    result_.cameraShake.attackScale =
        (std::clamp)(cameraShake_.value("attackScale", result_.cameraShake.attackScale), 0.0f, 1.0f);
    result_.cameraShake.frequency =
        (std::max)(cameraShake_.value("frequency", result_.cameraShake.frequency), 0.0f);
    result_.cameraShake.priority = cameraShake_.value("priority", result_.cameraShake.priority);
    const Json_ weapon_ = json_.value("weaponHitBox", Json_::object());
    result_.weaponHitBox.enabled = weapon_.value("enabled", false);
    result_.weaponHitBox.hitBoxName = weapon_.value("hitBoxName", std::string("HitBox"));
    result_.weaponHitBox.startNodeName = weapon_.value("startNodeName", std::string("WeaponRoot"));
    result_.weaponHitBox.endNodeName = weapon_.value("endNodeName", std::string("WeaponTip"));
    result_.weaponHitBox.radius = weapon_.value("radius", 0.18f);
    result_.weaponHitBox.activeStartTime = weapon_.value("activeStartTime", 0.0f);
    result_.weaponHitBox.activeEndTime = weapon_.value("activeEndTime", 0.18f);
    const Json_ hitEffect_ = json_.value("hitEffect", Json_::object());
    result_.hitEffect.spawnOnHit = hitEffect_.value("spawnOnHit", true);
    result_.hitEffect.hitParticleAssetPath = hitEffect_.value(
        "hitParticleAssetPath",
        hitEffect_.value(
            "hitParticlePath",
            hitEffect_.value(
                "particlePath",
                hitEffect_.value("hitEffectPath", std::string{}))));
    result_.hitEffect.hitMeshVfxAssetPath = hitEffect_.value(
        "hitMeshVfxAssetPath",
        hitEffect_.value(
            "hitMeshVFXAssetPath",
            hitEffect_.value("meshVfxPath", std::string{})));
    result_.hitEffect.hitParticleEmitterName = hitEffect_.value("hitParticleEmitterName", std::string{});
    result_.hitEffect.hitParticleLifeTime = (std::max)(hitEffect_.value("hitParticleLifeTime", 1.0f), 0.0f);
    result_.hitEffect.hitParticleNormalOffset = hitEffect_.value("hitParticleNormalOffset", 0.0f);
    result_.hitEffect.hitParticleUseImpactAxis = hitEffect_.value("hitParticleUseImpactAxis", false);
    result_.hitEffect.hitParticleAxisToAttackerBlend = std::clamp(
        hitEffect_.value("hitParticleAxisToAttackerBlend", 0.0f),
        0.0f,
        1.0f);
    result_.hitEffect.hitParticleUseTargetAttraction =
        hitEffect_.value("hitParticleUseTargetAttraction", false);
    result_.hitEffect.hitParticleTargetAttractionStrength =
        hitEffect_.value("hitParticleTargetAttractionStrength", 0.0f);
    result_.hitEffect.hitParticleTargetAttractionRange =
        (std::max)(hitEffect_.value("hitParticleTargetAttractionRange", 0.0f), 0.0f);
    result_.hitEffect.hitParticleTargetAttractionFalloff =
        (std::max)(hitEffect_.value("hitParticleTargetAttractionFalloff", 1.0f), 0.001f);
    result_.hitEffect.hitMeshVfxLifeTime = (std::max)(hitEffect_.value("hitMeshVfxLifeTime", 1.0f), 0.0f);
    result_.hitEffect.hitMeshVfxNormalOffset = hitEffect_.value("hitMeshVfxNormalOffset", 0.0f);
    std::printf(
        "[AttackTemplateManager::LoadTemplate] id='%s' hitParticleAssetPath='%s' "
        "hitMeshVfxAssetPath='%s'\n",
        result_.id.c_str(),
        result_.hitEffect.hitParticleAssetPath.c_str(),
        result_.hitEffect.hitMeshVfxAssetPath.c_str());

    for (const Json_& slashJson_ : json_.value("slashEffects", Json_::array()))
    {
        SlashEffectTemplate slash_{};
        slash_.enabled = slashJson_.value("enabled", true);
        slash_.vfxPath = slashJson_.value("vfxPath", std::string{});
        slash_.spawnTimeOffset = slashJson_.value("spawnTimeOffset", 0.0f);
        slash_.lifeTime = slashJson_.value("lifeTime", 0.18f);
        slash_.localOffset = ReadVector3(slashJson_, "localOffset", slash_.localOffset);
        slash_.localRotation = ReadVector3(slashJson_, "localRotation", slash_.localRotation);
        slash_.localScale = ReadVector3(slashJson_, "localScale", slash_.localScale);
        slash_.alpha = slashJson_.value("alpha", 1.0f);
        slash_.followOwner = slashJson_.value("followOwner", false);
        slash_.followNode = slashJson_.value("followNode", false);
        slash_.baseNodeName = slashJson_.value("baseNodeName", std::string{});
        const Json_ hitBox_ = slashJson_.value("hitBox", Json_::object());
        slash_.hitBox.enabled = hitBox_.value("enabled", false);
        slash_.hitBox.hitBoxName = hitBox_.value("hitBoxName", std::string("EffectHitBox"));
        slash_.hitBox.shape = ReadEnum(hitBox_, "shape", EFFECT_HIT_SHAPE::BOX);
        slash_.hitBox.activeStartTime = hitBox_.value("activeStartTime", 0.0f);
        slash_.hitBox.activeEndTime = hitBox_.value("activeEndTime", 0.08f);
        slash_.hitBox.localOffset = ReadVector3(hitBox_, "localOffset", slash_.hitBox.localOffset);
        slash_.hitBox.localRotation = ReadVector3(hitBox_, "localRotation", slash_.hitBox.localRotation);
        slash_.hitBox.localScale = ReadVector3(hitBox_, "localScale", slash_.hitBox.localScale);
        slash_.hitBox.radius = hitBox_.value("radius", 0.5f);
        slash_.hitBox.halfHeight = hitBox_.value("halfHeight", 1.0f);
        result_.slashEffects.push_back(slash_);
    }
    for (const Json_& effectJson_ : json_.value("attackEffects", Json_::array()))
    {
        AttackEffectSpawnTemplate effect_{};
        effect_.enabled = effectJson_.value("enabled", true);
        effect_.effectType = ReadEnum(effectJson_, "effectType", ATTACK_EFFECT_TYPE::INSTANT_AREA);
        effect_.particlePath = effectJson_.value("particlePath", std::string{});
        effect_.meshVfxPath = effectJson_.value("meshVfxPath", std::string{});
        effect_.spawnTimeOffset = effectJson_.value("spawnTimeOffset", 0.0f);
        effect_.lifeTime = effectJson_.value("lifeTime", 1.0f);
        effect_.localOffset = ReadVector3(effectJson_, "localOffset", effect_.localOffset);
        effect_.localRotation = ReadVector3(effectJson_, "localRotation", effect_.localRotation);
        effect_.localScale = ReadVector3(effectJson_, "localScale", effect_.localScale);
        effect_.followOwner = effectJson_.value("followOwner", false);
        effect_.followNode = effectJson_.value("followNode", false);
        effect_.baseNodeName = effectJson_.value("baseNodeName", std::string{});
        effect_.speed = effectJson_.value("speed", 0.0f);
        effect_.localDirection = ReadVector3(effectJson_, "localDirection", effect_.localDirection);
        for (const Json_& shapeJson_ : effectJson_.value("hitShapes", Json_::array()))
            effect_.hitShapes.push_back(ReadAttackHitShape(shapeJson_));
        result_.attackEffects.push_back(effect_);
    }
    if (AttackTemplate* existing_ = FindTemplate(result_.id)) *existing_ = result_;
    else templates.push_back(result_);
    return true;
}

bool AttackTemplateManager::SaveTemplate(const AttackTemplate& attackTemplate_)
{
    if (attackTemplate_.id.empty()) return false;

    // Editorで調整した攻撃データを、Animation Sequencerから参照できるID付きJSONとして保存する。
    std::printf(
        "[AttackTemplateManager::SaveTemplate] id='%s' hitParticleAssetPath='%s' "
        "hitMeshVfxAssetPath='%s'\n",
        attackTemplate_.id.c_str(),
        attackTemplate_.hitEffect.hitParticleAssetPath.c_str(),
        attackTemplate_.hitEffect.hitMeshVfxAssetPath.c_str());
    Json_ slashEffects_ = Json_::array();
    for (const SlashEffectTemplate& slash_ : attackTemplate_.slashEffects)
    {
        slashEffects_.push_back({
            { "enabled", slash_.enabled }, { "vfxPath", slash_.vfxPath },
            { "spawnTimeOffset", slash_.spawnTimeOffset }, { "lifeTime", slash_.lifeTime },
            { "localOffset", ToVectorJson(slash_.localOffset) },
            { "localRotation", ToVectorJson(slash_.localRotation) },
            { "localScale", ToVectorJson(slash_.localScale) }, { "alpha", slash_.alpha },
            { "followOwner", slash_.followOwner }, { "followNode", slash_.followNode },
            { "baseNodeName", slash_.baseNodeName },
            { "hitBox", {
                { "enabled", slash_.hitBox.enabled }, { "hitBoxName", slash_.hitBox.hitBoxName },
                { "shape", static_cast<int>(slash_.hitBox.shape) },
                { "activeStartTime", slash_.hitBox.activeStartTime },
                { "activeEndTime", slash_.hitBox.activeEndTime },
                { "localOffset", ToVectorJson(slash_.hitBox.localOffset) },
                { "localRotation", ToVectorJson(slash_.hitBox.localRotation) },
                { "localScale", ToVectorJson(slash_.hitBox.localScale) },
                { "radius", slash_.hitBox.radius }, { "halfHeight", slash_.hitBox.halfHeight } } }
        });
    }
    Json_ attackEffects_ = Json_::array();
    for (const AttackEffectSpawnTemplate& effect_ : attackTemplate_.attackEffects)
    {
        Json_ hitShapes_ = Json_::array();
        for (const AttackEffectHitShapeTemplate& shape_ : effect_.hitShapes)
            hitShapes_.push_back(WriteAttackHitShape(shape_));
        attackEffects_.push_back({
            { "enabled", effect_.enabled }, { "effectType", static_cast<int>(effect_.effectType) },
            { "particlePath", effect_.particlePath }, { "meshVfxPath", effect_.meshVfxPath },
            { "spawnTimeOffset", effect_.spawnTimeOffset }, { "lifeTime", effect_.lifeTime },
            { "localOffset", ToVectorJson(effect_.localOffset) },
            { "localRotation", ToVectorJson(effect_.localRotation) },
            { "localScale", ToVectorJson(effect_.localScale) },
            { "followOwner", effect_.followOwner }, { "followNode", effect_.followNode },
            { "baseNodeName", effect_.baseNodeName }, { "speed", effect_.speed },
            { "localDirection", ToVectorJson(effect_.localDirection) }, { "hitShapes", hitShapes_ }
        });
    }
    Json_ json_ = {
        { "AssetType", "AttackTemplate" }, { "AssetVersion", 1 },
        { "id", attackTemplate_.id }, { "displayName", attackTemplate_.displayName },
        { "owner", { { "ownerTarget", static_cast<int>(attackTemplate_.ownerTarget) },
            { "canHitOwner", attackTemplate_.canHitOwner } } },
        { "attack", { { "damage", attackTemplate_.damage },
            { "impactPower", attackTemplate_.impactPower },
            { "hitStopDuration", attackTemplate_.hitStopDuration },
            { "hitStopScale", attackTemplate_.hitStopScale },
            { "knockBackPower", attackTemplate_.knockBackPower },
            { "targetAirLiftPower", attackTemplate_.targetAirLiftPower },
            { "attackerAirLiftPower", attackTemplate_.attackerAirLiftPower },
            { "knockBackType", static_cast<int>(attackTemplate_.knockBackType) },
            { "hitGroupId", attackTemplate_.hitGroupId },
            { "allowMultiHit", attackTemplate_.allowMultiHit } } },
        { "cameraShake", { { "enabled", attackTemplate_.cameraShake.enabled },
            { "powerLevel", static_cast<int>(attackTemplate_.cameraShake.powerLevel) },
            { "receiveDuration", attackTemplate_.cameraShake.receiveDuration },
            { "receivePositionStrength", attackTemplate_.cameraShake.receivePositionStrength },
            { "receiveRotationStrength", attackTemplate_.cameraShake.receiveRotationStrength },
            { "attackScale", attackTemplate_.cameraShake.attackScale },
            { "frequency", attackTemplate_.cameraShake.frequency },
            { "priority", attackTemplate_.cameraShake.priority } } },
        { "weaponHitBox", { { "enabled", attackTemplate_.weaponHitBox.enabled },
            { "hitBoxName", attackTemplate_.weaponHitBox.hitBoxName },
            { "startNodeName", attackTemplate_.weaponHitBox.startNodeName },
            { "endNodeName", attackTemplate_.weaponHitBox.endNodeName },
            { "radius", attackTemplate_.weaponHitBox.radius },
            { "activeStartTime", attackTemplate_.weaponHitBox.activeStartTime },
            { "activeEndTime", attackTemplate_.weaponHitBox.activeEndTime } } },
        { "hitEffect", { { "spawnOnHit", attackTemplate_.hitEffect.spawnOnHit },
            { "hitParticleAssetPath", attackTemplate_.hitEffect.hitParticleAssetPath },
            { "hitParticleEmitterName", attackTemplate_.hitEffect.hitParticleEmitterName },
            { "hitParticleLifeTime", attackTemplate_.hitEffect.hitParticleLifeTime },
            { "hitParticleNormalOffset", attackTemplate_.hitEffect.hitParticleNormalOffset },
            { "hitParticleUseImpactAxis", attackTemplate_.hitEffect.hitParticleUseImpactAxis },
            { "hitParticleAxisToAttackerBlend", attackTemplate_.hitEffect.hitParticleAxisToAttackerBlend },
            { "hitParticleUseTargetAttraction", attackTemplate_.hitEffect.hitParticleUseTargetAttraction },
            { "hitParticleTargetAttractionStrength", attackTemplate_.hitEffect.hitParticleTargetAttractionStrength },
            { "hitParticleTargetAttractionRange", attackTemplate_.hitEffect.hitParticleTargetAttractionRange },
            { "hitParticleTargetAttractionFalloff", attackTemplate_.hitEffect.hitParticleTargetAttractionFalloff },
            { "hitMeshVfxAssetPath", attackTemplate_.hitEffect.hitMeshVfxAssetPath },
            { "hitMeshVfxLifeTime", attackTemplate_.hitEffect.hitMeshVfxLifeTime },
            { "hitMeshVfxNormalOffset", attackTemplate_.hitEffect.hitMeshVfxNormalOffset } } },
        { "slashEffects", slashEffects_ },
        { "attackEffects", attackEffects_ }
    };
    const std::filesystem::path path_ = WindowsDialogHelper::Utf8ToPath(NormalizeAttackTemplatePath(attackTemplate_.id));
    std::error_code error_;
    std::filesystem::create_directories(path_.parent_path(), error_);
    std::ofstream stream_(path_);
    if (!stream_) return false;
    stream_ << json_.dump(4);
    return true;
}

bool AttackTemplateManager::DuplicateTemplate(const AttackTemplate& source_, const std::string& newId_)
{
    if (newId_.empty() || FindTemplate(newId_)) return false;

    // 既存テンプレートをベースに、IDだけ差し替えた新規攻撃Assetを作る。
    AttackTemplate copy_ = source_;
    copy_.id = newId_;
    copy_.displayName = newId_;
    if (!SaveTemplate(copy_)) return false;
    templates.push_back(copy_);
    return true;
}

AttackTemplate* AttackTemplateManager::FindTemplate(const std::string& id_)
{
    const std::string resolvedId_ = ResolveRenamedTemplateId(id_);
    const auto iterator_ = std::find_if(templates.begin(), templates.end(), [&resolvedId_](const AttackTemplate& value_)
        {
            return value_.id == resolvedId_;
        });
    return iterator_ == templates.end() ? nullptr : &*iterator_;
}

const AttackTemplate* AttackTemplateManager::FindTemplate(const std::string& id_) const
{
    const std::string resolvedId_ = ResolveRenamedTemplateId(id_);
    const auto iterator_ = std::find_if(templates.begin(), templates.end(), [&resolvedId_](const AttackTemplate& value_)
        {
            return value_.id == resolvedId_;
        });
    return iterator_ == templates.end() ? nullptr : &*iterator_;
}

std::string AttackTemplateManager::ResolveRenamedTemplateId(
    const std::string& id_) const
{
    std::string resolvedId_ = id_;
    for (size_t depth_ = 0; depth_ < renamedTemplateIds.size(); ++depth_)
    {
        const auto iterator_ = renamedTemplateIds.find(resolvedId_);
        if (iterator_ == renamedTemplateIds.end() || iterator_->second == resolvedId_) break;
        resolvedId_ = iterator_->second;
    }

    return resolvedId_;
}

std::string AttackTemplateManager::NormalizeAttackTemplatePath(const std::string& pathOrId_) const
{
    // IDだけ渡された場合でもData/AttackTemplate/*.jsonとして扱えるようパスを正規化する。
    std::filesystem::path path_ = WindowsDialogHelper::Utf8ToPath(pathOrId_);
    if (path_.extension() != ".json") path_ += ".json";
    if (!path_.has_parent_path()) path_ = WindowsDialogHelper::Utf8ToPath("Data/AttackTemplate") / path_;
    return WindowsDialogHelper::PathToUtf8(path_.lexically_normal());
}

void AttackTemplateManager::OpenEditor(const std::string& id_)
{
    // Window版Editorを開く前に一覧を読み込み、編集対象IDとリネーム用バッファをそろえる。
    if (!loaded) LoadAll();
    editingTemplateId = id_;
    editingIdBuffer = id_;
    renamedTemplateId.clear();
    //editorOpen = true;
}

void AttackTemplateManager::DrawEditorGui()
{
    if (!editorOpen)
    {
        return;
    }

    if (!ImGui::Begin("Attack Template Editor", &editorOpen))
    {
        ImGui::End();
        return;
    }

    DrawEditorContent(editingTemplateId);
    if (!renamedTemplateId.empty())
    {
        editingTemplateId = renamedTemplateId;
        editingIdBuffer = renamedTemplateId;
        renamedTemplateId.clear();
    }

    ImGui::End();
}

void AttackTemplateManager::DrawInlineEditor(const std::string& id_)
{
    std::string mutableId_ = id_;
    DrawInlineEditor(mutableId_, nullptr);
}

void AttackTemplateManager::DrawInlineEditor(
    std::string& id_,
    GameObjectManager* gameObjectManager_)
{
    if (id_.empty())
    {
        ImGui::TextDisabled("No AttackTemplate selected.");
        return;
    }

    ImGui::PushID(("AttackTemplateInline_" + id_).c_str());

    // ComponentやAnimEventのInspector内から、対象テンプレートをその場で編集できる入口。
    ImGui::SeparatorText("Attack Template");
    DrawEditorContent(id_, gameObjectManager_);
    if (!renamedTemplateId.empty())
    {
        id_ = renamedTemplateId;
        editingTemplateId = renamedTemplateId;
        editingIdBuffer = renamedTemplateId;
        renamedTemplateId.clear();
    }

    ImGui::PopID();
}

bool AttackTemplateManager::DrawEditorContent(const std::string& id_)
{
    return DrawEditorContent(id_, nullptr);
}

bool AttackTemplateManager::DrawEditorContent(
    const std::string& id_,
    GameObjectManager* gameObjectManager_)
{
    if (!loaded)
    {
        LoadAll();
    }

    // 攻撃数値、武器HitBox、ヒット演出、派生攻撃エフェクトを1画面で編集する本体。
    AttackTemplate* attackTemplate_ = FindTemplate(id_);
    if (!attackTemplate_)
    {
        ImGui::TextDisabled("AttackTemplate not found: %s", id_.c_str());
        if (ImGui::Button("Create Missing AttackTemplate"))
        {
            AttackTemplate newTemplate_{};
            newTemplate_.id = id_;
            newTemplate_.displayName = id_;
            if (SaveTemplate(newTemplate_))
            {
                LoadAll();
            }
        }
        return false;
    }

    const std::string resolvedId_ = ResolveRenamedTemplateId(id_);
    if (editingTemplateId != resolvedId_ || editingIdBuffer.empty())
    {
        editingTemplateId = resolvedId_;
        editingIdBuffer = attackTemplate_->id;
    }

    bool saved_ = false;
    const std::vector<std::string> emptyAssets_{};
    EffectManager* effectManager_ = gameObjectManager_
        ? &gameObjectManager_->GetEffectManager()
        : nullptr;
    const std::vector<std::string>& particleAssets_ = gameObjectManager_
        ? gameObjectManager_->GetEffectManager().GetParticleAssetKeys()
        : emptyAssets_;
    std::vector<std::string> meshVfxAssets_ = gameObjectManager_
        ? gameObjectManager_->GetEffectManager().GetMeshVFXAssetKeys()
        : emptyAssets_;
    if (gameObjectManager_)
    {
        const std::vector<std::string>& sequenceAssets_ =
            gameObjectManager_->GetEffectManager().GetVFXSequenceAssetKeys();
        meshVfxAssets_.insert(
            meshVfxAssets_.end(),
            sequenceAssets_.begin(),
            sequenceAssets_.end());
    }
    const std::vector<std::string> hitBoxActionPrefixes_ =
        CollectHitBoxActionPrefixes(gameObjectManager_);

    // =========================================================
    // Basic
    // =========================================================
    if (ImGui::CollapsingHeader("Basic", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::InputText("ID", &editingIdBuffer);
        ImGui::InputText("Display Name", &attackTemplate_->displayName);

        int ownerTarget_ = static_cast<int>(attackTemplate_->ownerTarget);
        const char* ownerTargetNames_[] =
        {
            "Self",
            "Owner",
            "Weapon",
            "Target",
            "Lock On Target"
        };

        if (ImGui::Combo(
            "Owner Target",
            &ownerTarget_,
            ownerTargetNames_,
            IM_ARRAYSIZE(ownerTargetNames_)))
        {
            attackTemplate_->ownerTarget =
                static_cast<ATTACK_TEMPLATE_OWNER_TARGET>(ownerTarget_);
        }

        ImGui::Checkbox("Can Hit Owner", &attackTemplate_->canHitOwner);
    }

    // =========================================================
    // Damage
    // =========================================================
    if (ImGui::CollapsingHeader("AttackData", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::DragFloat(
            "Damage",
            &attackTemplate_->damage,
            0.1f,
            0.0f);

        ImGui::DragFloat(
            "Impact Power",
            &attackTemplate_->impactPower,
            0.1f,
            0.0f);

        ImGui::DragFloat(
            "Hit Stop Duration",
            &attackTemplate_->hitStopDuration,
            0.001f,
            0.0f,
            10.0f,
            "%.3f s");

        ImGui::DragFloat(
            "Hit Stop Scale",
            &attackTemplate_->hitStopScale,
            0.01f,
            0.0f,
            1.0f,
            "%.2fx");

        ImGui::DragFloat(
            "Knock Back Power",
            &attackTemplate_->knockBackPower,
            0.1f,
            0.0f);

        ImGui::DragFloat(
            "Target Air Lift Power",
            &attackTemplate_->targetAirLiftPower,
            0.1f,
            0.0f);

        ImGui::DragFloat(
            "Attacker Air Lift Power",
            &attackTemplate_->attackerAirLiftPower,
            0.1f,
            0.0f);

        int knockBackType_ =
            static_cast<int>(attackTemplate_->knockBackType);

        const char* knockBackNames_[] =
        {
            "Owner Forward",
            "Effect Forward",
            "Owner To Target",
            "Effect To Target"
        };

        if (ImGui::Combo(
            "Knock Back Type",
            &knockBackType_,
            knockBackNames_,
            IM_ARRAYSIZE(knockBackNames_)))
        {
            attackTemplate_->knockBackType =
                static_cast<KNOCK_BACK_TYPE>(knockBackType_);
        }

        ImGui::InputScalar(
            "Hit Group ID",
            ImGuiDataType_U32,
            &attackTemplate_->hitGroupId);

        ImGui::Checkbox(
            "Allow Multi Hit",
            &attackTemplate_->allowMultiHit);

        ImGui::SeparatorText("Camera Shake");
        ImGui::Checkbox("Camera Shake Enabled", &attackTemplate_->cameraShake.enabled);
        int cameraShakePowerLevel_ =
            static_cast<int>(attackTemplate_->cameraShake.powerLevel);
        const char* cameraShakePowerNames_[] =
        {
            "Minimum",
            "Medium",
            "Heavy"
        };
        if (ImGui::Combo(
            "Camera Shake Power",
            &cameraShakePowerLevel_,
            cameraShakePowerNames_,
            IM_ARRAYSIZE(cameraShakePowerNames_)))
        {
            attackTemplate_->cameraShake.powerLevel =
                static_cast<CAMERA_SHAKE_POWER_LEVEL>(
                    (std::clamp)(cameraShakePowerLevel_, 0, 2));
            ApplyCameraShakePowerLevel(attackTemplate_->cameraShake);
        }
        ImGui::TextDisabled(
            "Attacker shake uses half of receiver strength and duration.");
    }

    // =========================================================
    // Weapon HitBox
    // =========================================================
    if (ImGui::CollapsingHeader("Weapon HitBox", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox(
            "Enabled##WeaponHitBox",
            &attackTemplate_->weaponHitBox.enabled);

        DrawHitBoxActionPrefixCombo(
            "HitBox Action Prefix##WeaponHitBox",
            attackTemplate_->weaponHitBox.hitBoxName,
            hitBoxActionPrefixes_);

        ImGui::DragFloat(
            "Active Start##WeaponHitBox",
            &attackTemplate_->weaponHitBox.activeStartTime,
            0.01f,
            0.0f);

        ImGui::DragFloat(
            "Active End##WeaponHitBox",
            &attackTemplate_->weaponHitBox.activeEndTime,
            0.01f,
            0.0f);

        if (attackTemplate_->weaponHitBox.activeEndTime <
            attackTemplate_->weaponHitBox.activeStartTime)
        {
            attackTemplate_->weaponHitBox.activeEndTime =
                attackTemplate_->weaponHitBox.activeStartTime;
        }
    }

    // =========================================================
    // Hit Effect
    // =========================================================
    if (ImGui::CollapsingHeader("Hit Effect"))
    {
        ImGui::Checkbox(
            "Spawn On Hit",
            &attackTemplate_->hitEffect.spawnOnHit);

        DrawAssetPathCombo(
            "Particle Asset",
            attackTemplate_->hitEffect.hitParticleAssetPath,
            particleAssets_,
            effectManager_,
            true);

        ImGui::InputText(
            "Particle Emitter",
            &attackTemplate_->hitEffect.hitParticleEmitterName);
        ImGui::DragFloat(
            "Particle Life Time",
            &attackTemplate_->hitEffect.hitParticleLifeTime,
            0.01f,
            0.0f);
        ImGui::DragFloat(
            "Particle Normal Offset",
            &attackTemplate_->hitEffect.hitParticleNormalOffset,
            0.01f);
        ImGui::Checkbox(
            "Use Particle Impact Axis",
            &attackTemplate_->hitEffect.hitParticleUseImpactAxis);
        ImGui::BeginDisabled(!attackTemplate_->hitEffect.hitParticleUseImpactAxis);
        ImGui::SliderFloat(
            "Axis To Attacker Blend",
            &attackTemplate_->hitEffect.hitParticleAxisToAttackerBlend,
            0.0f,
            1.0f);
        ImGui::EndDisabled();
        ImGui::Checkbox(
            "Use Particle Target Pull",
            &attackTemplate_->hitEffect.hitParticleUseTargetAttraction);
        ImGui::BeginDisabled(!attackTemplate_->hitEffect.hitParticleUseTargetAttraction);
        ImGui::DragFloat(
            "Target Pull Strength",
            &attackTemplate_->hitEffect.hitParticleTargetAttractionStrength,
            0.01f,
            -100.0f,
            100.0f);
        ImGui::DragFloat(
            "Target Pull Range",
            &attackTemplate_->hitEffect.hitParticleTargetAttractionRange,
            0.01f,
            0.0f,
            100.0f);
        ImGui::DragFloat(
            "Target Pull Falloff",
            &attackTemplate_->hitEffect.hitParticleTargetAttractionFalloff,
            0.01f,
            0.001f,
            8.0f);
        ImGui::EndDisabled();

        DrawAssetPathCombo(
            "Mesh VFX / Sequence Asset",
            attackTemplate_->hitEffect.hitMeshVfxAssetPath,
            meshVfxAssets_,
            effectManager_,
            false);
        ImGui::DragFloat(
            "Mesh VFX Life Time",
            &attackTemplate_->hitEffect.hitMeshVfxLifeTime,
            0.01f,
            0.0f);
        ImGui::DragFloat(
            "Mesh VFX Normal Offset",
            &attackTemplate_->hitEffect.hitMeshVfxNormalOffset,
            0.01f);
    }

    // =========================================================
    // Slash Effects
    // =========================================================
    if (ImGui::CollapsingHeader("Slash Effects", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::Button("Add Slash Effect"))
        {
            attackTemplate_->slashEffects.emplace_back();
        }

        int removeIndex_ = -1;
        int duplicateIndex_ = -1;

        for (int slashIndex_ = 0;
            slashIndex_ < static_cast<int>(attackTemplate_->slashEffects.size());
            ++slashIndex_)
        {
            SlashEffectTemplate& slash_ =
                attackTemplate_->slashEffects[slashIndex_];

            ImGui::PushID(slashIndex_);

            std::string slashLabel_ =
                "Slash Effect " + std::to_string(slashIndex_ + 1);

            if (ImGui::TreeNodeEx(
                "SlashEffectNode",
                ImGuiTreeNodeFlags_DefaultOpen,
                "%s",
                slashLabel_.c_str()))
            {
                ImGui::Checkbox("Enabled", &slash_.enabled);

                DrawAssetPathCombo("VFX Path", slash_.vfxPath, meshVfxAssets_, effectManager_, false);

                ImGui::DragFloat(
                    "Spawn Offset",
                    &slash_.spawnTimeOffset,
                    0.01f,
                    0.0f);

                ImGui::DragFloat(
                    "Life Time",
                    &slash_.lifeTime,
                    0.01f,
                    0.0f);

                DrawVector3("Local Offset", slash_.localOffset);
                DrawVector3("Local Rotation", slash_.localRotation);
                DrawVector3("Local Scale", slash_.localScale);

                ImGui::DragFloat(
                    "Alpha",
                    &slash_.alpha,
                    0.01f,
                    0.0f,
                    1.0f);

                ImGui::Checkbox("Follow Owner", &slash_.followOwner);
                ImGui::Checkbox("Follow Node", &slash_.followNode);

                ImGui::InputText("Base Node", &slash_.baseNodeName);

                if (ImGui::TreeNodeEx(
                    "SlashHitBoxNode",
                    ImGuiTreeNodeFlags_DefaultOpen,
                    "HitBox"))
                {
                    ImGui::Checkbox(
                        "Enabled##SlashHitBox",
                        &slash_.hitBox.enabled);

                    DrawHitBoxActionPrefixCombo(
                        "HitBox Action Prefix##SlashHitBox",
                        slash_.hitBox.hitBoxName,
                        hitBoxActionPrefixes_);

                    int shape_ = static_cast<int>(slash_.hitBox.shape);
                    const char* shapeNames_[] =
                    {
                        "None",
                        "Box",
                        "Capsule",
                        "Sphere Group",
                        "Fan"
                    };

                    if (ImGui::Combo(
                        "Shape",
                        &shape_,
                        shapeNames_,
                        IM_ARRAYSIZE(shapeNames_)))
                    {
                        slash_.hitBox.shape =
                            static_cast<EFFECT_HIT_SHAPE>(shape_);
                    }

                    ImGui::DragFloat(
                        "Active Start",
                        &slash_.hitBox.activeStartTime,
                        0.01f,
                        0.0f);

                    ImGui::DragFloat(
                        "Active End",
                        &slash_.hitBox.activeEndTime,
                        0.01f,
                        0.0f);

                    if (slash_.hitBox.activeEndTime <
                        slash_.hitBox.activeStartTime)
                    {
                        slash_.hitBox.activeEndTime =
                            slash_.hitBox.activeStartTime;
                    }

                    DrawVector3(
                        "Hit Local Offset",
                        slash_.hitBox.localOffset);

                    DrawVector3(
                        "Hit Local Rotation",
                        slash_.hitBox.localRotation);

                    DrawVector3(
                        "Hit Local Scale",
                        slash_.hitBox.localScale);

                    ImGui::DragFloat(
                        "Radius",
                        &slash_.hitBox.radius,
                        0.01f,
                        0.0f);

                    ImGui::DragFloat(
                        "Half Height",
                        &slash_.hitBox.halfHeight,
                        0.01f,
                        0.0f);

                    ImGui::TreePop();
                }

                if (ImGui::Button("Duplicate Slash"))
                {
                    duplicateIndex_ = slashIndex_;
                }

                ImGui::SameLine();

                if (ImGui::Button("Remove Slash"))
                {
                    removeIndex_ = slashIndex_;
                }

                ImGui::TreePop();
            }

            ImGui::PopID();
        }

        if (duplicateIndex_ >= 0 &&
            duplicateIndex_ < static_cast<int>(attackTemplate_->slashEffects.size()))
        {
            SlashEffectTemplate copy_ =
                attackTemplate_->slashEffects[duplicateIndex_];

            attackTemplate_->slashEffects.insert(
                attackTemplate_->slashEffects.begin() + duplicateIndex_ + 1,
                copy_);
        }

        if (removeIndex_ >= 0 &&
            removeIndex_ < static_cast<int>(attackTemplate_->slashEffects.size()))
        {
            attackTemplate_->slashEffects.erase(
                attackTemplate_->slashEffects.begin() + removeIndex_);
        }
    }

    if (ImGui::CollapsingHeader("Attack Effects", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::Button("Add Attack Effect")) attackTemplate_->attackEffects.emplace_back();
        int removeEffectIndex_ = -1;
        int duplicateEffectIndex_ = -1;
        for (int effectIndex_ = 0; effectIndex_ < static_cast<int>(attackTemplate_->attackEffects.size()); ++effectIndex_)
        {
            AttackEffectSpawnTemplate& effect_ = attackTemplate_->attackEffects[effectIndex_];
            ImGui::PushID(effectIndex_);
            if (ImGui::TreeNode("AttackEffect", "Attack Effect %d", effectIndex_ + 1))
            {
                ImGui::Checkbox("Enabled", &effect_.enabled);
                int effectType_ = static_cast<int>(effect_.effectType);
                const char* effectNames_[] = { "Instant Area", "Projectile", "Area Over Time", "Beam", "Breath" };
                if (ImGui::Combo("Effect Type", &effectType_, effectNames_, IM_ARRAYSIZE(effectNames_)))
                    effect_.effectType = static_cast<ATTACK_EFFECT_TYPE>(effectType_);
                DrawAssetPathCombo("Particle Path", effect_.particlePath, particleAssets_, effectManager_, true);
                DrawAssetPathCombo("Mesh VFX Path", effect_.meshVfxPath, meshVfxAssets_, effectManager_, false);
                ImGui::DragFloat("Spawn Time Offset", &effect_.spawnTimeOffset, 0.01f, 0.0f);
                ImGui::DragFloat("Life Time", &effect_.lifeTime, 0.01f, 0.0f);
                DrawVector3("Local Offset", effect_.localOffset);
                DrawVector3("Local Rotation", effect_.localRotation);
                DrawVector3("Local Scale", effect_.localScale);
                ImGui::Checkbox("Follow Owner", &effect_.followOwner);
                ImGui::Checkbox("Follow Node", &effect_.followNode);
                ImGui::InputText("Base Node", &effect_.baseNodeName);
                ImGui::DragFloat("Speed", &effect_.speed, 0.1f, 0.0f);
                DrawVector3("Local Direction", effect_.localDirection);
                if (ImGui::Button("Add Hit Shape")) effect_.hitShapes.emplace_back();
                int removeShapeIndex_ = -1;
                for (int shapeIndex_ = 0; shapeIndex_ < static_cast<int>(effect_.hitShapes.size()); ++shapeIndex_)
                {
                    AttackEffectHitShapeTemplate& shape_ = effect_.hitShapes[shapeIndex_];
                    ImGui::PushID(shapeIndex_);
                    if (ImGui::TreeNode("HitShape", "Hit Shape %d", shapeIndex_ + 1))
                    {
                        shape_.shape = NormalizeAttackHitShape(shape_.shape);
                        ImGui::Checkbox("Enabled", &shape_.enabled);
                        DrawHitBoxActionPrefixCombo(
                            "HitBox Action Prefix",
                            shape_.hitBoxName,
                            hitBoxActionPrefixes_);
                        ATTACK_HIT_SHAPE supportedShapes_[] = {
                            ATTACK_HIT_SHAPE::NONE,
                            ATTACK_HIT_SHAPE::SPHERE,
                            ATTACK_HIT_SHAPE::CAPSULE,
                            ATTACK_HIT_SHAPE::SPHERE_GROUP
                        };
                        const char* shapeNames_[] = { "None", "Sphere", "Capsule", "Sphere Group" };
                        int shapeType_ = 0;
                        for (int index_ = 0; index_ < IM_ARRAYSIZE(supportedShapes_); ++index_)
                        {
                            if (supportedShapes_[index_] == shape_.shape)
                            {
                                shapeType_ = index_;
                                break;
                            }
                        }
                        if (ImGui::Combo("Shape", &shapeType_, shapeNames_, IM_ARRAYSIZE(shapeNames_)))
                            shape_.shape = supportedShapes_[shapeType_];
                        DrawVector3("Hit Offset", shape_.localOffset);
                        DrawVector3("Hit Rotation", shape_.localRotation);
                        DrawVector3("Hit Scale", shape_.localScale);
                        ImGui::DragFloat("Radius", &shape_.radius, 0.01f, 0.01f);
                        if (shape_.shape == ATTACK_HIT_SHAPE::CAPSULE)
                            ImGui::DragFloat("Half Height", &shape_.halfHeight, 0.01f, 0.0f);
                        ImGui::DragFloat("Active Start", &shape_.activeStartTime, 0.01f, 0.0f);
                        ImGui::DragFloat("Active End", &shape_.activeEndTime, 0.01f, shape_.activeStartTime);
                        ImGui::DragFloat("Hit Interval", &shape_.hitInterval, 0.01f, 0.0f);
                        ImGui::Checkbox("Destroy On Hit", &shape_.destroyOnHit);
                        ImGui::Checkbox("Allow Multi Hit", &shape_.allowMultiHit);
                        if (shape_.shape == ATTACK_HIT_SHAPE::SPHERE_GROUP)
                        {
                            if (ImGui::Button("Add Sphere")) shape_.spheres.emplace_back();
                            int removeSphereIndex_ = -1;
                            for (int sphereIndex_ = 0; sphereIndex_ < static_cast<int>(shape_.spheres.size()); ++sphereIndex_)
                            {
                                ImGui::PushID(sphereIndex_);
                                ImGui::Text("Sphere %d", sphereIndex_ + 1);
                                DrawVector3("Sphere Offset", shape_.spheres[sphereIndex_].localOffset);
                                ImGui::DragFloat("Sphere Radius", &shape_.spheres[sphereIndex_].radius, 0.01f, 0.01f);
                                if (ImGui::Button("Remove Sphere")) removeSphereIndex_ = sphereIndex_;
                                ImGui::PopID();
                            }
                            if (removeSphereIndex_ >= 0)
                                shape_.spheres.erase(shape_.spheres.begin() + removeSphereIndex_);
                        }
                        if (ImGui::Button("Remove Hit Shape")) removeShapeIndex_ = shapeIndex_;
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }
                if (removeShapeIndex_ >= 0) effect_.hitShapes.erase(effect_.hitShapes.begin() + removeShapeIndex_);
                if (ImGui::Button("Duplicate Effect")) duplicateEffectIndex_ = effectIndex_;
                ImGui::SameLine();
                if (ImGui::Button("Remove Effect")) removeEffectIndex_ = effectIndex_;
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
        if (duplicateEffectIndex_ >= 0)
            attackTemplate_->attackEffects.insert(
                attackTemplate_->attackEffects.begin() + duplicateEffectIndex_ + 1,
                attackTemplate_->attackEffects[duplicateEffectIndex_]);
        if (removeEffectIndex_ >= 0)
            attackTemplate_->attackEffects.erase(attackTemplate_->attackEffects.begin() + removeEffectIndex_);
    }

    ImGui::Separator();

    if (ImGui::Button("Save Attack Template"))
    {
        const std::string oldId_ = attackTemplate_->id;
        const std::string requestedId_ = editingIdBuffer.empty() ? oldId_ : editingIdBuffer;
        const AttackTemplate* duplicate_ = FindTemplate(requestedId_);
        if (requestedId_ != oldId_ && duplicate_ != nullptr)
        {
            ImGui::OpenPopup("AttackTemplateIdConflict");
        }
        else
        {
            attackTemplate_->id = requestedId_;
            saved_ = SaveTemplate(*attackTemplate_);
            if (saved_)
            {
                if (requestedId_ != oldId_)
                {
                    ReplaceAttackTemplateReferences(oldId_, requestedId_);
                    std::string oldPath_ = NormalizeAttackTemplatePath(oldId_);
                    std::string newPath_ = NormalizeAttackTemplatePath(requestedId_);
                    std::transform(
                        oldPath_.begin(), oldPath_.end(), oldPath_.begin(),
                        [](unsigned char character_) { return static_cast<char>(std::tolower(character_)); });
                    std::transform(
                        newPath_.begin(), newPath_.end(), newPath_.begin(),
                        [](unsigned char character_) { return static_cast<char>(std::tolower(character_)); });
                    if (oldPath_ != newPath_)
                    {
                        std::error_code error_;
                        std::filesystem::remove(
                            WindowsDialogHelper::Utf8ToPath(NormalizeAttackTemplatePath(oldId_)),
                            error_);
                    }
                    renamedTemplateIds[oldId_] = requestedId_;
                    renamedTemplateId = requestedId_;
                }
                editingTemplateId = requestedId_;
            }
            else
            {
                attackTemplate_->id = oldId_;
            }
        }
    }

    if (ImGui::BeginPopup("AttackTemplateIdConflict"))
    {
        ImGui::TextDisabled("That AttackTemplate ID already exists.");
        if (ImGui::Button("OK")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    ImGui::SameLine();

    if (ImGui::Button("Reload Attack Templates"))
    {
        LoadAll();
    }

    if (saved_)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("Saved.");
    }

    return true;
}

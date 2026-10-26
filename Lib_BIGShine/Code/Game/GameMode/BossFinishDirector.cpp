#include "PrecompiledHeader.h"
#include "BossFinishDirector.h"
#include "GameModeManager.h"
#include "Engine/Common/DebugLog.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Camera/CameraManager.h"
#include "Engine/GamePlay/Camera/CameraComponent.h"
#include "Engine/GamePlay/Transform/ModelAttachmentComponent.h"
#include "Engine/GamePlay/Input/KeyConfig.h"
#include "Engine/GamePlay/System/TimeManager.h"
#include "Engine/GamePlay/Combat/HealthComponent.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Graphics/RenderSystem/SpriteRenderSystem.h"
#include "Engine/Graphics/Particle/ParticleComponent.h"
#include "Engine/Graphics/VFX/MeshVFXComponent.h"
#include "Engine/UI/UITextComponent.h"
#include "Engine/Audio/AudioManager.h"
#include "Engine/Math/Curve/CurveTrack.h"
#include "Engine/Platform/WindowsDialogHelper.h"
#include <fstream>
#include <cmath>
#include <algorithm>
#include <filesystem>

namespace
{
    constexpr const char* PRESET_PATH = "Data/Presentation/AnathemaBossFinish.json";
    constexpr float FIXED_STEP = 1.0f / 60.0f;

    DirectX::XMFLOAT3 ReadPosition(const nlohmann::json& json_)
    {
        return { json_.at(0).get<float>(), json_.at(1).get<float>(), json_.at(2).get<float>() };
    }

    DirectX::XMFLOAT4 ReadColor(const nlohmann::json& json_)
    {
        return { json_.at(0).get<float>(), json_.at(1).get<float>(), json_.at(2).get<float>(), json_.at(3).get<float>() };
    }

    DirectX::XMFLOAT3 AddPosition(const DirectX::XMFLOAT3& a_, const DirectX::XMFLOAT3& b_)
    {
        return { a_.x + b_.x, a_.y + b_.y, a_.z + b_.z };
    }

    // 装備を含む更新済みモデルから、指定ノードのワールド位置を取得する。
    bool ReadNodePosition(const std::shared_ptr<GameObject>& object_, const std::string& name_, DirectX::XMFLOAT3& position_)
    {
        const auto model_ = object_ ? object_->GetComponent<GltfModelCommponennt>() : nullptr;
        if (!model_ || !model_->IsModelReady()) return false;
        for (const auto& node_ : model_->GetNodes())
            if (node_.name == name_)
            {
                position_ = {node_.worldTransform._41, node_.worldTransform._42, node_.worldTransform._43};
                return std::isfinite(position_.x) && std::isfinite(position_.y) && std::isfinite(position_.z);
            }
        return false;
    }

    // 演出中は通常移動を止め、ポーズ評価の前に配置を確定する。
    void PlaceActor(const std::shared_ptr<GameObject>& object_, const DirectX::XMFLOAT3& position_, float yaw_)
    {
        if (!object_) return;
        object_->transform->localPosition = position_;
        object_->transform->localRotation = {0, yaw_, 0};
        object_->transform->moveOffset = {}; object_->transform->rotateOffset = {};
        object_->transform->MarkDirty(); object_->transform->UpdateWorldMatrix();
    }

    std::shared_ptr<GameObject> MakeObject(const char* name_)
    {
        // Sceneへ登録せず演出が所有し、通常更新とScene保存の対象へ混入させない。
        auto object_ = std::make_shared<GameObject>(name_);
        object_->saveFlg = false;
        object_->transform->gameObject = object_;
        return object_;
    }

    nlohmann::json DefaultSettings()
    {
        return nlohmann::json::parse(R"JSON({
  "Version": 1,
  "Anchor": [0.0, -0.12, 0.0],
  "Aspect": 1.7777778,
  "HitStop": 0.16,
  "ImpactHold": 0.16,
  "BlackoutEnd": 0.65,
  "CounterStart": 2.8,
  "CaptureTime": 4.25,
  "HoldTime": 1.3,
  "PromptDelay": 2.8,
  "ReturnFade": 0.65,
  "Camera": {"Position": [6.6, 3.2, -8.5], "Focus": [0.0, 1.5, 0.6], "Fov": 43.0, "Roll": -5.0, "Shake": 0.12, "ImpactShake": 0.44, "ImpactShakeDuration": 0.18, "ImpactRoll": 3.5, "ImpactFovKick": 5.0},
  "Player": {"Object": "PlayerModel", "StateAsset": "Data/Animation/StateData/Player/States/Run_Attack_01_Seq.json", "Position": [-1.6, 0.0, -3.5], "EndPosition": [-1.1, 0.0, -0.9], "Yaw": 10.0, "PoseTime": 0.85, "WindupTime": 0.35, "WindupSpeed": 0.22, "StrikeSpeed": 1.8, "HitPoseTime": 0.35, "WeaponObject": "Weapon", "BladeStartNode": "TrailStart", "BladeEndNode": "TrailEnd"},
  "Boss": {"Object": "HumanBoss", "StateAsset": "Data/Animation/StateData/HumanBoss/States/HumanBossBerserkDownLoop.json", "Position": [0.3, 0.0, 1.6], "Yaw": 180.0, "PoseTime": 0.8, "WindupPoseTime": 0.18, "HitNode": "spine_02", "HitRadius": 0.65, "ReactionAnimation": "Knock_Down_Combat_Loop_Seq", "ReactionSpeed": 1.0, "ReactionPoseTime": 1.8, "KeepPoseAnimation": true},
  "Equipment": ["Weapon", "Shield", "HumanBossWeapon", "HumanBossShield"],
  "Background": [0.009, 0.003, 0.018, 1.0],
  "ImageTint": [0.94, 1.0, 0.97, 1.0],
  "ZoomStart": 1.0,
  "ZoomEnd": 1.045,
  "ClearText": "討伐完了",
  "PromptText": "ボタンを押してタイトルへ",
  "ClearPosition": [0.5, 0.76],
  "PromptPosition": [0.5, 0.9],
  "ConfirmAction": "UI.MenuConfirm",
  "Effects": [
    {"Kind": "Particle", "Asset": "Data/Particle/AnathemaFinishMudBurst.json", "Position": [0.3, 1.6, 1.6], "Rotation": [0.0, 0.0, 0.0], "Scale": [1.0, 1.0, 1.0], "Start": 0.0, "Stop": 0.85, "PoseTime": 0.8, "Color": [1.0, 1.0, 1.0, 1.0], "Emission": 1.0, "Anchor": "Impact", "Offset": [0.0, 0.0, 0.0], "TimeAnchor": "Impact", "RequiresImpact": true},
    {"Kind": "Particle", "Asset": "Data/Particle/AnathemaFinishMudSlough.json", "Position": [0.3, 1.1, 1.6], "Rotation": [0.0, 0.0, 0.0], "Scale": [1.0, 1.0, 1.0], "Start": 0.08, "Stop": 1.25, "PoseTime": 1.1, "Color": [1.0, 1.0, 1.0, 1.0], "Emission": 1.0, "Anchor": "Boss", "Offset": [0.0, 0.0, 0.0], "TimeAnchor": "Impact", "RequiresImpact": true, "Node": "spine_02"},
    {"Kind": "Mesh", "Asset": "Data/VFX/TransitionMudPool.json", "Position": [0.3, 0.05, 1.6], "Rotation": [0.0, 0.0, 0.0], "Scale": [1.4, 1.0, 1.4], "Start": 0.7, "Stop": 4.25, "PoseTime": 2.5, "Color": [0.08, 0.025, 0.11, 1.0], "Emission": 1.0, "Anchor": "Boss", "Offset": [0.0, 0.05, 0.0]},
    {"Kind": "Mesh", "Asset": "Data/VFX/TransitionMudVortex.json", "Position": [0.3, 0.4, 1.6], "Rotation": [0.0, 0.0, 0.0], "Scale": [0.65, 1.3, 0.65], "Start": 1.1, "Stop": 4.25, "PoseTime": 2.4, "Color": [0.08, 0.02, 0.12, 1.0], "Emission": 1.0, "Anchor": "Boss", "Offset": [0.0, 0.4, 0.0]},
    {"Kind": "Mesh", "Asset": "Data/VFX/AnathemaFinishCross.json", "Position": [0.0, 1.7, 1.0], "Rotation": [0.0, 0.0, 0.0], "Scale": [1.0, 1.0, 1.0], "Start": 0.0, "Stop": 0.4, "PoseTime": 0.35, "Color": [0.83, 1.0, 0.89, 1.0], "Emission": 5.0, "Anchor": "Impact", "Offset": [0.0, 0.0, 0.0], "TimeAnchor": "Impact", "RequiresImpact": true}
  ]
}
)JSON");
    }
}

BossFinishDirector::BossFinishDirector(GameObjectManager& manager_) : manager(manager_), settings(DefaultSettings())
{
    Load();
}

BossFinishDirector::~BossFinishDirector()
{
    Stop();
}

float BossFinishDirector::GetCaptureTime() const { return settings.value("CaptureTime", 3.7f); }
float BossFinishDirector::GetAspect() const { return settings.value("Aspect", 16.0f / 9.0f); }
float BossFinishDirector::GetImpactCueTime() const { return settings.value("HitStop", 0.16f) + GetImpactTime(); }

void BossFinishDirector::SetPreviewPlaybackRate(float rate_)
{
    previewPlaybackRate = std::isfinite(rate_) ? std::clamp(rate_, 0.1f, 2.0f) : 1.0f;
}

BossFinishDirector::PreviewActorInfo BossFinishDirector::GetPreviewActorInfo(bool boss_) const
{
    const auto& actor_ = boss_ && !settings.at("Boss").value("KeepPoseAnimation", false) &&
        impactConfirmed && elapsed > GetImpactCueTime() + settings.value("ImpactHold", 0.16f) ? bossReaction : boss_ ? boss : player;
    PreviewActorInfo info_;
    info_.ready = actor_.ready;
    if (!actor_.ready || !actor_.object) return info_;
    const auto model_ = actor_.object->GetComponent<GltfModelCommponennt>();
    if (!model_ || actor_.state.animationIndex < 0 || actor_.state.animationIndex >= static_cast<int>(model_->GetAnimations().size())) return info_;
    info_.clipName = actor_.state.type == AnimStateType::SEQUENCE ? actor_.state.name + " (Sequence)" : model_->GetAnimations()[actor_.state.animationIndex].name;
    info_.duration = actor_.animator.GetCurrentSampleDuration();
    info_.sampleTime = actor_.animator.GetCurrentFadeTime();
    return info_;
}
DirectX::XMFLOAT4 BossFinishDirector::GetBackgroundColor() const { return ReadColor(settings.at("Background")); }

void BossFinishDirector::Normalize()
{
    // Editor入力でもゼロ除算や無期限のシークを作らず、演出の時間順序を保つ。
    const auto clamp_ = [&](const char* key_, float minimum_, float maximum_)
    {
        float value_ = settings.value(key_, minimum_);
        settings[key_] = std::isfinite(value_) ? std::clamp(value_, minimum_, maximum_) : minimum_;
    };
    clamp_("Aspect", 0.5f, 3.5f);
    clamp_("HitStop", 0.0f, 1.0f);
    clamp_("BlackoutEnd", settings["HitStop"].get<float>() + 0.05f, 3.0f);
    clamp_("CounterStart", settings["BlackoutEnd"].get<float>(), 15.0f);
    clamp_("CaptureTime", settings["CounterStart"].get<float>() + 0.1f, 25.0f);
    clamp_("HoldTime", 0.0f, 10.0f);
    clamp_("PromptDelay", settings["HoldTime"].get<float>(), 20.0f);
    clamp_("ReturnFade", 0.1f, 3.0f);
    clamp_("ZoomStart", 1.0f, 1.5f);
    clamp_("ZoomEnd", 1.0f, 1.5f);
    settings["Camera"]["Fov"] = std::clamp(settings["Camera"].value("Fov", 43.0f), 15.0f, 100.0f);
    clamp_("ImpactHold", 0.0f, 0.5f);
    auto& player_ = settings["Player"];
    player_["WindupTime"] = std::clamp(player_.value("WindupTime", 0.35f), 0.01f, 3.0f);
    player_["WindupSpeed"] = std::clamp(player_.value("WindupSpeed", 0.22f), 0.01f, 5.0f);
    player_["StrikeSpeed"] = std::clamp(player_.value("StrikeSpeed", 1.8f), 0.01f, 5.0f);
    player_["HitPoseTime"] = std::clamp(player_.value("HitPoseTime", 0.35f), 0.01f, 3.0f);
    player_["PoseTime"] = (std::max)(player_.value("PoseTime", 0.85f), player_["HitPoseTime"].get<float>());
    settings["CaptureTime"] = (std::max)(GetCaptureTime(), GetImpactTime() + settings["ImpactHold"].get<float>() + 0.15f);
    // キーが無い従来データは固定カメラのまま扱い、新規キーだけを時刻順に整える。
    auto& camera_ = settings["Camera"];
    if (camera_.contains("Keys") && camera_["Keys"].is_array())
    {
        std::vector<nlohmann::json> keys_;
        for (auto key_ : camera_["Keys"])
        {
            if (!key_.is_object()) continue;
            const float time_ = key_.value("Time", 0.0f);
            key_["Time"] = std::isfinite(time_) ? std::clamp(time_, 0.0f, GetCaptureTime() + settings["HitStop"].get<float>()) : 0.0f;
            for (const char* property_ : { "Position", "Focus" })
            {
                if (!key_.contains(property_) || !key_[property_].is_array() || key_[property_].size() != 3)
                    key_[property_] = camera_[property_];
                for (std::size_t index_ = 0; index_ < 3; ++index_)
                    if (!key_[property_][index_].is_number() || !std::isfinite(key_[property_][index_].get<float>()))
                        key_[property_][index_] = camera_[property_][index_];
            }
            const float fov_ = key_.value("Fov", camera_.value("Fov", 43.0f));
            const float roll_ = key_.value("Roll", camera_.value("Roll", 0.0f));
            key_["Fov"] = std::isfinite(fov_) ? std::clamp(fov_, 15.0f, 100.0f) : 43.0f;
            key_["Roll"] = std::isfinite(roll_) ? std::clamp(roll_, -180.0f, 180.0f) : 0.0f;
            if (!key_.contains("Easing") || !key_["Easing"].is_string()) key_["Easing"] = "Smooth";
            keys_.push_back(std::move(key_));
        }
        std::stable_sort(keys_.begin(), keys_.end(), [](const auto& left_, const auto& right_) { return left_.at("Time").template get<float>() < right_.at("Time").template get<float>(); });
        camera_["Keys"] = keys_;
    }
}

float BossFinishDirector::GetImpactTime() const
{
    const auto& config_ = settings.at("Player");
    const float windup_ = config_.value("WindupTime", 0.35f);
    const float speed_ = config_.value("WindupSpeed", 0.22f);
    const float pose_ = config_.value("HitPoseTime", 0.35f);
    const float offset_ = pose_ <= windup_ * speed_ ? pose_ / (std::max)(speed_, 0.01f)
        : windup_ + (pose_ - windup_ * speed_) / (std::max)(config_.value("StrikeSpeed", 1.8f), 0.01f);
    return settings.at("CounterStart").get<float>() + offset_;
}

bool BossFinishDirector::Load()
{
    if (LoadPreset(PRESET_PATH)) return true;
    settings = DefaultSettings();
    return false;
}

bool BossFinishDirector::LoadPreset(const std::string& path_)
{
    BS_PROFILE_SCOPE("BossFinishDirector::LoadPreset");
    const auto previous_ = settings;
    try
    {
        std::ifstream file_(WindowsDialogHelper::Utf8ToPath(path_));
        if (!file_) throw std::runtime_error("preset file missing");
        nlohmann::json loaded_;
        file_ >> loaded_;
        if (!loaded_.is_object() || (loaded_.contains("AssetType") && loaded_.value("AssetType", std::string()) != "BossFinishCinematic"))
            throw std::runtime_error("not a boss finish cinematic preset");
        auto merged_ = DefaultSettings();
        merged_.merge_patch(loaded_);
        // 必須の配列構造を検証し、読込に失敗した演出案では現在の編集内容を維持する。
        ReadPosition(merged_.at("Anchor"));
        for (const char* actor_ : { "Player", "Boss" }) ReadPosition(merged_.at(actor_).at("Position"));
        ReadPosition(merged_.at("Camera").at("Position"));
        ReadPosition(merged_.at("Camera").at("Focus"));
        ReadColor(merged_.at("Background"));
        ReadColor(merged_.at("ImageTint"));
        for (const auto& effect_ : merged_.at("Effects"))
        {
            ReadPosition(effect_.at("Position")); ReadPosition(effect_.at("Rotation"));
            ReadPosition(effect_.at("Scale")); ReadColor(effect_.at("Color"));
            if (effect_.contains("Anchor")) ReadPosition(effect_.at("Offset"));
        }
        settings = std::move(merged_);
        Normalize();
        status = std::string("Loaded: ") + path_;
        return true;
    }
    catch (const std::exception& error_)
    {
        settings = previous_;
        status = std::string("Finish preset fallback: ") + error_.what();
        BS_LOG_INFO(LOG_CATEGORY::GAME, status);
        return false;
    }
}

bool BossFinishDirector::Save()
{
    return SavePreset(PRESET_PATH);
}

bool BossFinishDirector::SavePreset(const std::string& path_)
{
    BS_PROFILE_SCOPE("BossFinishDirector::SavePreset");
    try
    {
        Normalize();
        const auto destination_ = WindowsDialogHelper::Utf8ToPath(path_);
        if (destination_.empty()) return false;
        if (!destination_.parent_path().empty()) std::filesystem::create_directories(destination_.parent_path());
        std::ofstream file_(destination_, std::ios::binary);
        // Windowsでも明示的にCRLFを書き、UTF-8文字列へBOMを付与しない。
        auto asset_ = settings;
        asset_["AssetType"] = "BossFinishCinematic";
        asset_["AssetVersion"] = 1;
        const auto text_ = asset_.dump(4, ' ', false);
        for (const char character_ : text_) { if (character_ == '\n') file_.put('\r'); file_.put(character_); }
        file_ << "\r\n";
        file_.flush();
        status = file_ ? "Finish preset saved." : "Finish preset save failed.";
        return static_cast<bool>(file_);
    }
    catch (const std::exception& error_)
    {
        status = std::string("Finish preset save failed: ") + error_.what();
        return false;
    }
}

void BossFinishDirector::PrepareActor(ActorPose& actor_, const char* key_)
{
    const auto& config_ = settings.at(key_);
    actor_.object = manager.Find(config_.value("Object", std::string()));
    actor_.ready = false;
    try
    {
        std::ifstream file_(WindowsDialogHelper::Utf8ToPath(config_.at("StateAsset").get<std::string>()));
        nlohmann::json data_;
        if (!file_) throw std::runtime_error("animation file missing");
        file_ >> data_;
        actor_.state = data_.at("State").get<AnimState>();
        const auto model_ = actor_.object ? actor_.object->GetComponent<GltfModelCommponennt>() : nullptr;
        if (!model_ || !model_->IsModelReady() || actor_.state.animationIndex < 0 ||
            actor_.state.animationIndex >= static_cast<int>(model_->GetAnimations().size()))
            throw std::runtime_error("actor or animation index unavailable");
        // カウンターの見た目を再利用し、ダメージ・ActionSequence・VFXの二重生成は起こさない。
        actor_.state.events.clear();
        actor_.state.isLoop = false;
        actor_.state.playbackSpeed = 0.0f;
        actor_.state.rootMotion.enable = false;
        actor_.state.rootMotion.cutVisualWhenDisabled = true;
        actor_.state.rootMotion.cutVisualPositionX = true;
        actor_.state.rootMotion.cutVisualPositionZ = true;
        actor_.animator = ModelAnimator{};
        actor_.animator.CrossFadeTo(&actor_.state, 0.0f);
        actor_.ready = true;
        model_->SetRuntimeVisible(true);
        SampleActor(actor_, 0.0f);
    }
    catch (const std::exception& error_)
    {
        status = std::string("Finish actor fallback (") + key_ + "): " + error_.what();
        BS_LOG_INFO(LOG_CATEGORY::GAME, status);
    }
}

void BossFinishDirector::SampleActor(ActorPose& actor_, float time_)
{
    if (!actor_.ready || !actor_.object) return;
    const auto model_ = actor_.object->GetComponent<GltfModelCommponennt>();
    if (!model_) return;
    actor_.animator.SetCurrentTime((std::max)(time_, 0.0f));
    actor_.animator.Update(0.0f, model_.get(), nullptr);
    model_->SetNodePoses(actor_.animator.GetCurrentPose());
    model_->UpdateTransform();
}

void BossFinishDirector::Begin(bool preview_)
{
    BS_PROFILE_SCOPE("BossFinishDirector::Begin");
    if (active) return;
    Normalize();
    preview = preview_;
    active = true;
    paused = false; showFixed = false; captureRequested = false;
    inputArmed = false; returning = false; titleRequested = false;
    elapsed = 0.0f; returnElapsed = 0.0f; effectsTime = 0.0f;
    previewCameraOverride = false;
    previewPlaybackRate = 1.0f;
    impactReady = false; impactConfirmed = false; completionCommitted = false;
    finishAuthorized = false;
    if (!preview) previousBgmVolumes = AudioManager::Instance().CaptureBgmVolumes();
    if (auto mode_ = manager.GetGameModeManager())
    {
        // BOSS_DEFEATEDへ到達した時点で戦闘側の有効打は受付済み。
        // 演出内の簡易接触チェックを、もう一度撃破を要求する条件にはしない。
        finishAuthorized = !preview && mode_->GetCurrentGameState() == GameProgressState::BOSS_DEFEATED;
        previousPlayerLock = mode_->IsPlayerControlLocked(); previousAILock = mode_->IsEnemyAILocked();
        previousPresentation = mode_->IsPresentationPlaying(); previousGameplayLock = mode_->IsGameplayLocked();
        mode_->SetPlayerControlLocked(true); mode_->SetEnemyAILocked(true);
        mode_->SetPresentationPlaying(true); mode_->SetGameplayLocked(true);
    }
    // Scene全体を進めないため、暴走・弾・危険範囲・RootMotion・物理の予約も停止したまま保持できる。
    // プレビュー中断では元の攻防を壊さず、本番ではScene破棄が予約をまとめて解放する。
    for (const auto& object_ : manager.GetObjects())
    {
        if (!object_ || object_->IsPendingDestroy()) continue;
        SavedActor saved_;
        saved_.object = object_; saved_.transform = *object_->transform;
        for (const auto& collider_ : object_->colliderComponents)
        {
            saved_.colliderStates.emplace_back(collider_,collider_->IsRuntimeSuppressed());
            collider_->SetRuntimeSuppressed(true);
        }
        if (const auto model_ = object_->GetComponent<GltfModelCommponennt>())
        {
            model_->GetNodePoses(saved_.poses); saved_.visible = model_->IsRuntimeVisible();
        }
        savedActors.push_back(std::move(saved_));
    }
    camera = std::make_shared<CameraComponent>();
    PrepareActor(player, "Player"); PrepareActor(boss, "Boss");
    PrepareImpact();
    PrepareSprites(); CreateEffects(); Evaluate(0.0f, 0.0f);
}

void BossFinishDirector::Stop()
{
    BS_PROFILE_SCOPE("BossFinishDirector::Stop");
    if (!active) return;
    for (auto& saved_ : savedActors)
    {
        if (!saved_.object || saved_.object->IsPendingDestroy()) continue;
        *saved_.object->transform = saved_.transform;
        for (const auto& state_ : saved_.colliderStates)
            if (const auto collider_ = state_.first.lock()) collider_->SetRuntimeSuppressed(state_.second);
        if (const auto model_ = saved_.object->GetComponent<GltfModelCommponennt>())
        {
            model_->SetNodePoses(saved_.poses); model_->SetRuntimeVisible(saved_.visible); model_->UpdateTransform();
        }
    }
    savedActors.clear(); effects.clear();
    if (!preview) AudioManager::Instance().ApplyBgmVolumes(previousBgmVolumes,1.0f);
    previousBgmVolumes.clear();
    capturedView.Reset(); capturedTexture.Reset();
    imageSprite.reset(); solidSprite.reset(); clearSprite.reset(); promptSprite.reset(); camera.reset();
    player.object.reset(); boss.object.reset(); player.ready = false; boss.ready = false;
    bossReaction.object.reset(); bossReaction.ready = false;
    if (preview) if (auto mode_ = manager.GetGameModeManager())
    {
        mode_->SetPlayerControlLocked(previousPlayerLock); mode_->SetEnemyAILocked(previousAILock);
        mode_->SetPresentationPlaying(previousPresentation); mode_->SetGameplayLocked(previousGameplayLock);
    }
    active = false; showFixed = false; captureRequested = false;
    previewCameraOverride = false;
    impactReady = false; impactConfirmed = false; completionCommitted = false;
    finishAuthorized = false;
}

void BossFinishDirector::PrepareImpact()
{
    BS_PROFILE_SCOPE("BossFinishDirector::PrepareImpact");
    impactReady = false;
    const auto anchor_ = ReadPosition(settings["Anchor"]);
    const auto& playerConfig_ = settings["Player"];
    const auto& bossConfig_ = settings["Boss"];
    const auto reportContact_ = [&](const std::string& reason_)
    {
        status = "Finish contact diagnostic: " + reason_ + ". Presentation and clear display will continue.";
        BS_LOG_INFO(LOG_CATEGORY::GAME,status);
    };
    alignedEndPosition = ReadPosition(playerConfig_["EndPosition"]);
    impactPosition = AddPosition(anchor_, ReadPosition(bossConfig_["Position"]));
    if (!player.ready || !boss.ready) { reportContact_("player or boss animation unavailable"); return; }
    // 命中後の姿勢はプリセットで選ぶ。ダウン維持では同じループを最後まで使う。
    bossReaction.object = boss.object;
    bossReaction.state = boss.state;
    bossReaction.ready = false;
    const auto bossModel_ = boss.object->GetComponent<GltfModelCommponennt>();
    const auto& animations_ = bossModel_->GetAnimations();
    for (std::size_t index_ = 0; index_ < animations_.size(); ++index_)
        if (animations_[index_].name == bossConfig_.value("ReactionAnimation", std::string("Hit_Combat_Large_Death_Seq")))
        {
            bossReaction.state.animationIndex = static_cast<int>(index_);
            bossReaction.animator = ModelAnimator{};
            bossReaction.animator.CrossFadeTo(&bossReaction.state, 0.0f);
            bossReaction.ready = true;
            break;
        }
    if (!bossReaction.ready && !bossConfig_.value("KeepPoseAnimation",false))
    { reportContact_("boss reaction animation missing"); return; }
    PlaceActor(boss.object, impactPosition, bossConfig_.value("Yaw", 180.0f));
    // 接触位置の計算にも本番の命中時刻と同じダウン姿勢を使い、胴体の高さを一致させる。
    SampleActor(boss, bossConfig_.value("WindupPoseTime", 0.18f) +
        (bossConfig_.value("KeepPoseAnimation",false) ? GetImpactTime() : 0.0f));
    PlaceActor(player.object, AddPosition(anchor_, alignedEndPosition), playerConfig_.value("Yaw", 10.0f));
    SampleActor(player, playerConfig_.value("HitPoseTime", 0.35f));
    const auto weapon_ = manager.Find(playerConfig_.value("WeaponObject", std::string("Weapon")));
    const auto attachment_ = weapon_ ? weapon_->GetComponent<ModelAttachmentComponent>() : nullptr;
    if (!attachment_ || !attachment_->IsEnabled()) { reportContact_("weapon attachment missing or disabled"); return; }
    attachment_->SyncAttachment();
    DirectX::XMFLOAT3 target_, start_, end_;
    if (!ReadNodePosition(boss.object, bossConfig_.value("HitNode", std::string("spine_02")), target_) ||
        !ReadNodePosition(weapon_, playerConfig_.value("BladeStartNode", std::string("TrailStart")), start_) ||
        !ReadNodePosition(weapon_, playerConfig_.value("BladeEndNode", std::string("TrailEnd")), end_))
    { reportContact_("boss hit node or blade nodes unavailable"); return; }
    // 刃の高さに合う点を選び、接地高さを変えず水平移動だけで敵の胴体へ寄せる。
    const float bladeRatio_ = std::abs(end_.y - start_.y) > 0.001f
        ? std::clamp((target_.y - start_.y) / (end_.y - start_.y), 0.0f, 1.0f) : 0.5f;
    const DirectX::XMFLOAT3 bladePoint_{start_.x + (end_.x-start_.x)*bladeRatio_,
        start_.y + (end_.y-start_.y)*bladeRatio_, start_.z + (end_.z-start_.z)*bladeRatio_};
    alignedEndPosition.x += target_.x - bladePoint_.x;
    alignedEndPosition.z += target_.z - bladePoint_.z;
    PlaceActor(player.object, AddPosition(anchor_, alignedEndPosition), playerConfig_.value("Yaw", 10.0f));
    SampleActor(player, playerConfig_.value("HitPoseTime", 0.35f));
    attachment_->SyncAttachment();
    if (!ReadNodePosition(weapon_, playerConfig_.value("BladeStartNode", std::string("TrailStart")), start_) ||
        !ReadNodePosition(weapon_, playerConfig_.value("BladeEndNode", std::string("TrailEnd")), end_))
    { reportContact_("aligned blade nodes unavailable"); return; }
    using namespace DirectX;
    const auto startVector_ = XMLoadFloat3(&start_);
    const auto segment_ = XMLoadFloat3(&end_) - startVector_;
    const float lengthSquared_ = XMVectorGetX(XMVector3LengthSq(segment_));
    if (lengthSquared_ <= 0.000001f) { reportContact_("blade segment has zero length"); return; }
    const float ratio_ = std::clamp(XMVectorGetX(XMVector3Dot(XMLoadFloat3(&target_)-startVector_, segment_)) / lengthSquared_, 0.0f, 1.0f);
    const auto contact_ = startVector_ + segment_ * ratio_;
    const float radius_ = std::clamp(bossConfig_.value("HitRadius", 0.65f), 0.05f, 2.0f);
    impactReady = XMVectorGetX(XMVector3LengthSq(contact_ - XMLoadFloat3(&target_))) <= radius_ * radius_;
    XMStoreFloat3(&impactPosition, contact_);
    if (!impactReady)
    {
        reportContact_("blade does not reach the boss; adjust HitPoseTime / HitNode");
    }
}

DirectX::XMFLOAT3 BossFinishDirector::GetEffectPosition(const nlohmann::json& config_) const
{
    const auto anchorType_ = config_.value("Anchor", std::string());
    if (anchorType_ == "Impact") return AddPosition(impactPosition, ReadPosition(config_.at("Offset")));
    if (anchorType_ == "Boss" && boss.object)
    {
        // 胴体から落ちる粒子は死亡ポーズのノードへ追従し、地面の演出は足元に保つ。
        auto position_ = boss.object->transform->GetWorldPosition();
        const auto nodeName_ = config_.value("Node", std::string());
        if (!nodeName_.empty()) ReadNodePosition(boss.object, nodeName_, position_);
        return AddPosition(position_, ReadPosition(config_.at("Offset")));
    }
    if (anchorType_ == "Player" && player.object)
    {
        // プレイヤーに付ける演出も、敵と同じノード追従の規則で配置する。
        auto position_ = player.object->transform->GetWorldPosition();
        const auto nodeName_ = config_.value("Node", std::string());
        if (!nodeName_.empty()) ReadNodePosition(player.object, nodeName_, position_);
        return AddPosition(position_, ReadPosition(config_.at("Offset")));
    }
    // 旧プリセットの絶対配置はそのまま読み込み、新形式だけ基準位置とオフセットを使う。
    return AddPosition(ReadPosition(settings.at("Anchor")), ReadPosition(config_.at("Position")));
}

void BossFinishDirector::CreateEffects()
{
    BS_PROFILE_SCOPE("BossFinishDirector::CreateEffects");
    effects.clear();
    for (const auto& config_ : settings.at("Effects"))
    {
        auto object_ = MakeObject("FinishEffect");
        object_->SetManager(&manager);
        object_->transform->localPosition = GetEffectPosition(config_);
        object_->transform->localRotation = ReadPosition(config_["Rotation"]);
        object_->transform->localScale = ReadPosition(config_["Scale"]);
        object_->transform->MarkDirty(); object_->transform->UpdateWorldMatrix();
        try
        {
            const auto path_ = config_.at("Asset").get<std::string>();
            if (config_.value("Kind", "Mesh") == "Particle")
            {
                auto particle_ = object_->AddComponent<ParticleComponent>();
                if (!particle_->LoadAsset(path_)) throw std::runtime_error(path_);
                const auto rotation_ = ReadPosition(config_["Rotation"]);
                const auto scale_ = ReadPosition(config_["Scale"]);
                const auto tint_ = ReadColor(config_["Color"]);
                const float emission_ = config_.value("Emission",1.0f);
                const auto local_ = DirectX::XMMatrixScaling(scale_.x,scale_.y,scale_.z) *
                    DirectX::XMMatrixRotationRollPitchYaw(DirectX::XMConvertToRadians(rotation_.x),DirectX::XMConvertToRadians(rotation_.y),DirectX::XMConvertToRadians(rotation_.z));
                for (auto& track_ : particle_->GetTracks())
                {
                    // ParticleはObject回転・拡大を自動適用しないため、発生設定へ変換して反映する。
                    DirectX::XMStoreFloat4(&track_.settings.emissionPosition,DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat4(&track_.settings.emissionPosition),local_));
                    DirectX::XMStoreFloat4(&track_.settings.shapeStart,DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat4(&track_.settings.shapeStart),local_));
                    DirectX::XMStoreFloat4(&track_.settings.shapeEnd,DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat4(&track_.settings.shapeEnd),local_));
                    DirectX::XMStoreFloat4(&track_.settings.emissionAxis,DirectX::XMVector3Normalize(DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat4(&track_.settings.emissionAxis),local_)));
                    track_.spawnSpace = ParticleSpawnSpace::OWNER_RELATIVE;
                    track_.settings.emissionSize.x *= std::abs(scale_.x); track_.settings.emissionSize.y *= std::abs(scale_.x);
                    track_.settings.shapeSize.x *= std::abs(scale_.x); track_.settings.shapeSize.y *= std::abs(scale_.y); track_.settings.shapeSize.z *= std::abs(scale_.z);
                    const auto tintColor_ = [&](DirectX::XMFLOAT4& color_)
                    {
                        color_.x *= tint_.x*emission_; color_.y *= tint_.y*emission_; color_.z *= tint_.z*emission_; color_.w *= tint_.w;
                    };
                    tintColor_(track_.settings.manualColor); tintColor_(track_.settings.colorStart); tintColor_(track_.settings.colorEnd);
                }
                particle_->ResetRuntime(); particle_->Play(); particle_->SetRuntimeSuppressed(true);
            }
            else
            {
                auto mesh_ = object_->AddComponent<MeshVFXComponent>();
                if (!mesh_->LoadAsset(path_)) throw std::runtime_error(path_);
                for (auto& part_ : mesh_->GetAsset().parts)
                {
                    part_.renderEnabled = true;
                    part_.emissiveStrength = config_.value("Emission", 1.0f);
                }
                mesh_->SetRuntimeAppearance({1,1,1}, ReadColor(config_["Color"]), ReadColor(config_["Color"]));
                mesh_->Reset(); mesh_->Play(); mesh_->SetRuntimeSuppressed(true);
            }
        }
        catch (const std::exception& error_)
        {
            status = std::string("Finish VFX fallback: ") + error_.what();
            BS_LOG_INFO(LOG_CATEGORY::GAME, status);
        }
        effects.push_back(std::move(object_));
    }
}

void BossFinishDirector::AdvanceEffects(float from_, float to_)
{
    BS_PROFILE_SCOPE("BossFinishDirector::AdvanceEffects");
    const auto& configs_ = settings["Effects"];
    for (std::size_t index_ = 0; index_ < effects.size(); ++index_)
    {
        const auto& config_ = configs_[index_];
        const float origin_ = config_.value("TimeAnchor", std::string()) == "Impact" ? GetImpactTime() : 0.0f;
        const float start_ = origin_ + config_.value("Start", 0.0f);
        const float stop_ = origin_ + config_.value("Stop", GetCaptureTime() - origin_);
        const float limit_ = (std::max)(config_.value("PoseTime", 10.0f), 0.0f);
        const float previous_ = std::clamp(from_ - start_, 0.0f, limit_);
        const float current_ = std::clamp(to_ - start_, 0.0f, limit_);
        const auto& effect_ = effects[index_];
        effect_->transform->localPosition = GetEffectPosition(config_);
        effect_->transform->MarkDirty(); effect_->transform->UpdateWorldMatrix();
        const bool visible_ = to_ >= start_ && to_ <= stop_ && (!config_.value("RequiresImpact", false) || impactConfirmed);
        if (const auto particle_ = effect_->GetComponent<ParticleComponent>())
        {
            // GPUの直接シークは使わず、リセット後も本番も同じ固定刻みでシミュレーションする。
            particle_->SetRuntimeSuppressed(!visible_);
            const float growth_ = config_.value("RequiresImpact", false) ? 1.0f : std::clamp(current_ / 1.8f, 0.15f, 1.0f);
            particle_->SetEmissionScale(growth_);
            if (current_ > previous_) particle_->Update(current_ - previous_);
        }
        if (const auto mesh_ = effect_->GetComponent<MeshVFXComponent>())
        {
            mesh_->SetRuntimeSuppressed(!visible_);
            mesh_->Update((std::max)(current_ - previous_, 0.0f));
        }
    }
}

void BossFinishDirector::Evaluate(float time_, float deltaTime_)
{
    BS_PROFILE_SCOPE("BossFinishDirector::Evaluate");
    (void)deltaTime_;
    const float captureTime_ = GetCaptureTime();
    const float sampleTime_ = (std::min)(time_, captureTime_);
    const float impactTime_ = GetImpactTime();
    const float impactAge_ = (std::max)(sampleTime_ - impactTime_, 0.0f);
    const float impactHold_ = settings["ImpactHold"].get<float>();
    // 接触の成否は構図調整用。本番では既に成立した戦闘の撃破受付を優先し、
    // ダウン姿勢による胴体の高さや欠損アセットで討伐完了・タイトル復帰を停止させない。
    // Previewも本番と同じ命中キューとクリア表示を確認できるが、死亡確定は実行しない。
    impactConfirmed = (impactReady || finishAuthorized || preview) && sampleTime_ >= impactTime_;
    // 命中したポーズで短く止めてから振り抜き、接触前に敵をのけぞらせない。
    const float actionTime_ = sampleTime_ - (impactConfirmed ? (std::min)(impactAge_, impactHold_) : 0.0f);
    const float counterTime_ = (std::max)(actionTime_ - settings["CounterStart"].get<float>(), 0.0f);
    const auto& playerConfig_ = settings["Player"];
    const float windup_ = playerConfig_.value("WindupTime", 0.35f);
    const float windupSpeed_ = playerConfig_.value("WindupSpeed", 0.22f);
    const float strikeSpeed_ = playerConfig_.value("StrikeSpeed", 1.8f);
    const float poseTime_ = (std::min)(counterTime_, windup_) * windupSpeed_ + (std::max)(counterTime_ - windup_, 0.0f) * strikeSpeed_;
    const auto anchor_ = ReadPosition(settings["Anchor"]);
    if (player.object)
    {
        auto position_ = ReadPosition(playerConfig_["Position"]);
        const auto end_ = alignedEndPosition;
        const float travel_ = std::clamp((actionTime_ - settings["CounterStart"].get<float>() - windup_) /
            (std::max)(impactTime_ - settings["CounterStart"].get<float>() - windup_, 0.01f), 0.0f, 1.0f);
        position_ = {position_.x+(end_.x-position_.x)*travel_, position_.y+(end_.y-position_.y)*travel_, position_.z+(end_.z-position_.z)*travel_};
        PlaceActor(player.object, AddPosition(anchor_, position_), playerConfig_.value("Yaw", 0.0f));
        SampleActor(player, (std::min)(poseTime_, playerConfig_.value("PoseTime", 0.85f)));
    }
    if (boss.object)
    {
        const auto& bossConfig_ = settings["Boss"];
        PlaceActor(boss.object, AddPosition(anchor_, ReadPosition(bossConfig_["Position"])), bossConfig_.value("Yaw", 180.0f));
        const float reaction_ = impactConfirmed ? (std::max)(impactAge_ - impactHold_, 0.0f) * bossConfig_.value("ReactionSpeed", 1.6f) : 0.0f;
        if (bossConfig_.value("KeepPoseAnimation",false))
            // 主人公と同じ命中停止時間を除き、ダウンLoopの位相を前後で途切れさせない。
            SampleActor(boss,bossConfig_.value("WindupPoseTime",0.18f) + actionTime_);
        else if (impactConfirmed && impactAge_ > impactHold_)
            SampleActor(bossReaction, (std::min)(reaction_, bossConfig_.value("ReactionPoseTime", 1.8f)));
        else
            SampleActor(boss, bossConfig_.value("WindupPoseTime", 0.18f));
    }
    for (auto& saved_ : savedActors)
    {
        if (!ShouldRenderActor(saved_.object)) continue;
        if (const auto attachment_ = saved_.object->GetComponent<ModelAttachmentComponent>()) attachment_->SyncAttachment();
        saved_.object->transform->UpdateWorldMatrix();
        if (const auto model_ = saved_.object->GetComponent<GltfModelCommponennt>()) model_->UpdateTransform();
    }
    // フレームレートに依存しない固定刻みで、再撮影時も同じシミュレーション時刻へ到達する。
    while (effectsTime + FIXED_STEP <= sampleTime_)
    {
        AdvanceEffects(effectsTime, effectsTime + FIXED_STEP);
        effectsTime += FIXED_STEP;
    }
    if (preview && previewCameraOverride) return;
    auto config_ = settings["Camera"];
    // 演出時計上のキーを既存Curve補間で評価し、保存された固定構図へ上書きする。
    if (config_.contains("Keys") && config_["Keys"].is_array() && !config_["Keys"].empty())
    {
        const auto& keys_ = config_["Keys"];
        const nlohmann::json* first_ = &keys_.front();
        const nlohmann::json* second_ = first_;
        const float cameraTime_ = (std::min)(elapsed, GetCaptureTime() + settings["HitStop"].get<float>());
        for (const auto& key_ : keys_)
        {
            second_ = &key_;
            if (key_.at("Time").get<float>() >= cameraTime_) break;
            first_ = second_;
        }
        const float span_ = second_->at("Time").get<float>() - first_->at("Time").get<float>();
        float rate_ = span_ > 0.0001f ? std::clamp((cameraTime_ - first_->at("Time").get<float>()) / span_, 0.0f, 1.0f) : 0.0f;
        const auto easing_ = first_->value("Easing", std::string("Smooth"));
        static const CurveTrack CAMERA_CURVE;
        const auto interpolation_ = easing_ == "Linear" ? CURVE_INTERPOLATION_TYPE::LINEAR :
            easing_ == "EaseIn" ? CURVE_INTERPOLATION_TYPE::EASE_IN : easing_ == "EaseOut" ? CURVE_INTERPOLATION_TYPE::EASE_OUT : CURVE_INTERPOLATION_TYPE::SMOOTH;
        rate_ = CAMERA_CURVE.ApplyInterpolation(rate_, interpolation_);
        for (const char* property_ : { "Position", "Focus" })
            for (std::size_t index_ = 0; index_ < 3; ++index_)
                config_[property_][index_] = (*first_)[property_][index_].get<float>() +
                    ((*second_)[property_][index_].get<float>() - (*first_)[property_][index_].get<float>()) * rate_;
        for (const char* property_ : { "Fov", "Roll" })
            config_[property_] = first_->at(property_).get<float>() + (second_->at(property_).get<float>() - first_->at(property_).get<float>()) * rate_;
    }
    auto eye_ = AddPosition(anchor_, ReadPosition(config_["Position"]));
    auto focus_ = AddPosition(anchor_, ReadPosition(config_["Focus"]));
    const float chargeShake_ = sampleTime_ > settings["BlackoutEnd"].get<float>() && sampleTime_ < settings["CounterStart"].get<float>()
        ? config_.value("Shake", 0.12f) * std::clamp(sampleTime_ / 2.5f, 0.0f, 1.0f) : 0.0f;
    // 命中を起点に位置とロールを強く揺らし、減衰させて決め絵までに静める。
    const float decay_ = impactConfirmed ? std::exp(-impactAge_ / (std::max)(config_.value("ImpactShakeDuration", 0.22f), 0.01f)) : 0.0f;
    const float shake_ = chargeShake_ + config_.value("ImpactShake", 0.38f) * decay_;
    const float phase_ = sampleTime_ * 71.0f;
    const DirectX::XMFLOAT3 offset_{std::sin(phase_) * shake_, std::cos(sampleTime_ * 93.0f) * shake_ * 0.7f,
        std::sin(sampleTime_ * 57.0f) * shake_ * 0.4f};
    eye_ = AddPosition(eye_, offset_); focus_ = AddPosition(focus_, offset_);
    using namespace DirectX;
    const float roll_ = XMConvertToRadians(config_.value("Roll", 0.0f) + std::sin(phase_) * config_.value("ImpactRoll", 3.0f) * decay_);
    const auto forward_ = XMVector3Normalize(XMLoadFloat3(&focus_) - XMLoadFloat3(&eye_));
    const auto upVector_ = XMVector3Rotate(XMVectorSet(0,1,0,0), XMQuaternionRotationAxis(forward_, roll_));
    XMFLOAT3 up_; XMStoreFloat3(&up_, upVector_);
    camera->SetLookAt(eye_, focus_, up_);
    auto& graphics_ = GraphicsDeviceManager::Instance();
    const float aspect_ = static_cast<float>((std::max)(graphics_.GetRenderWidth(), 1)) / (std::max)(graphics_.GetRenderHeight(), 1);
    float fov_ = (std::max)(15.0f, config_.value("Fov", 43.0f) - config_.value("ImpactFovKick", 5.0f) * decay_);
    if (aspect_ < GetAspect()) fov_ = XMConvertToDegrees(2.0f * std::atan(std::tan(XMConvertToRadians(fov_) * 0.5f) * GetAspect() / aspect_));
    camera->SetPerspectiveFov(fov_, aspect_, 0.05f, 300.0f);
}

bool BossFinishDirector::Update(float rawDeltaTime_)
{
    BS_PROFILE_SCOPE("BossFinishDirector::Update");
    const auto mode_ = manager.GetGameModeManager();
    if (!active && mode_ && mode_->GetCurrentGameState() == GameProgressState::BOSS_DEFEATED) Begin(false);
    if (!active) return false;
    const float delta_ = std::clamp(rawDeltaTime_, 0.0f, 0.1f);
    const float previous_ = elapsed;
    if (!paused) elapsed += delta_ * (preview ? previewPlaybackRate : 1.0f);
    if (!preview) AudioManager::Instance().ApplyBgmVolumes(previousBgmVolumes,1.0f-std::clamp(elapsed/0.7f,0.0f,1.0f));
    // 最初の短い停止は演出時計だけを進め、通常TimeScaleを変更せずに表現する。
    Evaluate((std::max)(elapsed - settings["HitStop"].get<float>(), 0.0f), delta_);
    const float captureAt_ = GetCaptureTime() + settings["HitStop"].get<float>();
    if (elapsed >= captureAt_ && previous_ < captureAt_) RequestCapture();
    const float hold_ = elapsed - captureAt_;
    if (!preview && completionCommitted && hold_ >= settings["PromptDelay"].get<float>())
    {
        const auto action_ = settings.value("ConfirmAction", std::string("UI.MenuConfirm"));
        const bool pressed_ = KeyConfig::Instance().ActionPressed(action_);
        if (!pressed_) inputArmed = true;
        if (inputArmed && KeyConfig::Instance().ActionPressedIn(action_) && !returning) returning = true;
    }
    if (returning)
    {
        returnElapsed += delta_;
        if (returnElapsed >= settings["ReturnFade"].get<float>() && !titleRequested)
        {
            titleRequested = true;
            manager.GetTimeManager()->ResetToDefaults(static_cast<uint32_t>(TarGetLayerType::All), true);
            AudioManager::Instance().StopBGM();
            // 既存のLoading Scene経路を使い、旧Scene破棄で演出のGPU資源も解放する。
            ActionRegistry::Instance().Call("GoSceneTitle");
        }
    }
    return true;
}

void BossFinishDirector::Seek(float time_)
{
    BS_PROFILE_SCOPE("BossFinishDirector::Seek");
    if (!active || !preview) return;
    Normalize();
    PrepareActor(player,"Player"); PrepareActor(boss,"Boss"); PrepareImpact(); PrepareSprites();
    effectsTime = 0.0f;
    CreateEffects();
    capturedView.Reset(); capturedTexture.Reset();
    elapsed = std::clamp(time_, 0.0f, GetCaptureTime() + settings["PromptDelay"].get<float>() + 5.0f);
    Evaluate((std::max)(elapsed - settings["HitStop"].get<float>(), 0.0f), 0.0f);
    paused = true; showFixed = false;
    if (elapsed >= GetCaptureTime() + settings["HitStop"].get<float>()) RequestCapture();
}

void BossFinishDirector::RequestCapture()
{
    if (!active) return;
    captureRequested = true; showFixed = false;
}

bool BossFinishDirector::ShouldRenderActor(const std::shared_ptr<GameObject>& object_) const
{
    if (!object_) return false;
    if (object_ == player.object || object_ == boss.object) return true;
    for (const auto& name_ : settings.at("Equipment")) if (object_->GetName() == name_.get<std::string>()) return true;
    return false;
}

void BossFinishDirector::PrepareSprites()
{
    const auto makeSprite_ = [](const char* name_)
    {
        auto object_ = MakeObject(name_);
        const auto sprite_ = object_->AddComponent<SpriteRenderer>();
        sprite_->Deserialize({{"SdrTexture", true}, {"LinearSampling", true}});
        sprite_->SetPivot(PosOfsset::TOP_L);
        return object_;
    };
    imageSprite = makeSprite_("FinishImage"); solidSprite = makeSprite_("FinishFade");
    clearSprite = makeSprite_("FinishClear"); promptSprite = makeSprite_("FinishPrompt");
    const auto setText_ = [](const std::shared_ptr<GameObject>& object_, const std::string& text_, float size_)
    {
        const auto textComponent_ = object_->AddComponent<UITextComponent>();
        textComponent_->Deserialize({{"Text", text_}, {"FontSize", size_}, {"AutoSizeWidget", false}, {"Color", {0.9f,1.0f,0.94f,1.0f}}});
        textComponent_->Update(0.0f);
    };
    setText_(clearSprite, settings.value("ClearText", std::string()), 64.0f);
    setText_(promptSprite, settings.value("PromptText", std::string()), 24.0f);
}

bool BossFinishDirector::Capture(ID3D11RenderTargetView* target_)
{
    BS_PROFILE_SCOPE("BossFinishDirector::Capture");
    auto& graphics_ = GraphicsDeviceManager::Instance();
    Microsoft::WRL::ComPtr<ID3D11Resource> resource_;
    target_->GetResource(resource_.GetAddressOf());
    Microsoft::WRL::ComPtr<ID3D11Texture2D> source_;
    if (FAILED(resource_.As(&source_))) return false;
    D3D11_TEXTURE2D_DESC description_{}; source_->GetDesc(&description_);
    description_.Usage = D3D11_USAGE_DEFAULT; description_.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    description_.CPUAccessFlags = 0; description_.MiscFlags = 0;
    capturedView.Reset(); capturedTexture.Reset();
    if (FAILED(graphics_.GetDevice()->CreateTexture2D(&description_, nullptr, capturedTexture.GetAddressOf())) ||
        FAILED(graphics_.GetDevice()->CreateShaderResourceView(capturedTexture.Get(), nullptr, capturedView.GetAddressOf())))
    {
        BS_LOG_INFO(LOG_CATEGORY::GAME, "Finish capture unavailable; clear and title flow continues.");
        return false;
    }
    graphics_.GetContext()->OMSetRenderTargets(0, nullptr, nullptr);
    graphics_.GetContext()->CopyResource(capturedTexture.Get(), source_.Get());
    const float width_ = static_cast<float>(description_.Width), height_ = static_cast<float>(description_.Height);
    const float fitWidth_ = (std::min)(width_, height_ * GetAspect());
    const float fitHeight_ = fitWidth_ / GetAspect();
    captureUv = {(width_-fitWidth_)/(2*width_), (height_-fitHeight_)/(2*height_), (width_+fitWidth_)/(2*width_), (height_+fitHeight_)/(2*height_)};
    imageSprite->GetComponent<SpriteRenderer>()->SetTextureResource(capturedView, description_);
    return true;
}

void BossFinishDirector::DrawSprite(SpriteRenderSystem& sprites_, const std::shared_ptr<GameObject>& object_,
    float x_, float y_, float width_, float height_, const DirectX::XMFLOAT4& color_)
{
    object_->transform->localPosition = {x_, y_, 0};
    object_->transform->MarkDirty(); object_->transform->UpdateWorldMatrix();
    const auto sprite_ = object_->GetComponent<SpriteRenderer>();
    sprite_->SetLocalSize({width_, height_}); sprite_->SetColor(color_);
    sprites_.Submit(sprite_);
    sprites_.RenderAll(GraphicsDeviceManager::Instance().GetContext());
}

void BossFinishDirector::RenderOutput(SpriteRenderSystem& sprites_, ID3D11RenderTargetView* target_)
{
    BS_PROFILE_SCOPE("BossFinishDirector::RenderOutput");
    if (!active) return;
    auto& graphics_ = GraphicsDeviceManager::Instance();
    auto* context_ = graphics_.GetContext();
    if (captureRequested)
    {
        showFixed = Capture(target_); captureRequested = false;
    }
    // 撮影要求の有無・成功とは独立して、本番の既定時刻に一度だけ完了させる。
    // 接触診断NGや再撮影で要求を消費しても、クリア文字と決定入力まで必ず進める。
    if (!preview && finishAuthorized && !completionCommitted &&
        elapsed >= GetCaptureTime() + settings["HitStop"].get<float>())
    {
        completionCommitted = true;
        if (boss.object)
            if (const auto health_ = boss.object->GetComponent<HealthComponent>()) health_->ConfirmPresentationDeath();
        AudioManager::Instance().PlaySE(SE_GAMECLEAR);
        if (auto mode_ = manager.GetGameModeManager()) mode_->NotifySequenceFinished();
        BS_LOG_INFO(LOG_CATEGORY::GAME,"Boss finish committed: clear text and title confirmation enabled");
    }
    context_->OMSetRenderTargets(1, &target_, nullptr);
    const auto* states_ = graphics_.GetRenderState();
    context_->OMSetBlendState(states_->GetBlendState(BlendState::Transparency), nullptr, 0xffffffff);
    context_->OMSetDepthStencilState(states_->GetDepthStencilState(DepthState::NoTestNoWrite), 0);
    context_->RSSetState(states_->GetRasterizerState(RasterizerState::SolidCullNone));
    D3D11_VIEWPORT viewport_{}; UINT count_ = 1; context_->RSGetViewports(&count_, &viewport_);
    const float width_ = viewport_.Width, height_ = viewport_.Height;
    const float fitWidth_ = (std::min)(width_, height_ * GetAspect()), fitHeight_ = fitWidth_ / GetAspect();
    const float left_ = (width_-fitWidth_)*0.5f, top_ = (height_-fitHeight_)*0.5f;
    const float held_ = elapsed - GetCaptureTime() - settings["HitStop"].get<float>();
    if (showFixed && capturedView)
    {
        const float black_[4] = {0,0,0,1}; context_->ClearRenderTargetView(target_, black_);
        const float ratio_ = std::clamp(held_ / (std::max)(settings["PromptDelay"].get<float>(), 0.01f), 0.0f, 1.0f);
        const float zoom_ = settings["ZoomStart"].get<float>() + (settings["ZoomEnd"].get<float>()-settings["ZoomStart"].get<float>()) * ratio_;
        const auto sprite_ = imageSprite->GetComponent<SpriteRenderer>();
        const auto dimensions_ = sprite_->GetTextureSize();
        const float uvWidth_ = (captureUv.z-captureUv.x)/zoom_, uvHeight_ = (captureUv.w-captureUv.y)/zoom_;
        sprite_->SetUVRect(((captureUv.x+captureUv.z-uvWidth_)*0.5f)*dimensions_.x, ((captureUv.y+captureUv.w-uvHeight_)*0.5f)*dimensions_.y, uvWidth_*dimensions_.x, uvHeight_*dimensions_.y);
        // 撮影済みSDRをSpriteで直接合成するため、BloomとToneMappingを再適用しない。
        DrawSprite(sprites_, imageSprite, left_, top_, fitWidth_, fitHeight_, ReadColor(settings["ImageTint"]));
    }
    // 本番とEditorとも同じアスペクト領域だけを見せ、画面サイズ変更でも伸長しない。
    if (top_ > 0) { DrawSprite(sprites_, solidSprite, 0,0,width_,top_, {0,0,0,1}); DrawSprite(sprites_, solidSprite,0,top_+fitHeight_,width_,top_,{0,0,0,1}); }
    if (left_ > 0) { DrawSprite(sprites_, solidSprite,0,0,left_,height_,{0,0,0,1}); DrawSprite(sprites_, solidSprite,left_+fitWidth_,0,left_,height_,{0,0,0,1}); }
    if (impactConfirmed && (preview || completionCommitted) && held_ >= settings["HoldTime"].get<float>())
    {
        const auto drawText_ = [&](const std::shared_ptr<GameObject>& object_, const char* position_, float alpha_)
        {
            const auto size_ = object_->GetComponent<SpriteRenderer>()->GetTextureSize();
            const float scale_ = fitWidth_ / 1600.0f;
            DrawSprite(sprites_, object_, left_+fitWidth_*settings[position_][0].get<float>()-size_.x*scale_*0.5f,
                top_+fitHeight_*settings[position_][1].get<float>()-size_.y*scale_*0.5f, size_.x*scale_, size_.y*scale_, {1,1,1,alpha_});
        };
        drawText_(clearSprite,"ClearPosition",std::clamp((held_-settings["HoldTime"].get<float>())/0.5f,0.0f,1.0f));
        if (held_ >= settings["PromptDelay"].get<float>()) drawText_(promptSprite,"PromptPosition",1.0f);
    }
    // 白い閃光も実際の命中時刻に合わせ、攻撃前やプレビュー停止中に再発火させない。
    const float impactAge_ = elapsed - settings["HitStop"].get<float>() - GetImpactTime();
    if (impactConfirmed && impactAge_ >= 0.0f && impactAge_ < 0.12f)
        DrawSprite(sprites_,solidSprite,0,0,width_,height_,{1,1,1,0.65f * std::exp(-impactAge_ / 0.035f)});
    const float hitStop_ = settings["HitStop"].get<float>();
    const float blackout_ = settings["BlackoutEnd"].get<float>();
    const float fade_ = elapsed <= hitStop_ ? 1.0f : 1.0f-std::clamp((elapsed-hitStop_)/(std::max)(blackout_,0.01f),0.0f,1.0f);
    if (fade_ > 0) DrawSprite(sprites_,solidSprite,0,0,width_,height_,{0,0,0,fade_});
    if (returning) DrawSprite(sprites_,solidSprite,0,0,width_,height_,{0,0,0,std::clamp(returnElapsed/settings["ReturnFade"].get<float>(),0.0f,1.0f)});
}

void BossFinishDirector::CaptureEditorCamera()
{
    const auto editorCamera_ = manager.GetCameraManager()->GetEditorCamera();
    if (!editorCamera_) return;
    const auto anchor_ = ReadPosition(settings["Anchor"]);
    const auto eye_ = editorCamera_->GetEye(), focus_ = editorCamera_->GetFocus();
    settings["Camera"]["Position"] = {eye_.x-anchor_.x,eye_.y-anchor_.y,eye_.z-anchor_.z};
    settings["Camera"]["Focus"] = {focus_.x-anchor_.x,focus_.y-anchor_.y,focus_.z-anchor_.z};
    settings["Camera"]["Fov"] = editorCamera_->GetFovY();
    using namespace DirectX;
    const auto forward_ = XMVector3Normalize(XMLoadFloat3(&focus_)-XMLoadFloat3(&eye_));
    const auto right_ = XMVector3Normalize(XMVector3Cross(XMVectorSet(0,1,0,0),forward_));
    const auto baseUp_ = XMVector3Cross(forward_,right_);
    const auto up_ = XMLoadFloat3(&editorCamera_->GetUp());
    settings["Camera"]["Roll"] = XMConvertToDegrees(std::atan2(-XMVectorGetX(XMVector3Dot(up_,right_)),XMVectorGetX(XMVector3Dot(up_,baseUp_))));
}

#include "PrecompiledHeader.h"
#include "BossFinishDirector.h"
#include "GameModeManager.h"
#include "PlayerDeathDirector.h"
#include "Engine/GamePlay/Graphics/ModelDissolveComponent.h"
#include "Engine/GamePlay/Graphics/ModelMaterialComponent.h"
#include "Engine/Common/DebugLog.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Camera/CameraManager.h"
#include "Engine/GamePlay/Camera/CameraComponent.h"
#include "Engine/GamePlay/Transform/ModelAttachmentComponent.h"
#include "Engine/GamePlay/Input/KeyConfig.h"
#include "Engine/GamePlay/System/TimeManager.h"
#include "Engine/GamePlay/Combat/HealthComponent.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Graphics/Core/CPUResourceLoader.h"
#include "Engine/Graphics/RenderSystem/SpriteRenderSystem.h"
#include "Engine/Graphics/Particle/ParticleComponent.h"
#include "Engine/Graphics/VFX/MeshVFXComponent.h"
#include "Engine/Graphics/VFX/MaterialOverlayComponent.h"
#include "Engine/UI/UITextComponent.h"
#include "Engine/UI/UIVisualEffectComponent.h"
#include "Engine/UI/GlassTransition.h"
#include "Engine/Audio/AudioManager.h"
#include "Engine/Math/Curve/CurveTrack.h"
#include "Engine/Platform/WindowsDialogHelper.h"
#include <fstream>
#include <format>
#include <cmath>
#include <algorithm>
#include <array>
#include <imgui.h>
#include <filesystem>
#include <string_view>

namespace
{
    constexpr const char* PRESET_PATH = "Data/Presentation/AnathemaBossFinish.json";
    constexpr float FIXED_STEP = 1.0f / 60.0f;
    constexpr float RESULT_RANK_DELAY = 0.44f;
    constexpr float RESULT_RANK_REVEAL = 0.52f;
    constexpr char RESULT_GRADES[] = {'S', 'A', 'B', 'C'};
    constexpr DirectX::XMFLOAT4 RESULT_CLEAR_TEXT_COLOR{0.9f, 1.0f, 0.94f, 1.0f};
    constexpr DirectX::XMFLOAT4 RESULT_FAILURE_TEXT_COLOR{1.0f, 0.72f, 0.74f, 1.0f};
    constexpr const char* RESULT_RANK_BURST_PATH = "Data/Texture/Particle/kenney_particle-pack/PNG (Transparent)/star_09.png";
    constexpr float TRUE_END_HAND_LIGHT = 0.85f;
    constexpr float TRUE_END_AWAKEN = 1.65f;
    constexpr float TRUE_END_LUNGE = 3.05f;
    constexpr float TRUE_END_SWEEP = 3.25f;
    constexpr float TRUE_END_SWEEP_HIT = TRUE_END_SWEEP + 0.58f;
    constexpr float TRUE_END_CHARGE = 4.5f;
    constexpr float TRUE_END_BEAM = 6.3f;
    constexpr float TRUE_END_PILLAR = TRUE_END_BEAM + 0.18f;
    constexpr float TRUE_END_FINISH = 9.45f;
    constexpr float TRUE_END_PORTRAIT = TRUE_END_BEAM + 1.75f;
    constexpr float TRUE_END_RAISED_POSE = 0.52f;
    constexpr float TRUE_END_RELEASE_POSE = 0.78f;
    constexpr float TRUE_END_IMPACT_DEPTH = -5.4f;
    constexpr DirectX::XMFLOAT4 TRUE_END_GOLD{1.0f, 0.55f, 0.10f, 1.0f};
    constexpr DirectX::XMFLOAT4 TRUE_END_CORE{1.0f, 0.91f, 0.61f, 1.0f};
    constexpr float BAD_END_COUNTER = 0.95f;
    constexpr float BAD_END_FLASHBACK = 1.25f;
    constexpr float BAD_END_FLASHBACK_DURATION = 1.0f;
    constexpr float BAD_END_KNEEL = BAD_END_FLASHBACK + BAD_END_FLASHBACK_DURATION;
    constexpr float BAD_END_PIERCE = BAD_END_KNEEL + 1.28f;
    constexpr float BAD_END_THRUST_START = BAD_END_PIERCE - 0.18f;
    constexpr float BAD_END_PIERCE_CUE = 0.37f;
    constexpr float BAD_END_IMPACT_HOLD = 0.16f;
    constexpr float BAD_END_FALL = BAD_END_PIERCE + 2.29f;
    constexpr float BAD_END_RISE = BAD_END_FALL + 1.74f;
    constexpr float BAD_END_WIPE = BAD_END_RISE + 3.0f;
    constexpr float BAD_END_COVERED = BAD_END_WIPE + 1.32f;
    constexpr float BAD_END_FINISH = BAD_END_COVERED + 0.55f;
    constexpr DirectX::XMFLOAT4 BAD_END_RED{1.0f, 0.0f, 0.0f, 1.0f};
    constexpr DirectX::XMFLOAT4 BAD_END_MUD{0.1f, 0.025f, 0.032f, 1.0f};
    constexpr const char* BAD_END_EQUIPMENT_TEXTURE = "Data/Texture/VFX/T_Free_Magic_Noise2.PNG";
    constexpr const char* BAD_END_HAIR_TEXTURE = "Data/Model/Material/BadEndHiar.png";

    float ShotProgress(float time_, float start_, float end_)
    {
        const float rate_ = std::clamp((time_ - start_) / (end_ - start_), 0.0f, 1.0f);
        return rate_ * rate_ * (3.0f - 2.0f * rate_);
    }

    float ImpactPulse(float time_, float cue_, float duration_)
    {
        return time_ < cue_ ? 0.0f : 1.0f - ShotProgress(time_, cue_, cue_ + duration_);
    }

    void SetResultText(const std::shared_ptr<GameObject>& object_, const std::string& text_, float size_,
        const char* align_ = "LEFT", const DirectX::XMFLOAT4& color_ = RESULT_CLEAR_TEXT_COLOR)
    {
        auto textComponent_ = object_->GetComponent<UITextComponent>();
        if (textComponent_)
        {
            textComponent_->SetText(text_);
        }
        else
        {
            textComponent_ = object_->AddComponent<UITextComponent>();
            textComponent_->Deserialize({{"Text", text_}, {"FontSize", size_}, {"AutoSizeWidget", false},
                {"HorizontalAlign", align_}, {"Color", {color_.x, color_.y, color_.z, color_.w}}});
        }
        textComponent_->Update(0.0f);
    }

    void ValidateResultRanks(const nlohmann::json& ranks_)
    {
    // ランク閾値は S、A、B の順。旧プリセットでは既定値がマージされる。
    for (const char* key_ : {"TimeSeconds", "JustActions", "DamageTaken"})
    {
        const auto& thresholds_ = ranks_.at(key_);
        if (!thresholds_.is_array() || thresholds_.size() != 3 ||
            !std::all_of(thresholds_.begin(), thresholds_.end(), [](const auto& threshold_)
            {
                return threshold_.is_number() && threshold_ >= 0;
            }))
        {
            throw std::runtime_error("invalid result rank thresholds");
        }
        const double s_ = thresholds_[0].get<double>();
        const double a_ = thresholds_[1].get<double>();
        const double b_ = thresholds_[2].get<double>();
        const bool descending_ = std::string_view(key_) == "JustActions";
        if (!std::isfinite(s_) || !std::isfinite(a_) || !std::isfinite(b_) ||
            (descending_ ? !(s_ > a_ && a_ > b_) : !(s_ < a_ && a_ < b_)))
        {
            throw std::runtime_error("result rank thresholds are out of order");
        }
    }
    }

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
        if (!model_ || !model_->IsModelReady())
        {
            return false;
        }
        for (const auto& node_ : model_->GetNodes())
        {
            if (node_.name == name_)
            {
                position_ = {node_.worldTransform._41, node_.worldTransform._42, node_.worldTransform._43};
                return std::isfinite(position_.x) && std::isfinite(position_.y) && std::isfinite(position_.z);
            }
        }
        return false;
    }

    // 演出中は通常移動を止め、ポーズ評価の前に配置を確定する。
    void PlaceActor(const std::shared_ptr<GameObject>& object_, const DirectX::XMFLOAT3& position_, float yaw_)
    {
        if (!object_)
        {
            return;
        }
        object_->transform->localPosition = position_;
        object_->transform->localRotation = {0, yaw_, 0};
        object_->transform->moveOffset = {};
        object_->transform->rotateOffset = {};
        object_->transform->MarkDirty();
        object_->transform->UpdateWorldMatrix();
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
  "Version": 2,
  "Anchor": [
    0.0,
    -0.12,
    0.0
  ],
  "Aspect": 1.7777778,
  "HitStop": 0.0,
  "ImpactHold": 0.0,
  "BlackoutEnd": 0.0,
  "CounterStart": 0.0,
  "CaptureTime": 6.8,
  "HoldTime": 1.3,
  "PromptDelay": 2.8,
  "ReturnFade": 0.65,
  "Camera": {
    "Position": [
      5,
      1.55,
      -6
    ],
    "Focus": [
      0,
      1.15,
      0.1
    ],
    "Fov": 43.0,
    "Roll": 0,
    "Shake": 0,
    "ImpactShake": 0.12,
    "ImpactShakeDuration": 0.18,
    "ImpactRoll": 0.8,
    "ImpactFovKick": 1.5
  },
  "Player": {
    "Object": "PlayerModel",
    "StateAsset": "Data/Animation/StateData/Player/States/Run_Attack_01_Seq.json",
    "Position": [
      0,
      0,
      -3.6
    ],
    "EndPosition": [
      0,
      0,
      -0.9
    ],
    "Yaw": 0,
    "PoseTime": 2.0666668,
    "WindupTime": 0.35,
    "WindupSpeed": 0.22,
    "StrikeSpeed": 1.8,
    "HitPoseTime": 0.35,
    "WeaponObject": "Weapon",
    "BladeStartNode": "TrailStart",
    "BladeEndNode": "TrailEnd",
    "Animation": "Run_Attack_01_Seq_0",
    "ApproachTime": 0.55
  },
  "Boss": {
    "Object": "HumanBoss",
    "StateAsset": "Data/Animation/StateData/HumanBoss/States/HumanBossBerserkDownLoop.json",
    "Position": [
      0,
      0,
      0.9
    ],
    "Yaw": 180,
    "PoseTime": 0.8,
    "WindupPoseTime": 0.18,
    "HitNode": "spine_02",
    "HitRadius": 0.65,
    "ReactionAnimation": "Knock_Down_Combat_Loop_Seq",
    "ReactionSpeed": 1.0,
    "ReactionPoseTime": 1.8,
    "KeepPoseAnimation": true,
    "Animation": "Knock_Down_Loop_Seq",
    "Loop": true,
    "ExecutionDistance": 1.8
  },
  "Equipment": [
    "Weapon",
    "Shield",
    "HumanBossWeapon",
    "HumanBossShield"
  ],
  "Background": [
    0.009,
    0.003,
    0.018,
    1.0
  ],
  "ImageTint": [
    1,
    1,
    1,
    1
  ],
  "ZoomStart": 1.0,
  "ZoomEnd": 1.0,
  "ClearText": "討伐完了",
  "BadEndText": "討伐失敗",
  "ResultRanks": {
    "TimeSeconds": [300, 450, 600],
    "JustActions": [20, 10, 5],
    "DamageTaken": [500, 750, 1000]
  },
  "PromptText": "ボタンを押してタイトルへ",
  "ClearPosition": [
    0.5,
    0.76
  ],
  "PromptPosition": [
    0.5,
    0.9
  ],
  "ConfirmAction": "UI.MenuConfirm",
  "Effects": [
    {
      "Kind": "Particle",
      "Asset": "Data/Particle/AnathemaFinishMudSlough.json",
      "Position": [
        0,
        0,
        0
      ],
      "Rotation": [
        0,
        0,
        0
      ],
      "Scale": [
        1,
        1,
        1
      ],
      "Start": 0,
      "Stop": 3.15,
      "PoseTime": 3.2,
      "Color": [
        1,
        1,
        1,
        1
      ],
      "Emission": 1.0,
      "Anchor": "Boss",
      "Offset": [
        0,
        0,
        0
      ],
      "MeshSourceObject": true,
      "EmitDuration": 2.1,
      "TimeAnchor": "Dissolve"
    }
  ],
  "StageCenterObject": "Stage",
  "ExecutionStart": 2.3,
  "AlignDuration": 0.23,
  "SlamTime": 0.8333333333333334,
  "DissolveDelay": 0.4,
  "DissolveDuration": 2.1,
  "PlayerExecution": {
    "Object": "PlayerModel",
    "StateAsset": "Data/Animation/StateData/HumanBoss/States/Execution_03_Seq.json",
    "Animation": "Execution_03_Seq"
  },
  "BossExecution": {
    "Object": "HumanBoss",
    "StateAsset": "Data/Animation/StateData/Player/States/Execution_Target_03_Seq.json",
    "Animation": "Execution_Target_03_Seq"
  }
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

float BossFinishDirector::GetCaptureTime() const
{
    return settings.value("CaptureTime", 3.7f);
}

float BossFinishDirector::GetAspect() const
{
    return settings.value("Aspect", 16.0f / 9.0f);
}

float BossFinishDirector::GetImpactCueTime() const
{
    return settings.value("HitStop", 0.16f) + GetImpactTime();
}

void BossFinishDirector::SetPreviewPlaybackRate(float rate_)
{
    previewPlaybackRate = std::isfinite(rate_) ? std::clamp(rate_, 0.1f, 2.0f) : 1.0f;
}

BossFinishDirector::PreviewActorInfo BossFinishDirector::GetPreviewActorInfo(bool boss_) const
{
    const auto& actor_ = elapsed >= settings.at("ExecutionStart").get<float>()
        ? (boss_ ? bossReaction : playerExecution) : (boss_ ? boss : player);
    PreviewActorInfo info_;
    info_.ready = actor_.ready;
    if (!actor_.ready || !actor_.object)
    {
        return info_;
    }
    const auto model_ = actor_.object->GetComponent<GltfModelCommponennt>();
    if (!model_ || actor_.state.animationIndex < 0 || actor_.state.animationIndex >= static_cast<int>(model_->GetAnimations().size()))
    {
        return info_;
    }
    info_.clipName = actor_.state.type == AnimStateType::SEQUENCE ? actor_.state.name + " (Sequence)" : model_->GetAnimations()[actor_.state.animationIndex].name;
    info_.duration = actor_.animator.GetCurrentSampleDuration();
    info_.sampleTime = actor_.animator.GetCurrentFadeTime();
    return info_;
}

DirectX::XMFLOAT4 BossFinishDirector::GetBackgroundColor() const
{
    return IsBadEndingImpactShot() ? DirectX::XMFLOAT4{0,0,0,1} : ReadColor(settings.at("Background"));
}

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
    // 旧プリセットでも暗転・単発カウンターへ戻らず、新しい連続演出の時計へ統一する。
    settings["HitStop"] = 0.0f;
    settings["BlackoutEnd"] = 0.0f;
    settings["CounterStart"] = 0.0f;
    settings["ImpactHold"] = 0.0f;
    clamp_("ExecutionStart", 0.1f, 10.0f);
    clamp_("AlignDuration", 0.01f, settings["ExecutionStart"].get<float>());
    clamp_("SlamTime", 0.0f, 1.9166667f);
    clamp_("DissolveDelay", 0.0f, 5.0f);
    clamp_("DissolveDuration", 0.1f, 10.0f);
    clamp_("CounterStart", settings["BlackoutEnd"].get<float>(), 15.0f);
    clamp_("CaptureTime", settings["CounterStart"].get<float>() + 0.1f, 25.0f);
    clamp_("HoldTime", 0.0f, 10.0f);
    clamp_("PromptDelay", settings["HoldTime"].get<float>() + RESULT_RANK_DELAY + RESULT_RANK_REVEAL, 20.0f);
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
    const float runDuration_ = player_["PoseTime"].get<float>();
    const float blendDuration_ = settings["AlignDuration"].get<float>();
    settings["ExecutionStart"] = (std::max)(settings["ExecutionStart"].get<float>(), runDuration_ + blendDuration_);
    const float approach_ = player_.value("ApproachTime", 0.55f);
    player_["ApproachTime"] = std::isfinite(approach_) ? std::clamp(approach_, 0.01f, runDuration_) : 0.55f;
    auto& boss_ = settings["Boss"];
    const float distance_ = boss_.value("ExecutionDistance", 1.8f);
    boss_["ExecutionDistance"] = std::isfinite(distance_) ? std::clamp(distance_, 0.8f, 4.0f) : 1.8f;
    const auto position_ = ReadPosition(boss_["Position"]);
    const float yaw_ = DirectX::XMConvertToRadians(player_.value("Yaw", 0.0f));
    player_["EndPosition"] = {position_.x - std::sin(yaw_)*boss_["ExecutionDistance"].get<float>(), position_.y,
        position_.z - std::cos(yaw_)*boss_["ExecutionDistance"].get<float>()};
    settings["CaptureTime"] = (std::max)(GetCaptureTime(), GetImpactTime() + settings["ImpactHold"].get<float>() + 0.15f);
    settings["CaptureTime"] = (std::max)(GetCaptureTime(), GetImpactTime() +
        settings["DissolveDelay"].get<float>() + settings["DissolveDuration"].get<float>() + 1.0f);
    // キーが無い従来データは固定カメラのまま扱い、新規キーだけを時刻順に整える。
    auto& camera_ = settings["Camera"];
    if (camera_.contains("Keys") && camera_["Keys"].is_array())
    {
        std::vector<nlohmann::json> keys_;
        for (auto key_ : camera_["Keys"])
        {
            if (!key_.is_object())
            {
                continue;
            }
            const float time_ = key_.value("Time", 0.0f);
            key_["Time"] = std::isfinite(time_) ? std::clamp(time_, 0.0f, GetCaptureTime() + settings["HitStop"].get<float>()) : 0.0f;
            for (const char* property_ : { "Position", "Focus" })
            {
                if (!key_.contains(property_) || !key_[property_].is_array() || key_[property_].size() != 3)
                {
                    key_[property_] = camera_[property_];
                }
                for (std::size_t index_ = 0; index_ < 3; ++index_)
                {
                    if (!key_[property_][index_].is_number() || !std::isfinite(key_[property_][index_].get<float>()))
                    {
                        key_[property_][index_] = camera_[property_][index_];
                    }
                }
            }
            const float fov_ = key_.value("Fov", camera_.value("Fov", 43.0f));
            const float roll_ = key_.value("Roll", camera_.value("Roll", 0.0f));
            key_["Fov"] = std::isfinite(fov_) ? std::clamp(fov_, 15.0f, 100.0f) : 43.0f;
            key_["Roll"] = std::isfinite(roll_) ? std::clamp(roll_, -180.0f, 180.0f) : 0.0f;
            if (!key_.contains("Easing") || !key_["Easing"].is_string())
            {
                key_["Easing"] = "Smooth";
            }
            keys_.push_back(std::move(key_));
        }
        std::stable_sort(keys_.begin(), keys_.end(), [](const auto& left_, const auto& right_)
        {
            return left_.at("Time").template get<float>() < right_.at("Time").template get<float>();
        });
        camera_["Keys"] = keys_;
    }
}

float BossFinishDirector::GetImpactTime() const
{
    return settings.at("ExecutionStart").get<float>() + settings.at("SlamTime").get<float>();
}

bool BossFinishDirector::Load()
{
    if (LoadPreset(PRESET_PATH))
    {
        return true;
    }
    settings = DefaultSettings();
    return false;
}

bool BossFinishDirector::LoadPreset(const std::string& path_)
{
    BS_PROFILE_SCOPE("BossFinishDirector::LoadPreset");
    const auto previous_ = settings;
    try
    {
        const auto pathOnDisk_ = std::filesystem::absolute(WindowsDialogHelper::Utf8ToPath(path_));
        std::ifstream file_(pathOnDisk_);
        if (!file_)
        {
            throw std::runtime_error("preset file missing");
        }
        nlohmann::json loaded_;
        file_ >> loaded_;
        if (!loaded_.is_object() || (loaded_.contains("AssetType") && loaded_.value("AssetType", std::string()) != "BossFinishCinematic"))
        {
            throw std::runtime_error("not a boss finish cinematic preset");
        }
        auto merged_ = DefaultSettings();
        merged_.merge_patch(loaded_);
        // 必須の配列構造を検証し、読込に失敗した演出案では現在の編集内容を維持する。
        ReadPosition(merged_.at("Anchor"));
        for (const char* actor_ : { "Player", "Boss" })
        {
            ReadPosition(merged_.at(actor_).at("Position"));
        }
        ReadPosition(merged_.at("Camera").at("Position"));
        ReadPosition(merged_.at("Camera").at("Focus"));
        ReadColor(merged_.at("Background"));
        ReadColor(merged_.at("ImageTint"));
        for (const auto& effect_ : merged_.at("Effects"))
        {
            ReadPosition(effect_.at("Position"));
            ReadPosition(effect_.at("Rotation"));
            ReadPosition(effect_.at("Scale"));
            ReadColor(effect_.at("Color"));
            if (effect_.contains("Anchor"))
            {
                ReadPosition(effect_.at("Offset"));
            }
        }
        ValidateResultRanks(merged_.at("ResultRanks"));
        settings = std::move(merged_);
        Normalize();
        status = std::string("Loaded: ") + WindowsDialogHelper::PathToUtf8(pathOnDisk_);
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

bool BossFinishDirector::ReloadResultRanks()
{
    try
    {
        const auto path_ = std::filesystem::absolute(WindowsDialogHelper::Utf8ToPath(PRESET_PATH));
        std::ifstream file_(path_);
        if (!file_)
        {
            throw std::runtime_error("preset file missing");
        }
        nlohmann::json preset_;
        file_ >> preset_;
        if (!preset_.is_object() || (preset_.contains("AssetType") &&
            preset_.value("AssetType", std::string()) != "BossFinishCinematic"))
        {
            throw std::runtime_error("not a boss finish cinematic preset");
        }
        auto ranks_ = DefaultSettings().at("ResultRanks");
        if (preset_.contains("ResultRanks"))
        {
            ranks_.merge_patch(preset_.at("ResultRanks"));
        }
        ValidateResultRanks(ranks_);
        settings["ResultRanks"] = std::move(ranks_);
        status = std::string("Rank criteria loaded: ") + WindowsDialogHelper::PathToUtf8(path_);
        return true;
    }
    catch (const std::exception& error_)
    {
        status = std::string("Finish preset fallback: ") + error_.what();
        BS_LOG_WARNING(LOG_CATEGORY::GAME, status);
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
        if (destination_.empty())
        {
            return false;
        }
        if (!destination_.parent_path().empty())
        {
            std::filesystem::create_directories(destination_.parent_path());
        }
        std::ofstream file_(destination_, std::ios::binary);
        // Windowsでも明示的にCRLFを書き、UTF-8文字列へBOMを付与しない。
        auto asset_ = settings;
        asset_["AssetType"] = "BossFinishCinematic";
        asset_["AssetVersion"] = 1;
        const auto text_ = asset_.dump(4, ' ', false);
        for (const char character_ : text_)
        {
            if (character_ == '\n')
            {
                file_.put('\r');
            }
            file_.put(character_);
        }
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
    actor_.hasRootReference = false;
    try
    {
        std::ifstream file_(WindowsDialogHelper::Utf8ToPath(config_.at("StateAsset").get<std::string>()));
        nlohmann::json data_;
        if (!file_)
        {
            throw std::runtime_error("animation file missing");
        }
        file_ >> data_;
        PrepareActor(actor_, actor_.object, data_.at("State").get<AnimState>(),
            config_.value("Animation", std::string()), config_.value("Loop", false));
        if (!actor_.ready)
        {
            throw std::runtime_error("actor or animation index unavailable");
        }
        const auto model_ = actor_.object->GetComponent<GltfModelCommponennt>();
        model_->SetRuntimeVisible(true);
        SampleActor(actor_, 0.0f);
    }
    catch (const std::exception& error_)
    {
        status = std::string("Finish actor fallback (") + key_ + "): " + error_.what();
        BS_LOG_INFO(LOG_CATEGORY::GAME, status);
    }
}

void BossFinishDirector::PrepareActor(ActorPose& actor_, const std::shared_ptr<GameObject>& object_,
    AnimState state_, const std::string& animation_, bool loop_)
{
    actor_.object = object_;
    actor_.ready = false;
    actor_.hasRootReference = false;
    const auto model_ = object_ ? object_->GetComponent<GltfModelCommponennt>() : nullptr;
    if (!model_ || !model_->IsModelReady())
    {
        return;
    }
    const auto& animations_ = model_->GetAnimations();
    if (!animation_.empty())
    {
        const auto found_ = std::find_if(animations_.begin(), animations_.end(),
            [&](const auto& clip_)
        {
            return clip_.name == animation_;
        });
        if (found_ == animations_.end())
        {
            return;
        }
        state_.animationIndex = static_cast<int>(found_ - animations_.begin());
        state_.name = animation_;
    }
    if (state_.animationIndex < 0 || state_.animationIndex >= static_cast<int>(animations_.size()))
    {
        return;
    }
    // 専用Animatorから戦闘イベントを実行せず、必要なクリップだけ姿勢を評価する。
    state_.events.clear();
    state_.isLoop = loop_;
    state_.playbackSpeed = 0.0f;
    state_.rootMotion.enable = false;
    state_.rootMotion.cutVisualWhenDisabled = actor_.constrainHorizontal;
    state_.rootMotion.cutVisualPositionX = actor_.constrainHorizontal;
    state_.rootMotion.cutVisualPositionZ = actor_.constrainHorizontal;
    actor_.state = std::move(state_);
    actor_.animator = ModelAnimator{};
    actor_.animator.CrossFadeTo(&actor_.state, 0.0f);
    actor_.ready = true;
}

void BossFinishDirector::SampleActor(ActorPose& actor_, float time_)
{
    if (!actor_.ready || !actor_.object)
    {
        return;
    }
    const auto model_ = actor_.object->GetComponent<GltfModelCommponennt>();
    if (!model_)
    {
        return;
    }
    const float duration_ = actor_.animator.GetCurrentSampleDuration();
    const float sample_ = duration_ > 0.0f ? (actor_.state.isLoop ? std::fmod(time_, duration_) : (std::min)(time_, duration_)) : time_;
    actor_.animator.SetCurrentTime((std::max)(sample_, 0.0f));
    actor_.animator.Update(0.0f, model_.get(), nullptr);
    auto poses_ = actor_.animator.GetCurrentPose();
    // SetCurrentTimeでAnimatorのRootMotion基準が初期化されるため、演出側の固定基準で補正する。
    if (actor_.constrainHorizontal)
    {
        ConstrainActorPose(actor_, poses_);
    }
    model_->SetNodePoses(poses_);
    model_->UpdateTransform();
}

// 骨盤までの親階層を含めて水平移動を除去し、配置移動との二重加算を防ぐ。
void BossFinishDirector::ConstrainActorPose(ActorPose& actor_, std::vector<GltfModelCommponennt::NodePose>& poses_)
{
    BS_PROFILE_SCOPE("BossFinishDirector::ConstrainActorPose");
    const auto model_ = actor_.object ? actor_.object->GetComponent<GltfModelCommponennt>() : nullptr;
    if (!model_)
    {
        return;
    }
    const int rootIndex_ = model_->GetNodeIndex(actor_.state.rootMotion.sourceNodeName.empty()
        ? "pelvis" : actor_.state.rootMotion.sourceNodeName.c_str());
    const auto& nodes_ = model_->GetNodes();
    if (rootIndex_ < 0 || rootIndex_ >= static_cast<int>(poses_.size()) || rootIndex_ >= static_cast<int>(nodes_.size()))
    {
        return;
    }
    using namespace DirectX::SimpleMath;
    Matrix parentMatrix_ = Matrix::Identity;
    int parentIndex_ = nodes_[rootIndex_].parentIndex;
    for (std::size_t depth_ = 0; parentIndex_ >= 0 && depth_ < nodes_.size(); ++depth_)
    {
        if (parentIndex_ >= static_cast<int>(nodes_.size()) || parentIndex_ >= static_cast<int>(poses_.size()))
        {
            return;
        }
        const auto& pose_ = poses_[parentIndex_];
        parentMatrix_ *= Matrix::CreateScale(pose_.scale) * Matrix::CreateFromQuaternion(pose_.rotation) * Matrix::CreateTranslation(pose_.position);
        parentIndex_ = nodes_[parentIndex_].parentIndex;
    }
    auto position_ = Vector3::Transform(poses_[rootIndex_].position, parentMatrix_);
    if (!actor_.hasRootReference)
    {
        actor_.rootReferencePosition = position_;
        actor_.hasRootReference = true;
    }
    position_.x = actor_.rootReferencePosition.x;
    position_.z = actor_.rootReferencePosition.z;
    // 叩きつけの上下動は残し、水平成分だけを骨盤のローカル座標へ戻す。
    poses_[rootIndex_].position = Vector3::Transform(position_, parentMatrix_.Invert());
}

// 同じ固定位置で双方の姿勢をつなぎ、クリップ切り替え時の見た目の位置飛びを防ぐ。
void BossFinishDirector::BlendExecutionPose(ActorPose& source_, ActorPose& execution_, float sourceTime_, float blend_)
{
    BS_PROFILE_SCOPE("BossFinishDirector::BlendExecutionPose");
    if (!source_.ready || !execution_.ready || !source_.object)
    {
        return;
    }
    const auto model_ = source_.object->GetComponent<GltfModelCommponennt>();
    if (!model_)
    {
        return;
    }
    SampleActor(source_, sourceTime_);
    std::vector<GltfModelCommponennt::NodePose> sourcePoses_;
    model_->GetNodePoses(sourcePoses_);
    SampleActor(execution_, 0.0f);
    std::vector<GltfModelCommponennt::NodePose> poses_;
    model_->GetNodePoses(poses_);
    if (sourcePoses_.size() != poses_.size())
    {
        return;
    }
    using namespace DirectX;
    const float ratio_ = std::clamp(blend_, 0.0f, 1.0f);
    const float eased_ = ratio_ * ratio_ * (3.0f - 2.0f * ratio_);
    for (std::size_t index_ = 0; index_ < poses_.size(); ++index_)
    {
        auto& pose_ = poses_[index_];
        const auto& sourcePose_ = sourcePoses_[index_];
        XMStoreFloat3(&pose_.position, XMVectorLerp(XMLoadFloat3(&sourcePose_.position), XMLoadFloat3(&pose_.position), eased_));
        XMStoreFloat3(&pose_.scale, XMVectorLerp(XMLoadFloat3(&sourcePose_.scale), XMLoadFloat3(&pose_.scale), eased_));
        XMStoreFloat4(&pose_.rotation, XMQuaternionSlerp(XMLoadFloat4(&sourcePose_.rotation), XMLoadFloat4(&pose_.rotation), eased_));
    }
    ConstrainActorPose(execution_, poses_);
    model_->SetNodePoses(poses_);
    model_->UpdateTransform();
}

void BossFinishDirector::Begin(bool preview_, Ending previewEnding_)
{
    BS_PROFILE_SCOPE("BossFinishDirector::Begin");
    if (active || manager.IsResultPreviewScene())
    {
        return;
    }
    if (!preview_)
    {
        ReloadResultRanks();
    }
    Normalize();
    preview = preview_;
    active = true;
    paused = false;
    showFixed = false;
    captureRequested = false;
    inputArmed = false;
    returning = false;
    titleRequested = false;
    elapsed = 0.0f;
    returnElapsed = 0.0f;
    effectsTime = 0.0f;
    previewCameraOverride = false;
    previewPlaybackRate = 1.0f;
    impactReady = false;
    impactConfirmed = false;
    completionCommitted = false;
    finishAuthorized = false;
    ending = preview ? previewEnding_ : Ending::TRUE_END;
    if (!preview)
    {
        previousBgmVolumes = AudioManager::Instance().CaptureBgmVolumes();
    }
    if (auto mode_ = manager.GetGameModeManager())
    {
        // BOSS_DEFEATEDへ到達した時点で戦闘側の有効打は受付済み。
        // 演出用クリップの準備状態を、もう一度撃破を要求する条件にはしない。
        finishAuthorized = !preview && mode_->GetCurrentGameState() == GameProgressState::BOSS_DEFEATED;
        if (const auto death_ = manager.GetPlayerDeathDirector(); finishAuthorized && death_ && death_->HasReachedGameOver())
        {
            ending = Ending::BAD_END;
        }
        previousPlayerLock = mode_->IsPlayerControlLocked();
        previousAILock = mode_->IsEnemyAILocked();
        previousPresentation = mode_->IsPresentationPlaying();
        previousGameplayLock = mode_->IsGameplayLocked();
        mode_->SetPlayerControlLocked(true);
        mode_->SetEnemyAILocked(true);
        mode_->SetPresentationPlaying(true);
        mode_->SetGameplayLocked(true);
    }
    // Scene全体を進めないため、暴走・弾・危険範囲・RootMotion・物理の予約も停止したまま保持できる。
    // プレビュー中断では元の攻防を壊さず、本番ではScene破棄が予約をまとめて解放する。
    for (const auto& object_ : manager.GetObjects())
    {
        if (!object_ || object_->IsPendingDestroy())
        {
            continue;
        }
        SavedActor saved_;
        saved_.object = object_;
        saved_.transform = *object_->transform;
        for (const auto& collider_ : object_->colliderComponents)
        {
            saved_.colliderStates.emplace_back(collider_,collider_->IsRuntimeSuppressed());
            collider_->SetRuntimeSuppressed(true);
        }
        if (const auto model_ = object_->GetComponent<GltfModelCommponennt>())
        {
            model_->GetNodePoses(saved_.poses);
            saved_.visible = model_->IsRuntimeVisible();
        }
        savedActors.push_back(std::move(saved_));
    }
    camera = std::make_shared<CameraComponent>();
    PrepareActor(player, "Player");
    PrepareActor(boss, "Boss");
    PrepareExecution();
    PrepareSprites();
    CreateEffects();
    Evaluate(0.0f, 0.0f);
}

void BossFinishDirector::ResetBadEnding()
{
    if (badEndingTime < 0.0f)
    {
        return;
    }
    if (const auto model_ = player.object ? player.object->GetComponent<GltfModelCommponennt>() : nullptr)
    {
        model_->SetRuntimeSilhouette(previousSilhouette, previousSilhouetteColor);
    }
    for (const auto& state_ : badEndingMaterialStates)
    {
        if (const auto component_ = state_.first.lock())
        {
            component_->SetRuntimeSuppressed(state_.second);
        }
    }
    for (const auto& saved_ : savedActors)
    {
        if (const auto model_ = saved_.object->GetComponent<GltfModelCommponennt>())
        {
            model_->ClearPresentationMaterials();
        }
    }
    badEndingMaterialStates.clear();
    badEndingTime = -1.0f;
    badEndingCounter.object.reset();
    badEndingKneel.object.reset();
    badEndingPierced.object.reset();
    badEndingRise.object.reset();
    badEndingMud.reset();
    badEndingSpear.reset();
    badEndingParticles.reset();
    flashbackSprite.reset();
    badEndingGlass.reset();
    effects.clear();
}

void BossFinishDirector::Stop()
{
    BS_PROFILE_SCOPE("BossFinishDirector::Stop");
    if (!active && !resultPreviewReady)
    {
        return;
    }
    imageSprite.reset();
    solidSprite.reset();
    clearSprite.reset();
    promptSprite.reset();
    resultLabelsSprite.reset();
    resultValuesSprite.reset();
    resultRanksSprite.reset();
    overallLabelSprite.reset();
    overallRankSprite.reset();
    rankBurstSprite.reset();
    resultPreviewReady = false;
    if (!active)
    {
        ending = Ending::TRUE_END;
        return;
    }
    ResetBadEnding();
    ResetTrueEnding();
    ending = Ending::TRUE_END;
    for (auto& saved_ : savedActors)
    {
        if (!saved_.object || saved_.object->IsPendingDestroy())
        {
            continue;
        }
        *saved_.object->transform = saved_.transform;
        for (const auto& state_ : saved_.colliderStates)
        {
            if (const auto collider_ = state_.first.lock())
            {
                collider_->SetRuntimeSuppressed(state_.second);
            }
        }
        if (const auto model_ = saved_.object->GetComponent<GltfModelCommponennt>())
        {
            model_->SetNodePoses(saved_.poses);
            model_->SetRuntimeVisible(saved_.visible);
            model_->UpdateTransform();
        }
    }
    for (auto& saved_ : savedActors)
    {
        if (saved_.object)
        {
            if (const auto dissolve_ = saved_.object->GetComponent<ModelDissolveComponent>())
            {
                dissolve_->ClearPresentationProgress();
            }
        }
    }
    savedActors.clear();
    effects.clear();
    if (!preview)
    {
        AudioManager::Instance().ApplyBgmVolumes(previousBgmVolumes,1.0f);
    }
    previousBgmVolumes.clear();
    capturedView.Reset();
    capturedTexture.Reset();
    camera.reset();
    player.object.reset();
    boss.object.reset();
    player.ready = false;
    boss.ready = false;
    bossReaction.object.reset();
    bossReaction.ready = false;
    playerExecution.object.reset();
    playerExecution.ready = false;
    if (preview)
    {
        if (auto mode_ = manager.GetGameModeManager())
    {
        mode_->SetPlayerControlLocked(previousPlayerLock);
            mode_->SetEnemyAILocked(previousAILock);
        mode_->SetPresentationPlaying(previousPresentation);
            mode_->SetGameplayLocked(previousGameplayLock);
    }
    }
    active = false;
    showFixed = false;
    captureRequested = false;
    previewCameraOverride = false;
    impactReady = false;
    impactConfirmed = false;
    completionCommitted = false;
    finishAuthorized = false;
}

// 叩きつける側と受ける側を別Animatorで準備し、同じ演出時計で評価する。
void BossFinishDirector::PrepareExecution()
{
    BS_PROFILE_SCOPE("BossFinishDirector::PrepareExecution");
    PrepareActor(playerExecution, "PlayerExecution");
    PrepareActor(bossReaction, "BossExecution");
    // アニメーションごとの開始位置を採用せず、移動前の骨格位置を次のクリップへ引き継ぐ。
    playerExecution.rootReferencePosition = player.rootReferencePosition;
    playerExecution.hasRootReference = player.hasRootReference;
    bossReaction.rootReferencePosition = boss.rootReferencePosition;
    bossReaction.hasRootReference = boss.hasRootReference;
    impactPosition = AddPosition(GetStageAnchor(), ReadPosition(settings["Boss"]["Position"]));
    impactReady = player.ready && boss.ready && playerExecution.ready && bossReaction.ready;
}

// ステージの水平中心を基準にし、接地高さは演出設定で調整する。
DirectX::XMFLOAT3 BossFinishDirector::GetStageAnchor() const
{
    auto position_ = ReadPosition(settings.at("Anchor"));
    const auto stage_ = manager.Find(settings.value("StageCenterObject", std::string("Stage")));
    if (stage_ && stage_->transform)
    {
        const auto center_ = stage_->transform->GetWorldPosition();
        position_.x += center_.x;
        position_.z += center_.z;
    }
    return position_;
}

DirectX::XMFLOAT3 BossFinishDirector::GetEffectPosition(const nlohmann::json& config_) const
{
    const auto anchorType_ = config_.value("Anchor", std::string());
    if (anchorType_ == "Impact")
    {
        return AddPosition(impactPosition, ReadPosition(config_.at("Offset")));
    }
    if (anchorType_ == "Boss" && boss.object)
    {
        // 胴体から落ちる粒子は死亡ポーズのノードへ追従し、地面の演出は足元に保つ。
        auto position_ = boss.object->transform->GetWorldPosition();
        const auto nodeName_ = config_.value("Node", std::string());
        if (!nodeName_.empty())
        {
            ReadNodePosition(boss.object, nodeName_, position_);
        }
        return AddPosition(position_, ReadPosition(config_.at("Offset")));
    }
    if (anchorType_ == "Player" && player.object)
    {
        // プレイヤーに付ける演出も、敵と同じノード追従の規則で配置する。
        auto position_ = player.object->transform->GetWorldPosition();
        const auto nodeName_ = config_.value("Node", std::string());
        if (!nodeName_.empty())
        {
            ReadNodePosition(player.object, nodeName_, position_);
        }
        return AddPosition(position_, ReadPosition(config_.at("Offset")));
    }
    // 旧プリセットの絶対配置はそのまま読み込み、新形式だけ基準位置とオフセットを使う。
    return AddPosition(GetStageAnchor(), ReadPosition(config_.at("Position")));
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
        object_->transform->MarkDirty();
        object_->transform->UpdateWorldMatrix();
        try
        {
            const auto path_ = config_.at("Asset").get<std::string>();
            if (config_.value("Kind", "Mesh") == "Particle")
            {
                auto particle_ = object_->AddComponent<ParticleComponent>();
                if (!particle_->LoadAsset(path_))
                {
                    throw std::runtime_error(path_);
                }
                if (config_.value("MeshSourceObject", false))
                {
                    // 倒れた敵のスキニング済み頂点から細粒を発生させる。
                    nlohmann::json particleConfig_;
                    particle_->Serialize(particleConfig_);
                    particleConfig_["MeshSourceObjectName"] = settings["Boss"]["Object"];
                    particle_->Deserialize(particleConfig_);
                }
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
                    track_.settings.emissionSize.x *= std::abs(scale_.x);
                    track_.settings.emissionSize.y *= std::abs(scale_.x);
                    track_.settings.shapeSize.x *= std::abs(scale_.x);
                    track_.settings.shapeSize.y *= std::abs(scale_.y);
                    track_.settings.shapeSize.z *= std::abs(scale_.z);
                    const auto tintColor_ = [&](DirectX::XMFLOAT4& color_)
                    {
                        color_.x *= tint_.x*emission_;
                        color_.y *= tint_.y*emission_;
                        color_.z *= tint_.z*emission_;
                        color_.w *= tint_.w;
                    };
                    tintColor_(track_.settings.manualColor);
                    tintColor_(track_.settings.colorStart);
                    tintColor_(track_.settings.colorEnd);
                }
                particle_->ResetRuntime();
                particle_->Play();
                particle_->SetRuntimeSuppressed(true);
            }
            else
            {
                auto mesh_ = object_->AddComponent<MeshVFXComponent>();
                if (!mesh_->LoadAsset(path_))
                {
                    throw std::runtime_error(path_);
                }
                for (auto& part_ : mesh_->GetAsset().parts)
                {
                    part_.renderEnabled = true;
                    part_.emissiveStrength = config_.value("Emission", 1.0f);
                }
                mesh_->SetRuntimeAppearance({1,1,1}, ReadColor(config_["Color"]), ReadColor(config_["Color"]));
                mesh_->Reset();
                mesh_->Play();
                mesh_->SetRuntimeSuppressed(true);
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
        const auto timeAnchor_ = config_.value("TimeAnchor", std::string());
        const float origin_ = timeAnchor_ == "Dissolve" ? GetImpactTime() + settings["DissolveDelay"].get<float>() :
            timeAnchor_ == "Impact" ? GetImpactTime() : 0.0f;
        const float start_ = origin_ + config_.value("Start", 0.0f);
        const float stop_ = origin_ + config_.value("Stop", GetCaptureTime() - origin_);
        const float limit_ = (std::max)(config_.value("PoseTime", 10.0f), 0.0f);
        const float previous_ = std::clamp(from_ - start_, 0.0f, limit_);
        const float current_ = std::clamp(to_ - start_, 0.0f, limit_);
        const auto& effect_ = effects[index_];
        effect_->transform->localPosition = GetEffectPosition(config_);
        effect_->transform->MarkDirty();
        effect_->transform->UpdateWorldMatrix();
        const bool visible_ = to_ >= start_ && to_ <= stop_ && (!config_.value("RequiresImpact", false) || impactConfirmed);
        if (const auto particle_ = effect_->GetComponent<ParticleComponent>())
        {
            // GPUの直接シークは使わず、リセット後も本番も同じ固定刻みでシミュレーションする。
            particle_->SetRuntimeSuppressed(!visible_);
            // 敵の消滅粒子は開始直後から全量を発生させ、短い演出でも十分な密度を確保する。
            const float growth_ = (config_.value("MeshSourceObject", false) || config_.value("RequiresImpact", false))
                ? 1.0f : std::clamp(current_ / 1.8f, 0.15f, 1.0f);
            const float emitDuration_ = config_.value("MeshSourceObject", false) ? settings["DissolveDuration"].get<float>() :
                config_.value("EmitDuration", limit_ + 1.0f);
            particle_->SetEmissionScale(current_ >= emitDuration_ ? 0.0f : growth_);
            if (current_ > previous_)
            {
                particle_->Update(current_ - previous_);
            }
        }
        if (const auto mesh_ = effect_->GetComponent<MeshVFXComponent>())
        {
            mesh_->SetRuntimeSuppressed(!visible_);
            mesh_->Update((std::max)(current_ - previous_, 0.0f));
        }
    }
}

float BossFinishDirector::GetEndingBranchTime() const
{
    return GetCaptureTime() + settings.value("HitStop", 0.16f);
}

float BossFinishDirector::GetClearTime() const
{
    return GetEndingBranchTime() + TRUE_END_FINISH;
}

float BossFinishDirector::GetResultStartTime(Ending ending_) const
{
    return ending_ == Ending::BAD_END ? GetEndingBranchTime() + BAD_END_FINISH : GetClearTime();
}

float BossFinishDirector::GetPreviewEndTime(Ending ending_) const
{
    return GetResultStartTime(ending_) + settings.value("PromptDelay", 2.8f) + 1.0f;
}

std::span<const BossFinishDirector::EndingCue> BossFinishDirector::GetTrueEndingCues() const
{
    static constexpr EndingCue CUES[] = {
        {"Mud approach / Hand light", 0.0f, TRUE_END_AWAKEN},
        {"Awakening / Golden sword", TRUE_END_AWAKEN, TRUE_END_LUNGE},
        {"Mud lunge / Wide slash", TRUE_END_LUNGE, TRUE_END_CHARGE},
        {"Sword raised / Light gathers", TRUE_END_CHARGE, TRUE_END_BEAM},
        {"Beam / Pillar of light", TRUE_END_BEAM, TRUE_END_FINISH}
    };
    return CUES;
}

std::span<const BossFinishDirector::EndingCue> BossFinishDirector::GetBadEndingCues() const
{
    static constexpr EndingCue CUES[] = {
        {"Mud approach / Counter", 0.0f, BAD_END_FLASHBACK},
        {"Flashback", BAD_END_FLASHBACK, BAD_END_KNEEL},
        {"Kneel", BAD_END_KNEEL, BAD_END_PIERCE},
        {"Spear / Impact", BAD_END_PIERCE, BAD_END_PIERCE + BAD_END_IMPACT_HOLD},
        {"Fall", BAD_END_PIERCE + BAD_END_IMPACT_HOLD, BAD_END_FALL},
        {"Mud gathers", BAD_END_FALL, BAD_END_RISE},
        {"BadEndPlayer rises", BAD_END_RISE, BAD_END_WIPE},
        {"Glass shatter / Blackout", BAD_END_WIPE, BAD_END_FINISH}
    };
    return CUES;
}

void BossFinishDirector::BeginTrueEnding()
{
    BS_PROFILE_SCOPE("BossFinishDirector::BeginTrueEnding");
    trueEndingPosition = player.object ? player.object->transform->GetWorldPosition() : GetStageAnchor();
    PrepareActor(trueEndingIdle, player.object, {}, "Idle_Combat_Seq", true);
    PrepareActor(trueEndingAwaken, player.object, {}, "Buff_Seq", false);
    PrepareActor(trueEndingSweep, player.object, {}, "Combo_Attack_01_01_Seq", false);
    PrepareActor(trueEndingStrike, player.object, {}, "SS_Sp_Skill1", false);
    for (auto* actor_ : {&trueEndingIdle, &trueEndingAwaken, &trueEndingSweep, &trueEndingStrike})
    {
        actor_->rootReferencePosition = playerExecution.rootReferencePosition;
        actor_->hasRootReference = playerExecution.hasRootReference;
    }

    effects.clear();
    trueEndingMud = MakeObject("TrueEndingMud");
    trueEndingHandLight = MakeObject("TrueEndingHandLight");
    trueEndingAura = MakeObject("TrueEndingAura");
    trueEndingBladeLight = MakeObject("TrueEndingBladeLight");
    trueEndingArc = MakeObject("TrueEndingArc");
    trueEndingBeam = MakeObject("TrueEndingBeam");
    trueEndingPillar = MakeObject("TrueEndingPillar");
    effects = {trueEndingMud, trueEndingHandLight, trueEndingAura, trueEndingBladeLight,
        trueEndingArc, trueEndingBeam, trueEndingPillar};
    for (const auto& effect_ : effects)
    {
        effect_->SetManager(&manager);
    }

    const auto mud_ = trueEndingMud->AddComponent<MeshVFXComponent>();
    const auto mudParticles_ = trueEndingMud->AddComponent<ParticleComponent>();
    const auto hand_ = trueEndingHandLight->AddComponent<ParticleComponent>();
    const auto aura_ = trueEndingAura->AddComponent<ParticleComponent>();
    const auto pillarParticles_ = trueEndingPillar->AddComponent<ParticleComponent>();
    try
    {
        if (!mud_->LoadAsset("Data/VFX/TransitionMudVortex.json"))
        {
            throw std::runtime_error("Data/VFX/TransitionMudVortex.json");
        }
        for (auto& part_ : mud_->GetAsset().parts)
        {
            part_.renderEnabled = true;
            part_.animateScale = false;
            part_.scale = {1.45f, 1.05f, 1.45f};
        }
        mud_->Play();
        if (!mudParticles_->LoadAsset("Data/Particle/TransitionMudVortex.json"))
        {
            throw std::runtime_error("Data/Particle/TransitionMudVortex.json");
        }
        for (auto& track_ : mudParticles_->GetTracks())
        {
            track_.spawnSpace = ParticleSpawnSpace::OWNER_RELATIVE;
            track_.settings.shapeSize = {1.3f, 1.3f, 1.3f, 0.0f};
            track_.settings.emissionSize = {0.36f, 0.12f};
            track_.settings.burstCount = 95;
        }
        mudParticles_->Play();

        for (const auto& light_ : {hand_, aura_, pillarParticles_})
        {
            if (!light_->LoadAsset("Data/Particle/IntroGateLight.json"))
            {
                throw std::runtime_error("Data/Particle/IntroGateLight.json");
            }
            const bool isHand_ = light_ == hand_;
            const bool isPillar_ = light_ == pillarParticles_;
            for (auto& track_ : light_->GetTracks())
            {
                track_.spawnSpace = ParticleSpawnSpace::OWNER_RELATIVE;
                auto& config_ = track_.settings;
                config_.emissionPosition = {0, isPillar_ ? 0.2f : 0.0f, 0, 1};
                config_.emissionMode = 0;
                config_.burstCount = isHand_ ? 170 : 520;
                config_.shapeSize = isHand_ ? DirectX::XMFLOAT4{0.12f, 0.12f, 0.12f, 0} :
                    DirectX::XMFLOAT4{1.25f, isPillar_ ? 0.15f : 1.75f, 1.25f, 0};
                config_.emissionSize = {isPillar_ ? 0.11f : 0.075f, 0.015f};
                config_.lifespan = {0.35f, isPillar_ ? 1.15f : 0.95f};
                config_.emissionSpeed = isPillar_ ? DirectX::XMFLOAT2{4.5f, 10.0f} : DirectX::XMFLOAT2{0.6f, 2.4f};
                config_.emissionConeAngle = {0.0f, isPillar_ ? 12.0f : 42.0f};
                config_.orbitVelocity = isHand_ || isPillar_ ? 0.0f : 2.7f;
                config_.colorStart = {4.0f, 2.3f, 0.65f, 0.95f};
                config_.colorEnd = {1.4f, 0.55f, 0.08f, 0.0f};
            }
            light_->Play();
        }
    }
    catch (const std::exception& error_)
    {
        status = std::string("True ending VFX: ") + error_.what();
        BS_LOG_WARNING(LOG_CATEGORY::GAME, status);
    }

    const auto makeLight_ = [](const std::shared_ptr<GameObject>& object_, const char* shape_, float emission_,
        const DirectX::XMFLOAT3& outerScale_ = {})
    {
        MeshVFXAsset asset_;
        auto& part_ = asset_.parts.emplace_back();
        part_.modelPath = shape_;
        part_.meshIndex = 0;
        part_.renderEnabled = true;
        part_.useMaterialBaseTexture = false;
        part_.useMaterialNormalTexture = false;
        part_.blendMode = MeshVFXBlendMode::ADDITIVE;
        part_.color = TRUE_END_CORE;
        part_.edgeColor = TRUE_END_GOLD;
        part_.emissiveStrength = emission_;
        part_.emissivePulseAmount = 0.0f;
        part_.internalMeshSettings.segments = 64;
        if (outerScale_.x > 0.0f)
        {
            auto glow_ = part_;
            glow_.scale = outerScale_;
            glow_.alpha = 0.4f;
            glow_.emissiveStrength = emission_ * 0.65f;
            glow_.useMask = true;
            glow_.maskTexturePath = "Data/Texture/VFX/T_Free_Magic_Noise2.PNG";
            glow_.maskUVSpeed = {0.15f, -2.0f};
            asset_.parts.push_back(std::move(glow_));
        }
        const auto mesh_ = object_->AddComponent<MeshVFXComponent>();
        mesh_->SetAsset(asset_);
        mesh_->Play();
    };
    makeLight_(trueEndingHandLight, "Data/Model/VFX/Sphere.glb", 7.0f);
    makeLight_(trueEndingBladeLight, "__InternalMeshBreathBeam__", 5.0f);
    makeLight_(trueEndingArc, "__InternalMeshSlashRaisedCrescent__", 8.0f);
    makeLight_(trueEndingBeam, "__InternalMeshBreathBeam__", 14.0f, {1.65f, 1.65f, 1.0f});
    makeLight_(trueEndingPillar, "__InternalMeshCylinder__", 10.0f, {1.65f, 1.0f, 1.65f});
    UpdateTrueEnding(-1.0f, 0.0f);
}

void BossFinishDirector::ResetTrueEnding()
{
    if (trueEndingTime < 0.0f)
    {
        return;
    }
    trueEndingTime = -1.0f;
    for (auto* actor_ : {&trueEndingIdle, &trueEndingAwaken, &trueEndingSweep, &trueEndingStrike})
    {
        actor_->object.reset();
        actor_->ready = false;
    }
    for (auto* effect_ : {&trueEndingMud, &trueEndingHandLight, &trueEndingAura, &trueEndingBladeLight,
        &trueEndingArc, &trueEndingBeam, &trueEndingPillar})
    {
        effect_->reset();
    }
    effects.clear();
}

void BossFinishDirector::AdvanceTrueEnding(float time_)
{
    BS_PROFILE_SCOPE("BossFinishDirector::AdvanceTrueEnding");
    // 手・剣・泥の位置を固定刻みで評価し、シーク先では端数フレームの姿勢も反映する。
    while (trueEndingTime < time_)
    {
        const float next_ = trueEndingTime + FIXED_STEP;
        if (next_ > time_ && !paused && time_ < TRUE_END_FINISH)
        {
            break;
        }
        UpdateTrueEnding(trueEndingTime, (std::min)(next_, time_));
    }
}

void BossFinishDirector::UpdateTrueEnding(float previousTime_, float time_)
{
    trueEndingTime = time_;
    const float yaw_ = settings["Player"].value("Yaw", 0.0f);
    const float radians_ = DirectX::XMConvertToRadians(yaw_);
    const auto offset_ = [&](float x_, float y_, float z_)
    {
        return Vector3{x_ * std::cos(radians_) + z_ * std::sin(radians_), y_,
            z_ * std::cos(radians_) - x_ * std::sin(radians_)};
    };
    const Vector3 playerPosition_{trueEndingPosition};
    const float turn_ = ShotProgress(time_, TRUE_END_AWAKEN + 0.15f, TRUE_END_AWAKEN + 0.95f);
    const float playerYaw_ = yaw_ + 180.0f * turn_;
    PlaceActor(player.object, playerPosition_, playerYaw_);

    const float openingPose_ = (std::max)(0.0f, GetCaptureTime() - settings["ExecutionStart"].get<float>());
    if (time_ < 0.45f && playerExecution.ready && trueEndingIdle.ready)
    {
        BlendExecutionPose(playerExecution, trueEndingIdle, openingPose_, time_ / 0.45f);
    }
    else if (time_ < TRUE_END_AWAKEN)
    {
        SampleActor(trueEndingIdle, time_ - 0.45f);
    }
    else if (time_ < TRUE_END_SWEEP)
    {
        SampleActor(trueEndingAwaken, time_ - TRUE_END_AWAKEN);
    }
    else if (time_ < TRUE_END_CHARGE)
    {
        SampleActor(trueEndingSweep, (time_ - TRUE_END_SWEEP) * 1.12f);
    }
    else if (time_ < TRUE_END_CHARGE + 0.15f && trueEndingSweep.ready && trueEndingStrike.ready)
    {
        BlendExecutionPose(trueEndingSweep, trueEndingStrike, 1.16f, (time_ - TRUE_END_CHARGE) / 0.15f);
    }
    else if (time_ < TRUE_END_BEAM)
    {
        // 剣を掲げた区間を引き延ばし、振り下ろしの速度と光の溜めを分ける。
        const float raised_ = ShotProgress(time_, TRUE_END_CHARGE + 0.15f, TRUE_END_CHARGE + 0.55f);
        const float release_ = ShotProgress(time_, TRUE_END_BEAM - 0.28f, TRUE_END_BEAM);
        SampleActor(trueEndingStrike, TRUE_END_RAISED_POSE * raised_ +
            (TRUE_END_RELEASE_POSE - TRUE_END_RAISED_POSE) * release_);
    }
    else if (time_ < TRUE_END_BEAM + 1.35f)
    {
        SampleActor(trueEndingStrike, TRUE_END_RELEASE_POSE + (time_ - TRUE_END_BEAM) * 1.15f);
    }
    else if (time_ < TRUE_END_BEAM + 1.75f && trueEndingStrike.ready && trueEndingIdle.ready)
    {
        BlendExecutionPose(trueEndingStrike, trueEndingIdle, 2.3333335f, (time_ - TRUE_END_BEAM - 1.35f) / 0.4f);
    }
    else
    {
        SampleActor(trueEndingIdle, time_ - TRUE_END_BEAM - 1.75f);
    }

    const auto weapon_ = manager.Find(settings["Player"].value("WeaponObject", std::string("Weapon")));
    for (const auto& object_ : {weapon_, manager.Find("Shield")})
    {
        if (!object_)
        {
            continue;
        }
        if (const auto attachment_ = object_->GetComponent<ModelAttachmentComponent>())
        {
            attachment_->SyncAttachment();
        }
        object_->transform->UpdateWorldMatrix();
        if (const auto model_ = object_->GetComponent<GltfModelCommponennt>())
        {
            model_->UpdateTransform();
        }
    }
    auto handPosition_ = playerPosition_ + offset_(0.4f, 1.0f, 0.0f);
    ReadNodePosition(player.object, "hand_r", handPosition_);
    auto bladeStart_ = handPosition_;
    auto bladeEnd_ = handPosition_ + offset_(0.0f, 0.5f, std::cos(DirectX::XM_PI * turn_));
    ReadNodePosition(weapon_, settings["Player"].value("BladeStartNode", std::string("TrailStart")), bladeStart_);
    ReadNodePosition(weapon_, settings["Player"].value("BladeEndNode", std::string("TrailEnd")), bladeEnd_);

    const float approach_ = ShotProgress(time_, 0.0f, TRUE_END_HAND_LIGHT);
    const float recoil_ = ShotProgress(time_, TRUE_END_HAND_LIGHT, TRUE_END_HAND_LIGHT + 0.32f);
    const float lunge_ = ShotProgress(time_, TRUE_END_LUNGE, TRUE_END_SWEEP_HIT);
    const float repel_ = ShotProgress(time_, TRUE_END_SWEEP_HIT, TRUE_END_CHARGE);
    const float dissolve_ = ShotProgress(time_, TRUE_END_PILLAR, TRUE_END_PILLAR + 0.85f);
    const float distress_ = repel_ * (1.0f - ShotProgress(time_, TRUE_END_CHARGE, TRUE_END_BEAM));
    const float lungeDepth_ = TRUE_END_IMPACT_DEPTH + 2.9f * approach_ - 0.85f * recoil_ + 1.95f * lunge_;
    const float mudDepth_ = std::lerp(lungeDepth_, TRUE_END_IMPACT_DEPTH, repel_);
    const Vector3 mudPosition_ = playerPosition_ + offset_(
        std::sin(time_ * 24.0f) * 0.14f * distress_, 0.0f, mudDepth_);
    const Vector3 impact_ = playerPosition_ + offset_(0.0f, 0.0f, TRUE_END_IMPACT_DEPTH);

    const float step_ = time_ - (std::max)(previousTime_, 0.0f);
    const auto updateParticles_ = [&](const std::shared_ptr<GameObject>& object_, float start_, float stop_, float emission_)
    {
        const auto particles_ = object_->GetComponent<ParticleComponent>();
        particles_->SetRuntimeSuppressed(time_ < start_ || time_ >= stop_ + 1.15f);
        if (step_ <= 0.0f || time_ <= start_ || time_ >= stop_ + 1.15f)
        {
            return;
        }
        // 連続発生のレート 0 は既定レートになるため、粒子を残して発生だけ止める。
        if (previousTime_ < stop_ && time_ >= stop_)
        {
            particles_->Stop();
        }
        particles_->SetEmissionScale(emission_);
        particles_->Update((std::min)(step_, time_ - start_));
    };

    PlaceActor(trueEndingMud, mudPosition_ + offset_(0.0f, 0.42f * (1.0f - dissolve_), 0.0f), yaw_);
    const auto mud_ = trueEndingMud->GetComponent<MeshVFXComponent>();
    const float mudWidth_ = (0.65f + 0.22f * approach_ + 0.28f * lunge_) * (1.0f - 0.9f * dissolve_);
    const float flinch_ = recoil_ * (1.0f - ShotProgress(time_, TRUE_END_HAND_LIGHT + 0.32f, TRUE_END_AWAKEN));
    const float mudHeight_ = (0.4f + 0.42f * lunge_ - 0.22f * repel_ - 0.16f * flinch_ +
        0.12f * std::sin(time_ * 27.0f) * distress_) * (1.0f - 0.93f * dissolve_);
    const DirectX::XMFLOAT4 mudColor_{0.14f, 0.025f, 0.042f, 1.0f - dissolve_};
    mud_->SetRuntimeAppearance({mudWidth_, mudHeight_, mudWidth_}, mudColor_, mudColor_);
    mud_->SetRuntimeSuppressed(dissolve_ >= 1.0f);
    mud_->Update(step_);
    for (auto& track_ : trueEndingMud->GetComponent<ParticleComponent>()->GetTracks())
    {
        track_.settings.targetPosition = {mudPosition_.x, mudPosition_.y + 0.55f, mudPosition_.z, 1.0f};
    }
    updateParticles_(trueEndingMud, 0.0f, TRUE_END_PILLAR + 0.55f,
        std::lerp(0.65f, 1.0f, distress_) * (1.0f - dissolve_));

    PlaceActor(trueEndingHandLight, handPosition_, playerYaw_);
    PlaceActor(trueEndingAura, playerPosition_ + offset_(0.0f, 0.92f, 0.0f), playerYaw_);
    updateParticles_(trueEndingHandLight, TRUE_END_HAND_LIGHT, TRUE_END_BEAM + 0.45f, 1.0f);
    const float handFade_ = ShotProgress(time_, TRUE_END_HAND_LIGHT, TRUE_END_HAND_LIGHT + 0.12f) *
        (1.0f - ShotProgress(time_, TRUE_END_BEAM, TRUE_END_BEAM + 0.55f));
    const auto handGlow_ = trueEndingHandLight->GetComponent<MeshVFXComponent>();
    const float handSize_ = (0.14f + 0.03f * std::sin(time_ * 38.0f)) * handFade_;
    handGlow_->SetRuntimeSuppressed(handFade_ <= 0.0f);
    handGlow_->SetRuntimeAppearance({handSize_, handSize_, handSize_},
        {1.0f, 0.68f, 0.2f, handFade_}, TRUE_END_GOLD);

    const float charge_ = ShotProgress(time_, TRUE_END_CHARGE, TRUE_END_BEAM - 0.15f);
    const bool gathering_ = time_ >= TRUE_END_CHARGE && time_ < TRUE_END_BEAM;
    for (auto& track_ : trueEndingAura->GetComponent<ParticleComponent>()->GetTracks())
    {
        track_.settings.useTargetAttraction = gathering_ ? 1u : 0u;
        track_.settings.targetPosition = {bladeEnd_.x, bladeEnd_.y, bladeEnd_.z, 1.0f};
        track_.settings.targetAttractionRange = 5.0f;
        track_.settings.targetAttractionStrength = 17.0f * charge_;
    }
    updateParticles_(trueEndingAura, TRUE_END_AWAKEN, TRUE_END_BEAM + 0.95f, gathering_ ? 1.0f : 0.74f);

    // 内蔵ビームはローカル +Z へ伸びるため、剣先・着弾点から向きと長さを求める。
    const auto aimLight_ = [](const std::shared_ptr<GameObject>& object_, const Vector3& start_, const Vector3& end_,
        float width_, const DirectX::XMFLOAT4& color_, bool visible_)
    {
        const Vector3 direction_ = end_ - start_;
        const float length_ = direction_.Length();
        const auto mesh_ = object_->GetComponent<MeshVFXComponent>();
        mesh_->SetRuntimeSuppressed(!visible_ || length_ < 0.001f);
        if (!visible_ || length_ < 0.001f)
        {
            return;
        }
        const float horizontal_ = std::hypot(direction_.x, direction_.z);
        PlaceActor(object_, start_, DirectX::XMConvertToDegrees(std::atan2(direction_.x, direction_.z)));
        object_->transform->localRotation.x = DirectX::XMConvertToDegrees(-std::atan2(direction_.y, horizontal_));
        object_->transform->MarkDirty();
        object_->transform->UpdateWorldMatrix();
        mesh_->SetRuntimeAppearance({width_, width_, length_}, color_, TRUE_END_GOLD);
    };
    const float bladeFade_ = 1.0f - ShotProgress(time_, TRUE_END_BEAM + 0.25f, TRUE_END_BEAM + 1.8f);
    aimLight_(trueEndingBladeLight, bladeStart_, bladeEnd_, (0.19f + charge_ * 0.42f) * bladeFade_,
        {1.0f, 0.75f, 0.24f, bladeFade_}, time_ >= TRUE_END_AWAKEN && bladeFade_ > 0.0f);

    const float arcTravel_ = ShotProgress(time_, TRUE_END_SWEEP_HIT, TRUE_END_SWEEP_HIT + 0.35f);
    const float arcFade_ = 1.0f - ShotProgress(time_, TRUE_END_SWEEP_HIT + 0.12f, TRUE_END_CHARGE);
    PlaceActor(trueEndingArc, playerPosition_ + offset_(0.0f, 0.88f, 0.0f), yaw_ + 180.0f);
    const auto arc_ = trueEndingArc->GetComponent<MeshVFXComponent>();
    arc_->SetRuntimeSuppressed(time_ < TRUE_END_SWEEP_HIT || arcFade_ <= 0.0f);
    const float arcRadius_ = std::lerp(0.9f, 5.7f, arcTravel_);
    arc_->SetRuntimeAppearance({arcRadius_, 0.44f, arcRadius_}, {1.0f, 0.7f, 0.22f, arcFade_}, TRUE_END_GOLD);

    if (previousTime_ < TRUE_END_BEAM && time_ >= TRUE_END_BEAM)
    {
        trueEndingBeamOrigin = bladeStart_;
    }
    const Vector3 beamOrigin_{trueEndingBeamOrigin};
    const Vector3 beamTarget_ = impact_ + offset_(0.0f, 0.48f, 0.0f);
    const float beamTravel_ = ShotProgress(time_, TRUE_END_BEAM, TRUE_END_PILLAR);
    const float beamFade_ = 1.0f - ShotProgress(time_, TRUE_END_PILLAR + 0.18f, TRUE_END_PILLAR + 0.72f);
    aimLight_(trueEndingBeam, beamOrigin_, Vector3::Lerp(beamOrigin_, beamTarget_, beamTravel_),
        std::lerp(2.6f, 0.5f, 1.0f - beamFade_), {1.0f, 0.92f, 0.66f, beamFade_},
        time_ >= TRUE_END_BEAM && beamFade_ > 0.0f);

    const float pillarRise_ = ShotProgress(time_, TRUE_END_PILLAR, TRUE_END_PILLAR + 0.36f);
    const float pillarFade_ = 1.0f - ShotProgress(time_, TRUE_END_PILLAR + 0.65f, TRUE_END_FINISH - 0.55f);
    const float pillarHeight_ = 0.05f + 5.9f * pillarRise_;
    PlaceActor(trueEndingPillar, impact_, yaw_);
    updateParticles_(trueEndingPillar, TRUE_END_PILLAR, TRUE_END_PILLAR + 0.95f, 1.0f);
    PlaceActor(trueEndingPillar, impact_ + offset_(0.0f, pillarHeight_, 0.0f), yaw_);
    const auto pillar_ = trueEndingPillar->GetComponent<MeshVFXComponent>();
    const float pillarRadius_ = std::lerp(0.22f, 1.3f, pillarRise_) * pillarFade_;
    pillar_->SetRuntimeSuppressed(time_ < TRUE_END_PILLAR || pillarFade_ <= 0.0f);
    pillar_->SetRuntimeAppearance({pillarRadius_, pillarHeight_, pillarRadius_},
        {1.0f, 0.83f, 0.38f, pillarFade_}, TRUE_END_GOLD);

    if (!preview)
    {
        const auto crossed_ = [previousTime_, time_](float cue_)
        {
            return previousTime_ < cue_ && time_ >= cue_;
        };
        auto& audio_ = AudioManager::Instance();
        if (crossed_(0.1f))
        {
            audio_.PlaySEByPath("Data/Audio/SE/Sonniss/Shadow/TentacleMotion.wav");
        }
        if (crossed_(TRUE_END_SWEEP_HIT))
        {
            audio_.PlaySEByPath("Data/Audio/SE/Swordsmanship-2.wav");
        }
        if (crossed_(TRUE_END_BEAM))
        {
            audio_.PlaySEByPath("Data/Audio/SE/Sonniss/Dragon/FireWhoosh.wav");
        }
        if (crossed_(TRUE_END_PILLAR))
        {
            audio_.PlaySEByPath("Data/Audio/SE/Sonniss/Presentation/BassDropFast.wav");
        }
    }

    if (preview && previewCameraOverride)
    {
        return;
    }
    auto headPosition_ = playerPosition_ + offset_(0.0f, 1.52f, 0.0f);
    ReadNodePosition(player.object, "head", headPosition_);
    const Vector3 upperBody_ = headPosition_ - Vector3{0.0f, 0.23f, 0.0f};
    Vector3 eye_;
    Vector3 focus_;
    float fov_;
    float roll_;
    if (time_ < TRUE_END_AWAKEN)
    {
        const float push_ = ShotProgress(time_, 0.0f, TRUE_END_AWAKEN);
        eye_ = upperBody_ + offset_(std::lerp(3.75f, 1.8f, push_), 0.04f, std::lerp(3.0f, 2.15f, push_));
        focus_ = Vector3::Lerp(upperBody_, Vector3{handPosition_}, 0.18f + 0.37f * recoil_);
        fov_ = std::lerp(43.0f, 33.0f, push_);
        roll_ = std::lerp(-2.0f, 0.0f, push_);
    }
    else if (time_ < TRUE_END_LUNGE)
    {
        // 振り向く動きに合わせ、顔の正面寄りを回り込む。
        const float orbit_ = std::lerp(0.68f, 2.88f, turn_);
        eye_ = upperBody_ + offset_(std::sin(orbit_) * 2.7f, -0.26f, std::cos(orbit_) * 2.7f);
        focus_ = upperBody_;
        fov_ = std::lerp(34.0f, 39.0f, turn_);
        roll_ = 2.0f * turn_;
    }
    else if (time_ < TRUE_END_CHARGE)
    {
        eye_ = playerPosition_ + offset_(std::lerp(4.2f, 5.5f, lunge_), 1.12f, std::lerp(-2.35f, -4.1f, lunge_));
        focus_ = Vector3::Lerp(upperBody_, mudPosition_ + offset_(0.0f, 1.0f, 0.0f), 0.36f + 0.20f * repel_);
        fov_ = std::lerp(46.0f, 56.0f, lunge_);
        roll_ = std::lerp(-4.0f, 1.0f, repel_);
    }
    else if (time_ < TRUE_END_BEAM)
    {
        eye_ = upperBody_ + offset_(std::lerp(1.95f, 0.65f, charge_), std::lerp(-0.15f, -0.56f, charge_),
            std::lerp(-3.0f, -2.65f, charge_));
        focus_ = Vector3::Lerp(upperBody_, Vector3{bladeEnd_}, 0.24f * charge_);
        fov_ = std::lerp(38.0f, 49.0f, charge_);
        roll_ = std::lerp(2.5f, -1.0f, charge_);
    }
    else
    {
        const float widen_ = ShotProgress(time_, TRUE_END_BEAM, TRUE_END_PILLAR + 0.62f);
        const float settle_ = ShotProgress(time_, TRUE_END_PORTRAIT, TRUE_END_FINISH - 0.16f);
        const Vector3 face_ = headPosition_ + offset_(0.0f, 0.10f, -0.035f);
        eye_ = playerPosition_ + offset_(std::lerp(4.6f, 8.0f, widen_), std::lerp(1.3f, 2.2f, widen_),
            std::lerp(-1.4f, -4.0f, widen_));
        focus_ = Vector3::Lerp(upperBody_, impact_ + offset_(0.0f, 3.2f, 0.0f), 0.38f + 0.32f * widen_);
        eye_ = Vector3::Lerp(eye_, face_ + offset_(0.32f, 0.02f, -1.24f), settle_);
        focus_ = Vector3::Lerp(focus_, face_, settle_);
        fov_ = std::lerp(std::lerp(49.0f, 60.0f, widen_), 27.0f, settle_);
        roll_ = std::lerp(-2.5f, 0.0f, settle_);
    }

    const float shake_ = 0.055f * ImpactPulse(time_, TRUE_END_HAND_LIGHT, 0.24f) +
        0.19f * ImpactPulse(time_, TRUE_END_SWEEP_HIT, 0.38f) +
        0.16f * ImpactPulse(time_, TRUE_END_BEAM, 0.23f) +
        0.23f * ImpactPulse(time_, TRUE_END_PILLAR, 0.64f);
    SetEndingCamera(eye_, focus_, fov_, roll_, time_, shake_);
}

void BossFinishDirector::SetEndingCamera(Vector3 eye_, Vector3 focus_, float fov_, float roll_, float time_, float shake_)
{
    using namespace DirectX;
    const auto right_ = XMVector3Normalize(XMVector3Cross(XMVectorSet(0, 1, 0, 0), XMLoadFloat3(&focus_) - XMLoadFloat3(&eye_)));
    const float horizontal_ = std::sin(time_ * 58.0f);
    const auto shakeOffset_ = right_ * (horizontal_ * shake_) +
        XMVectorSet(0, std::cos(time_ * 73.0f) * shake_ * 0.60f, 0, 0);
    XMStoreFloat3(&eye_, XMLoadFloat3(&eye_) + shakeOffset_);
    XMStoreFloat3(&focus_, XMLoadFloat3(&focus_) + shakeOffset_ * 0.20f);
    const auto forward_ = XMVector3Normalize(XMLoadFloat3(&focus_) - XMLoadFloat3(&eye_));
    const auto rotation_ = XMQuaternionRotationAxis(forward_, XMConvertToRadians(roll_ + 8.0f * shake_ * horizontal_));
    XMFLOAT3 up_{};
    XMStoreFloat3(&up_, XMVector3Rotate(XMVectorSet(0, 1, 0, 0), rotation_));
    camera->SetLookAt(eye_, focus_, up_);

    const auto& graphics_ = GraphicsDeviceManager::Instance();
    const float aspect_ = static_cast<float>((std::max)(graphics_.GetRenderWidth(), 1)) /
        (std::max)(graphics_.GetRenderHeight(), 1);
    fov_ -= shake_ * 12.0f;
    if (aspect_ < GetAspect())
    {
        fov_ = XMConvertToDegrees(2.0f * std::atan(std::tan(XMConvertToRadians(fov_) * 0.5f) * GetAspect() / aspect_));
    }
    camera->SetPerspectiveFov(fov_, aspect_, 0.05f, 300.0f);
}

bool BossFinishDirector::IsBadEndingImpactShot() const
{
    return badEndingTime >= BAD_END_PIERCE && badEndingTime < BAD_END_PIERCE + 0.085f;
}

void BossFinishDirector::BeginBadEnding()
{
    BS_PROFILE_SCOPE("BossFinishDirector::BeginBadEnding");
    badEndingPosition = player.object ? player.object->transform->GetWorldPosition() : GetStageAnchor();
    badEndingFallPosition = badEndingPosition;
    badEndingCameraEye = camera->GetEye();
    badEndingCameraFocus = camera->GetFocus();
    badEndingPierced.constrainHorizontal = false;
    PrepareActor(badEndingCounter, player.object, {}, "Combo_Attack_01_01_Seq", false);
    PrepareActor(badEndingKneel, player.object, {}, "Knock_Down_Combat_Start_Seq", false);
    PrepareActor(badEndingPierced, player.object, {}, "Execution_Target_01_Seq", false);
    PrepareActor(badEndingRise, player.object, {}, "Get_Up_Combat_Seq", false);
    if (const auto model_ = player.object ? player.object->GetComponent<GltfModelCommponennt>() : nullptr)
    {
        previousSilhouette = model_->HasRuntimeSilhouette();
        previousSilhouetteColor = model_->GetRuntimeSilhouetteColor();
    }

    effects.clear();
    badEndingMud = MakeObject("BadEndingMudPool");
    badEndingParticles = MakeObject("BadEndingMudParticles");
    badEndingSpear = MakeObject("BadEndingSpear");
    effects = {badEndingMud, badEndingParticles, badEndingSpear};
    for (const auto& effect_ : effects)
    {
        effect_->SetManager(&manager);
    }
    const auto mud_ = badEndingMud->AddComponent<MeshVFXComponent>();
    const auto particles_ = badEndingParticles->AddComponent<ParticleComponent>();
    const auto spear_ = badEndingSpear->AddComponent<MeshVFXComponent>();
    try
    {
        if (mud_->LoadAsset("Data/VFX/TransitionMudPool.json"))
        {
            for (auto& part_ : mud_->GetAsset().parts)
            {
                part_.renderEnabled = true;
                part_.animateScale = false;
                part_.animateAlpha = false;
                part_.position.x *= 0.48f;
                part_.position.z *= 0.48f;
            }
            mud_->Play();
        }
        if (particles_->LoadAsset("Data/Particle/TransitionMudVortex.json"))
        {
            for (auto& track_ : particles_->GetTracks())
            {
                track_.settings.shapeSize = {4.0f, 2.8f, 4.0f, 0.0f};
                track_.settings.emissionSize = {0.55f, 0.28f};
                track_.settings.orbitVelocity *= 0.4f;
            }
            particles_->Play();
        }
    }
    catch (const std::exception& error_)
    {
        BS_LOG_WARNING(LOG_CATEGORY::GAME, std::string("Bad ending mud effect: ") + error_.what());
    }
    MeshVFXAsset spearAsset_;
    auto& spearPart_ = spearAsset_.parts.emplace_back();
    spearPart_.modelPath = "__InternalMeshCone__";
    spearPart_.meshIndex = 0;
    spearPart_.renderEnabled = true;
    spearPart_.shapeCylinderRadius = 0.19f;
    spearPart_.internalMeshSettings.coneTipRadiusRate = 0.0f;
    spearPart_.internalMeshSettings.coneLength = 1.0f;
    spearPart_.internalMeshSettings.segments = 6;
    spearPart_.useMaterialBaseTexture = false;
    spearPart_.useMaterialNormalTexture = false;
    spearPart_.useFresnel = true;
    spearPart_.fresnelStrength = 0.48f;
    spearPart_.useFresnelNoise = true;
    spearPart_.fresnelNoiseStrength = 0.45f;
    spearPart_.noiseTexturePath = BAD_END_EQUIPMENT_TEXTURE;
    spearPart_.noiseUVSpeed = {0.0f, 0.0f};
    spearPart_.emissiveStrength = 0.0f;
    spearPart_.emissivePulseAmount = 0.0f;
    spear_->SetAsset(spearAsset_);
    spear_->Play();
    if (const auto death_ = manager.GetPlayerDeathDirector(); death_ && death_->GetFlashbackTexture())
    {
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view_ = death_->GetFlashbackTexture();
        Microsoft::WRL::ComPtr<ID3D11Resource> resource_;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture_;
        view_->GetResource(resource_.GetAddressOf());
        if (SUCCEEDED(resource_.As(&texture_)))
        {
            D3D11_TEXTURE2D_DESC desc_{};
            texture_->GetDesc(&desc_);
            flashbackSprite = MakeObject("BadEndingFlashback");
            const auto sprite_ = flashbackSprite->AddComponent<SpriteRenderer>();
            sprite_->Deserialize({{"SdrTexture", true}, {"LinearSampling", true}});
            sprite_->SetPivot(PosOfsset::TOP_L);
            sprite_->SetTextureResource(view_, desc_);
            // 描画専用SpriteはSceneに所属しないため、Managerを参照する通常のStartを通さない。
            const auto blur_ = flashbackSprite->AddComponent<UIVisualEffectComponent>(false);
            blur_->Deserialize({{"Settings", {{"BlurRadius", 7.0f}}}});
            blur_->SetStarted(true);
            blur_->SetProgress(1.0f);
        }
    }

    badEndingGlass = std::make_unique<GlassTransition>();
    TransitionParameters glass_;
    glass_.type = TransitionType::GLASS;
    glass_.easing = TransitionEasing::EASE_OUT_CUBIC;
    glass_.durationOut = BAD_END_COVERED - BAD_END_WIPE;
    glass_.color = {0.014f, 0.003f, 0.008f, 1.0f};
    glass_.edgeColor = {1.0f, 0.30f, 0.39f, 0.85f};
    glass_.edgeWidth = 0.010f;
    glass_.glowIntensity = 0.85f;
    glass_.noiseSpeed = 1.35f;
    glass_.panelCount = 5;
    glass_.effectSettings = {{"ShardRotation", 1.15f}};
    badEndingGlass->SetParameters(glass_);
    if (!badEndingGlass->Initialize())
    {
        badEndingGlass.reset();
    }
}

void BossFinishDirector::ApplyBadEndingMaterials()
{
    BS_PROFILE_SCOPE("BossFinishDirector::ApplyBadEndingMaterials");
    const auto bossModel_ = boss.object ? boss.object->GetComponent<GltfModelCommponennt>() : nullptr;
    const auto bossMaterials_ = boss.object ? boss.object->GetComponent<ModelMaterialComponent>() : nullptr;
    if (bossMaterials_ && bossMaterials_->IsEnabled())
    {
        bossMaterials_->UpdateLoadState();
    }
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> hair_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mud_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> matte_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> flatNormal_;
    auto* device_ = GraphicsDeviceManager::Instance().GetDevice();
    CPUResourceLoader::LoadTextureFromFile(device_, WindowsDialogHelper::Utf8ToPath(BAD_END_HAIR_TEXTURE).c_str(), hair_.GetAddressOf());
    CPUResourceLoader::LoadTextureFromFile(device_, WindowsDialogHelper::Utf8ToPath(BAD_END_EQUIPMENT_TEXTURE).c_str(), mud_.GetAddressOf());
    CPUResourceLoader::CreateDummyTexture(device_, 0xFF00FF00, matte_.GetAddressOf());
    CPUResourceLoader::CreateDummyTexture(device_, 0xFFFF8080, flatNormal_.GetAddressOf());
    using Slot = ModelMaterial::TextureSlot;
    for (const auto& object_ : {player.object, manager.Find(settings["Player"].value("WeaponObject", std::string("Weapon"))), manager.Find("Shield")})
    {
        const auto model_ = object_ ? object_->GetComponent<GltfModelCommponennt>() : nullptr;
        if (!model_ || !model_->IsModelReady())
        {
            continue;
        }
        const auto& meshes_ = model_->GetMeshes();
        if (std::any_of(meshes_.begin(), meshes_.end(), [](const auto& mesh_)
        {
            return !mesh_.material;
        }))
        {
            continue;
        }
        for (const auto& component_ : {std::static_pointer_cast<Component>(object_->GetComponent<ModelMaterialComponent>()),
            std::static_pointer_cast<Component>(object_->GetComponent<MaterialOverlayComponent>())})
        {
            if (!component_)
            {
                continue;
            }
            badEndingMaterialStates.emplace_back(component_, component_->IsRuntimeSuppressed());
            component_->SetRuntimeSuppressed(true);
        }
        const bool equipment_ = object_ != player.object;
        std::vector<GltfModelCommponennt::Material> materials_;
        materials_.reserve(meshes_.size());
        for (std::size_t index_ = 0; index_ < meshes_.size(); ++index_)
        {
            const auto& mesh_ = meshes_[index_];
            auto& material_ = materials_.emplace_back(*mesh_.material);
            const bool isHair_ = material_.name.find("Hair") != std::string::npos;
            if (isHair_ && !equipment_)
            {
                if (hair_)
                {
                    material_.baseMap = hair_;
                }
            }
            else if (!equipment_ && bossModel_ && index_ < bossModel_->GetMeshes().size() &&
                bossModel_->GetMeshes()[index_].material &&
                bossModel_->GetMeshes()[index_].material->name == material_.name)
            {
                const auto& bossMesh_ = bossModel_->GetMeshes()[index_];
                material_ = *bossMesh_.material;
                // 敵のメッシュ単位の上書きも引き継ぎ、同じ材質の別パーツを一律の色にしない。
                if (bossMaterials_ && bossMaterials_->IsEnabled())
                {
                    const auto copy_ = [&](Slot slot_, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& map_)
                    {
                        map_ = bossMaterials_->ResolveTexture(bossMesh_.materialIndex, slot_, map_.Get(), static_cast<int>(index_));
                    };
                    copy_(Slot::BASE_COLOR, material_.baseMap);
                    copy_(Slot::NORMAL, material_.normalMap);
                    copy_(Slot::EMISSIVE, material_.emissiveMap);
                    copy_(Slot::OCCLUSION, material_.occlusionMap);
                    copy_(Slot::METALNESS_ROUGHNESS, material_.metalnessRoughnessMap);
                }
            }
            else
            {
                if (mud_)
                {
                    material_.baseMap = mud_;
                }
                material_.baseColor = {0.16f, 0.045f, 0.055f, 1.0f};
                material_.metalness = 0.0f;
                material_.roughness = 1.0f;
                material_.occlusionStrength = 0.0f;
                material_.specularStrength = 0.0f;
                material_.rimStrength = 0.0f;
                material_.emissiveColor = {0,0,0};
                if (matte_)
                {
                    material_.metalnessRoughnessMap = matte_;
                }
                if (flatNormal_)
                {
                    material_.normalMap = flatNormal_;
                }
                material_.emissiveMap.Reset();
                material_.occlusionMap.Reset();
                material_.alphaMode = GltfModelCommponennt::AlphaMode::Opaque;
            }
        }
        model_->SetPresentationMaterials(std::move(materials_));
    }
}

void BossFinishDirector::AdvanceBadEnding(float time_)
{
    if (badEndingTime < 0.0f)
    {
        UpdateBadEnding(-1.0f, 0.0f);
    }
    // 低フレームレートでも、槍が刺さる区間のポーズと接触点を評価する。
    while (badEndingTime < time_)
    {
        UpdateBadEnding(badEndingTime, (std::min)(badEndingTime + FIXED_STEP, time_));
    }
}

void BossFinishDirector::UpdateBadEnding(float previousTime_, float time_)
{
    BS_PROFILE_SCOPE("BossFinishDirector::UpdateBadEnding");
    badEndingTime = time_;
    const float yaw_ = settings["Player"].value("Yaw", 0.0f);
    const float radians_ = DirectX::XMConvertToRadians(yaw_);
    const auto offset_ = [&](float x_, float y_, float z_)
    {
        return Vector3{x_ * std::cos(radians_) + z_ * std::sin(radians_), y_,
            z_ * std::cos(radians_) - x_ * std::sin(radians_)};
    };
    const auto crossed_ = [&](float cue_)
    {
        return previousTime_ < cue_ && time_ >= cue_;
    };
    const Vector3 playerPosition_{badEndingPosition};
    const float turn_ = ShotProgress(time_, BAD_END_COUNTER - 0.2f, BAD_END_FLASHBACK);
    const float hitAge_ = time_ - BAD_END_PIERCE;
    const float afterHold_ = (std::max)(0.0f, hitAge_ - BAD_END_IMPACT_HOLD);
    const bool impactShot_ = IsBadEndingImpactShot();
    const bool emerging_ = time_ >= BAD_END_RISE;
    const float emerge_ = ShotProgress(time_, BAD_END_RISE, BAD_END_RISE + 1.35f);
    if (crossed_(BAD_END_RISE))
    {
        ApplyBadEndingMaterials();
    }

    if (time_ < BAD_END_FALL || emerging_)
    {
        auto position_ = emerging_ ? Vector3{badEndingFallPosition} : playerPosition_;
        if (emerging_)
        {
            position_.y -= 0.57f * (1.0f - emerge_);
        }
        else
        {
            const float sink_ = ShotProgress(time_, BAD_END_THRUST_START, BAD_END_PIERCE);
            position_.y -= 0.45f * sink_ * (1.0f - ShotProgress(afterHold_, 0.0f, 0.48f));
        }
        PlaceActor(player.object, position_, yaw_ + 180.0f * turn_);
        if (emerging_)
        {
            SampleActor(badEndingRise, (time_ - BAD_END_RISE) * 1.18f);
        }
        else if (hitAge_ >= 0.0f)
        {
            SampleActor(badEndingPierced, BAD_END_PIERCE_CUE + afterHold_);
        }
        else if (time_ >= BAD_END_KNEEL)
        {
            SampleActor(badEndingKneel, time_ - BAD_END_KNEEL);
        }
        else if (time_ >= BAD_END_COUNTER + 0.12f)
        {
            SampleActor(badEndingCounter, time_ - BAD_END_COUNTER - 0.12f);
        }
        else if (time_ >= BAD_END_COUNTER)
        {
            BlendExecutionPose(playerExecution, badEndingCounter,
                GetCaptureTime() - settings["ExecutionStart"].get<float>(),
                (time_ - BAD_END_COUNTER) / 0.12f);
        }
    }
    if (time_ >= BAD_END_FALL - 0.45f && (time_ < BAD_END_FALL || crossed_(BAD_END_FALL)))
    {
        if (crossed_(BAD_END_FALL))
        {
            SampleActor(badEndingPierced, BAD_END_PIERCE_CUE + afterHold_);
        }
        auto fallPosition_ = badEndingPosition;
        ReadNodePosition(player.object, "pelvis", fallPosition_);
        badEndingFallPosition = {fallPosition_.x, badEndingPosition.y, fallPosition_.z};
    }
    if (const auto model_ = player.object ? player.object->GetComponent<GltfModelCommponennt>() : nullptr)
    {
        model_->SetRuntimeSilhouette(impactShot_ || previousSilhouette, impactShot_ ? BAD_END_RED : previousSilhouetteColor);
        if (time_ < BAD_END_COUNTER)
        {
            model_->UpdateTransform();
        }
    }

    for (const auto& name_ : {settings["Player"].value("WeaponObject", std::string("Weapon")), std::string("Shield")})
    {
        const auto object_ = manager.Find(name_);
        if (!object_)
        {
            continue;
        }
        if (const auto attachment_ = object_->GetComponent<ModelAttachmentComponent>())
        {
            attachment_->SyncAttachment();
        }
        object_->transform->UpdateWorldMatrix();
        if (const auto model_ = object_->GetComponent<GltfModelCommponennt>())
        {
            model_->UpdateTransform();
        }
    }

    if (hitAge_ < BAD_END_IMPACT_HOLD)
    {
        badEndingSpearContact = playerPosition_ + offset_(0.0f, 0.85f, 0.0f);
        ReadNodePosition(player.object, "spine_02", badEndingSpearContact);
    }
    const Vector3 spearBase_ = playerPosition_ + offset_(-0.72f, -0.10f, -1.65f);
    const Vector3 contact_ = Vector3{badEndingSpearContact};
    Vector3 thrustDirection_ = contact_ - spearBase_;
    thrustDirection_.Normalize();
    const float thrust_ = ShotProgress(time_, BAD_END_THRUST_START, BAD_END_PIERCE);
    const float through_ = ShotProgress(time_, BAD_END_PIERCE, BAD_END_PIERCE + 0.06f);
    const float retract_ = ShotProgress(afterHold_, 0.16f, 0.65f);
    auto* spear_ = badEndingSpear->GetComponent<MeshVFXComponent>().get();
    const bool spearVisible_ = thrust_ > 0.0f && retract_ < 1.0f;
    spear_->SetRuntimeSuppressed(!spearVisible_);
    if (spearVisible_)
    {
        // 円錐は先端が原点、根元が +Z 側。先端と長さだけを変えて根元を地面に固定する。
        const Vector3 spearTip_ = Vector3::Lerp(spearBase_, contact_ + thrustDirection_ * (1.12f * through_),
            thrust_ * (1.0f - retract_));
        const Vector3 towardBase_ = spearBase_ - spearTip_;
        const float pitch_ = -std::atan2(towardBase_.y, std::hypot(towardBase_.x, towardBase_.z));
        PlaceActor(badEndingSpear, spearTip_, DirectX::XMConvertToDegrees(std::atan2(towardBase_.x, towardBase_.z)));
        badEndingSpear->transform->localRotation.x = DirectX::XMConvertToDegrees(pitch_);
        badEndingSpear->transform->MarkDirty();
        badEndingSpear->transform->UpdateWorldMatrix();
        const auto spearColor_ = impactShot_ ? BAD_END_RED : DirectX::XMFLOAT4{0.20f, 0.055f, 0.078f, 1.0f};
        spear_->SetRuntimeAppearance({1.0f, 0.72f, towardBase_.Length()}, spearColor_, spearColor_);
    }

    const float approach_ = ShotProgress(time_, 0.0f, BAD_END_THRUST_START);
    const float gather_ = ShotProgress(time_, BAD_END_FALL - 0.15f, BAD_END_RISE + 0.25f);
    const Vector3 mudPosition_ = Vector3::Lerp(spearBase_ + offset_(0.0f, 0.10f, -4.65f * (1.0f - approach_)),
        Vector3{badEndingFallPosition}, gather_);
    PlaceActor(badEndingMud, mudPosition_, yaw_);
    PlaceActor(badEndingParticles, mudPosition_ + offset_(0, 0.45f * emerge_, 0), yaw_);
    const auto mud_ = badEndingMud->GetComponent<MeshVFXComponent>();
    const auto particles_ = badEndingParticles->GetComponent<ParticleComponent>();
    const float step_ = (std::max)(0.0f, time_ - (std::max)(previousTime_, 0.0f));
    const float spread_ = 0.15f + 0.24f * approach_ + 0.34f * gather_;
    mud_->SetRuntimeAppearance({spread_, 1.0f, spread_}, BAD_END_MUD, BAD_END_MUD);
    mud_->SetRuntimeSuppressed(impactShot_);
    particles_->SetRuntimeSuppressed(impactShot_);
    particles_->SetEmissionScale(0.85f + 0.7f * approach_ + 1.3f * gather_);
    for (auto& track_ : particles_->GetTracks())
    {
        track_.settings.targetPosition = {mudPosition_.x, mudPosition_.y + 0.45f + emerge_ * 0.85f, mudPosition_.z, 1.0f};
    }
    if (step_ > 0.0f && !impactShot_)
    {
        mud_->Update(step_);
        particles_->Update(step_);
    }

    if (!preview)
    {
        auto& audio_ = AudioManager::Instance();
        if (crossed_(0.1f))
        {
            audio_.PlaySEByPath("Data/Audio/SE/Sonniss/Shadow/TentacleMotion.wav");
        }
        if (crossed_(BAD_END_FLASHBACK))
        {
            audio_.PlaySEByPath("Data/Audio/SE/Sonniss/Presentation/RattlingDowner.wav");
        }
        if (crossed_(BAD_END_PIERCE))
        {
            audio_.PlaySEByPath("Data/Audio/SE/Sonniss/Mud/SplatHit.wav");
        }
        if (crossed_(BAD_END_FALL + 0.22f))
        {
            audio_.PlaySEByPath("Data/Audio/SE/Sonniss/Mud/LiquidHit.wav");
        }
        if (crossed_(BAD_END_RISE + 0.28f))
        {
            audio_.PlaySEByPath("Data/Audio/SE/Sonniss/Boss/MonsterGrowl.wav");
        }
    }

    if (preview && previewCameraOverride)
    {
        return;
    }
    const Vector3 fallPosition_{badEndingFallPosition};
    auto headPosition_ = playerPosition_ + offset_(0.0f, emerging_ ? std::lerp(0.45f, 1.52f, emerge_) : 1.35f, 0.0f);
    ReadNodePosition(player.object, "head", headPosition_);
    const Vector3 upperBody_ = headPosition_ - Vector3{0.0f, 0.18f, 0.0f};
    Vector3 eye_;
    Vector3 focus_;
    float fov_;
    float roll_;
    if (time_ < BAD_END_KNEEL)
    {
        const float push_ = ShotProgress(time_, 0.0f, BAD_END_FLASHBACK);
        eye_ = Vector3::Lerp(Vector3{badEndingCameraEye}, upperBody_ + offset_(2.8f, -0.12f, -1.35f), push_);
        focus_ = Vector3::Lerp(Vector3{badEndingCameraFocus}, upperBody_, push_);
        fov_ = std::lerp(43.0f, 36.0f, push_);
        roll_ = -2.5f * push_;
    }
    else if (time_ < BAD_END_FALL)
    {
        const float pull_ = ShotProgress(time_, BAD_END_KNEEL, BAD_END_THRUST_START);
        const float fall_ = ShotProgress(time_, BAD_END_PIERCE + BAD_END_IMPACT_HOLD, BAD_END_FALL);
        const Vector3 lowEye_ = playerPosition_ + offset_(std::lerp(3.0f, 3.4f, pull_), 0.76f, std::lerp(-1.75f, -2.65f, pull_));
        const Vector3 strikeFocus_ = Vector3::Lerp(upperBody_, (spearBase_ + contact_) * 0.5f, 0.4f * pull_);
        eye_ = Vector3::Lerp(lowEye_, fallPosition_ + offset_(2.55f, 0.46f, -1.95f), fall_);
        focus_ = Vector3::Lerp(strikeFocus_, fallPosition_ + offset_(0.0f, 0.43f, 0.0f), fall_);
        fov_ = std::lerp(std::lerp(37.0f, 49.0f, pull_), 41.0f, fall_);
        roll_ = std::lerp(-3.5f, 5.0f, fall_);
    }
    else if (!emerging_)
    {
        const float drift_ = ShotProgress(time_, BAD_END_FALL, BAD_END_RISE);
        eye_ = fallPosition_ + offset_(std::lerp(2.55f, 1.55f, drift_), std::lerp(0.46f, 0.31f, drift_),
            std::lerp(-1.95f, -2.8f, drift_));
        focus_ = fallPosition_ + offset_(0.0f, 0.43f, 0.0f);
        fov_ = std::lerp(41.0f, 36.0f, drift_);
        roll_ = std::lerp(5.0f, 1.5f, drift_);
    }
    else
    {
        const float reveal_ = ShotProgress(time_, BAD_END_RISE + 0.25f, BAD_END_WIPE);
        eye_ = fallPosition_ + offset_(std::lerp(-2.05f, -0.65f, reveal_), std::lerp(0.45f, 1.16f, emerge_),
            std::lerp(-3.1f, -2.55f, reveal_));
        focus_ = Vector3::Lerp(fallPosition_ + offset_(0.0f, 0.55f, 0.0f), upperBody_, emerge_);
        fov_ = std::lerp(43.0f, 32.0f, reveal_);
        roll_ = std::lerp(-5.0f, 0.0f, reveal_);
    }

    const float shake_ = 0.065f * ImpactPulse(time_, BAD_END_COUNTER, 0.28f) +
        0.28f * ImpactPulse(time_, BAD_END_PIERCE, 0.48f) +
        0.12f * ImpactPulse(time_, BAD_END_FALL + 0.22f, 0.46f) +
        0.09f * ImpactPulse(time_, BAD_END_RISE + 0.28f, 0.58f);
    SetEndingCamera(eye_, focus_, fov_, roll_, time_, shake_);
}

void BossFinishDirector::Evaluate(float time_, float deltaTime_)
{
    BS_PROFILE_SCOPE("BossFinishDirector::Evaluate");
    (void)deltaTime_;
    const float captureTime_ = GetCaptureTime();
    const float sampleTime_ = (std::min)(time_, captureTime_);
    const float impactTime_ = GetImpactTime();
    const float impactAge_ = (std::max)(sampleTime_ - impactTime_, 0.0f);
    impactConfirmed = (impactReady || finishAuthorized || preview) && sampleTime_ >= impactTime_;
    const float executionStart_ = settings["ExecutionStart"].get<float>();
    const float executionTime_ = (std::max)(sampleTime_ - executionStart_, 0.0f);
    const auto anchor_ = GetStageAnchor();
    const auto& playerConfig_ = settings["Player"];
    const auto& bossConfig_ = settings["Boss"];
    const auto start_ = ReadPosition(playerConfig_["Position"]);
    const auto bossPosition_ = ReadPosition(bossConfig_["Position"]);
    const float yaw_ = DirectX::XMConvertToRadians(playerConfig_.value("Yaw", 0.0f));
    const float distance_ = std::clamp(bossConfig_.value("ExecutionDistance", 1.8f), 0.8f, 4.0f);
    // 停止位置と拘束位置を同じ敵座標から求め、攻撃後の再配置を不要にする。
    const DirectX::XMFLOAT3 end_{bossPosition_.x - std::sin(yaw_)*distance_,
        bossPosition_.y, bossPosition_.z - std::cos(yaw_)*distance_};
    const float runDuration_ = playerConfig_.value("PoseTime", 2.0666668f);
    const float travel_ = std::clamp(sampleTime_ / (std::max)(playerConfig_.value("ApproachTime", 0.55f), 0.01f), 0.0f, 1.0f);
    const DirectX::XMFLOAT3 playerPosition_{start_.x + (end_.x-start_.x)*travel_,
        start_.y + (end_.y-start_.y)*travel_, start_.z + (end_.z-start_.z)*travel_};
    PlaceActor(player.object, AddPosition(anchor_, playerPosition_), playerConfig_.value("Yaw", 0.0f));
    PlaceActor(boss.object, AddPosition(anchor_, bossPosition_), playerConfig_.value("Yaw", 0.0f) + 180.0f);
    const float blendDuration_ = settings["AlignDuration"].get<float>();
    const float blendStart_ = executionStart_ - blendDuration_;
    if (sampleTime_ >= executionStart_)
    {
        SampleActor(playerExecution, executionTime_);
        SampleActor(bossReaction, executionTime_);
    }
    else if (sampleTime_ >= blendStart_)
    {
        const float blend_ = (sampleTime_ - blendStart_) / blendDuration_;
        BlendExecutionPose(player, playerExecution, (std::min)(blendStart_, runDuration_), blend_);
        BlendExecutionPose(boss, bossReaction, blendStart_, blend_);
    }
    else
    {
        SampleActor(player, (std::min)(sampleTime_, runDuration_));
        SampleActor(boss, sampleTime_);
    }
    // 叩きつけ後に間を置き、敵本体と装備を同じ進行度で消滅させる。
    const float dissolveStart_ = impactTime_ + settings["DissolveDelay"].get<float>();
    const float dissolveProgress_ = std::clamp((sampleTime_ - dissolveStart_) / settings["DissolveDuration"].get<float>(), 0.0f, 1.0f);
    for (const auto& name_ : { bossConfig_["Object"].get<std::string>(), std::string("HumanBossWeapon"), std::string("HumanBossShield") })
    {
        const auto object_ = manager.Find(name_);
        if (!object_)
        {
            continue;
        }
        if (const auto dissolve_ = object_->GetComponent<ModelDissolveComponent>())
        {
            dissolve_->SetPresentationProgress(dissolveProgress_);
            dissolve_->UpdateLoadState();
        }
        if (const auto model_ = object_->GetComponent<GltfModelCommponennt>())
        {
            model_->SetRuntimeVisible(dissolveProgress_ < 1.0f);
        }
    }
    for (auto& saved_ : savedActors)
    {
        if (!ShouldRenderActor(saved_.object))
        {
            continue;
        }
        if (const auto attachment_ = saved_.object->GetComponent<ModelAttachmentComponent>())
        {
            attachment_->SyncAttachment();
        }
        saved_.object->transform->UpdateWorldMatrix();
        if (const auto model_ = saved_.object->GetComponent<GltfModelCommponennt>())
        {
            model_->UpdateTransform();
        }
    }
    // フレームレートに依存しない固定刻みで、再撮影時も同じシミュレーション時刻へ到達する。
    while (effectsTime + FIXED_STEP <= sampleTime_)
    {
        AdvanceEffects(effectsTime, effectsTime + FIXED_STEP);
        effectsTime += FIXED_STEP;
    }
    if (preview && previewCameraOverride)
    {
        return;
    }
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
            if (key_.at("Time").get<float>() >= cameraTime_)
            {
                break;
            }
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
        {
            for (std::size_t index_ = 0; index_ < 3; ++index_)
            {
                config_[property_][index_] = (*first_)[property_][index_].get<float>() +
                    ((*second_)[property_][index_].get<float>() - (*first_)[property_][index_].get<float>()) * rate_;
            }
        }
        for (const char* property_ : { "Fov", "Roll" })
        {
            config_[property_] = first_->at(property_).get<float>() + (second_->at(property_).get<float>() - first_->at(property_).get<float>()) * rate_;
        }
    }
    auto eye_ = AddPosition(anchor_, ReadPosition(config_["Position"]));
    auto focus_ = AddPosition(anchor_, ReadPosition(config_["Focus"]));
    // 叩きつけに合わせてカメラを揺らし、消滅が始まるまでに減衰させる。
    const float decay_ = impactConfirmed ? std::exp(-impactAge_ / (std::max)(config_.value("ImpactShakeDuration", 0.22f), 0.01f)) : 0.0f;
    const float shake_ = config_.value("ImpactShake", 0.38f) * decay_;
    const float phase_ = sampleTime_ * 71.0f;
    const DirectX::XMFLOAT3 offset_{std::sin(phase_) * shake_, std::cos(sampleTime_ * 93.0f) * shake_ * 0.7f,
        std::sin(sampleTime_ * 57.0f) * shake_ * 0.4f};
    eye_ = AddPosition(eye_, offset_);
    focus_ = AddPosition(focus_, offset_);
    using namespace DirectX;
    const float roll_ = XMConvertToRadians(config_.value("Roll", 0.0f) + std::sin(phase_) * config_.value("ImpactRoll", 3.0f) * decay_);
    const auto forward_ = XMVector3Normalize(XMLoadFloat3(&focus_) - XMLoadFloat3(&eye_));
    const auto upVector_ = XMVector3Rotate(XMVectorSet(0,1,0,0), XMQuaternionRotationAxis(forward_, roll_));
    XMFLOAT3 up_;
    XMStoreFloat3(&up_, upVector_);
    camera->SetLookAt(eye_, focus_, up_);
    auto& graphics_ = GraphicsDeviceManager::Instance();
    const float aspect_ = static_cast<float>((std::max)(graphics_.GetRenderWidth(), 1)) / (std::max)(graphics_.GetRenderHeight(), 1);
    float fov_ = (std::max)(15.0f, config_.value("Fov", 43.0f) - config_.value("ImpactFovKick", 5.0f) * decay_);
    if (aspect_ < GetAspect())
    {
        fov_ = XMConvertToDegrees(2.0f * std::atan(std::tan(XMConvertToRadians(fov_) * 0.5f) * GetAspect() / aspect_));
    }
    camera->SetPerspectiveFov(fov_, aspect_, 0.05f, 300.0f);
}

bool BossFinishDirector::Update(float rawDeltaTime_)
{
    BS_PROFILE_SCOPE("BossFinishDirector::Update");
    const auto mode_ = manager.GetGameModeManager();
    if (!active && mode_ && mode_->GetCurrentGameState() == GameProgressState::BOSS_DEFEATED)
    {
        Begin(false);
    }
    if (!active)
    {
        return false;
    }
    const float delta_ = std::clamp(rawDeltaTime_, 0.0f, 0.1f);
    const float previous_ = elapsed;
    if (!paused)
    {
        elapsed += delta_ * (preview ? previewPlaybackRate : 1.0f);
        if (preview && IsBadEnding())
        {
            elapsed = (std::min)(elapsed, GetPreviewEndTime(ending));
        }
    }
    if (!preview)
    {
        AudioManager::Instance().ApplyBgmVolumes(previousBgmVolumes,1.0f-std::clamp(elapsed/0.7f,0.0f,1.0f));
    }
    const float branchTime_ = GetEndingBranchTime();
    if (IsBadEnding() && elapsed >= branchTime_)
    {
        if (badEndingTime < 0.0f)
        {
            Evaluate(GetCaptureTime(), delta_);
            BeginBadEnding();
        }
        AdvanceBadEnding((std::min)(elapsed - branchTime_, BAD_END_FINISH));
    }
    else if (elapsed >= branchTime_)
    {
        if (trueEndingTime < 0.0f)
        {
            Evaluate(GetCaptureTime(), 0.0f);
            BeginTrueEnding();
        }
        AdvanceTrueEnding(elapsed >= GetClearTime() ? TRUE_END_FINISH : elapsed - branchTime_);
    }
    else
    {
        // 最初の短い停止は演出時計だけを進め、通常TimeScaleを変更せずに表現する。
        Evaluate((std::max)(elapsed - settings["HitStop"].get<float>(), 0.0f), delta_);
    }
    const float resultAt_ = GetResultStartTime(ending);
    if (elapsed >= resultAt_ && previous_ < resultAt_ && !IsBadEnding())
    {
        RequestCapture();
    }
    const float hold_ = elapsed - resultAt_;
    if (!preview && completionCommitted && hold_ >= settings["PromptDelay"].get<float>())
    {
        const auto action_ = settings.value("ConfirmAction", std::string("UI.MenuConfirm"));
        const bool pressed_ = KeyConfig::Instance().ActionPressed(action_);
        if (!pressed_)
        {
            inputArmed = true;
        }
        if (inputArmed && KeyConfig::Instance().ActionPressedIn(action_) && !returning)
        {
            returning = true;
        }
    }
    if (returning)
    {
        returnElapsed += delta_;
        if (returnElapsed >= settings["ReturnFade"].get<float>())
        {
            ReturnToTitle();
        }
    }
    return true;
}

void BossFinishDirector::CompleteFinish()
{
    if (preview || !finishAuthorized || completionCommitted)
    {
        return;
    }
    completionCommitted = true;
    if (boss.object)
    {
        if (const auto health_ = boss.object->GetComponent<HealthComponent>())
        {
            health_->ConfirmPresentationDeath();
        }
    }
    if (!IsBadEnding())
    {
        AudioManager::Instance().PlaySE(SE_GAMECLEAR);
    }
    if (auto mode_ = manager.GetGameModeManager())
    {
        mode_->NotifySequenceFinished();
    }
    BS_LOG_INFO(LOG_CATEGORY::GAME, IsBadEnding() ? "Boss finish committed: bad ending" : "Boss finish committed: clear text and title confirmation enabled");
}

void BossFinishDirector::ReturnToTitle()
{
    if (titleRequested)
    {
        return;
    }
    titleRequested = true;
    manager.GetTimeManager()->ResetToDefaults(static_cast<uint32_t>(TarGetLayerType::All), true);
    AudioManager::Instance().StopBGM();
    ActionRegistry::Instance().Call("GoSceneTitle");
}

void BossFinishDirector::Seek(float time_)
{
    BS_PROFILE_SCOPE("BossFinishDirector::Seek");
    if (!active || !preview)
    {
        return;
    }
    Normalize();
    ResetBadEnding();
    ResetTrueEnding();
    PrepareActor(player,"Player");
    PrepareActor(boss,"Boss");
    PrepareExecution();
    PrepareSprites();
    effectsTime = 0.0f;
    CreateEffects();
    capturedView.Reset();
    capturedTexture.Reset();
    elapsed = std::clamp(time_, 0.0f, GetPreviewEndTime(ending) + (IsBadEnding() ? 0.0f : 4.0f));
    const float branchTime_ = GetEndingBranchTime();
    paused = true;
    showFixed = false;
    captureRequested = false;
    if (IsBadEnding() && elapsed >= branchTime_)
    {
        Evaluate(GetCaptureTime(), 0.0f);
        BeginBadEnding();
        AdvanceBadEnding((std::min)(elapsed - branchTime_, BAD_END_FINISH));
    }
    else if (!IsBadEnding() && elapsed >= branchTime_)
    {
        Evaluate(GetCaptureTime(), 0.0f);
        BeginTrueEnding();
        AdvanceTrueEnding(elapsed >= GetClearTime() ? TRUE_END_FINISH : elapsed - branchTime_);
        if (elapsed >= GetClearTime())
        {
            RequestCapture();
        }
    }
    else
    {
        Evaluate((std::max)(elapsed - settings["HitStop"].get<float>(), 0.0f), 0.0f);
    }
}

void BossFinishDirector::RequestCapture()
{
    if (!active || IsBadEnding())
    {
        return;
    }
    captureRequested = true;
    showFixed = false;
}

bool BossFinishDirector::ShouldRenderActor(const std::shared_ptr<GameObject>& object_) const
{
    if (!object_)
    {
        return false;
    }
    if (badEndingTime >= 0.0f)
    {
        if (IsBadEndingImpactShot())
        {
            return object_ == player.object;
        }
        if (object_->GetName() == settings.value("StageCenterObject", std::string("Stage")))
        {
            return true;
        }
        if (badEndingTime >= BAD_END_FALL && badEndingTime < BAD_END_RISE)
        {
            return false;
        }
        return object_ == player.object || object_->GetName() == "Shield" ||
            object_->GetName() == settings["Player"].value("WeaponObject", std::string("Weapon"));
    }
    if (object_->GetName() == settings.value("StageCenterObject", std::string("Stage")))
    {
        return true;
    }
    if (trueEndingTime >= 0.0f)
    {
        return object_ == player.object || object_->GetName() == "Shield" ||
            object_->GetName() == settings["Player"].value("WeaponObject", std::string("Weapon"));
    }
    if (object_ == player.object || object_ == boss.object)
    {
        return true;
    }
    for (const auto& name_ : settings.at("Equipment"))
    {
        if (object_->GetName() == name_.get<std::string>())
        {
            return true;
        }
    }
    return false;
}

void BossFinishDirector::UpdateResultPreview(float deltaTime_)
{
    if (!resultPreviewReady)
    {
        if (!Load())
        {
            ReloadResultRanks();
            Normalize();
        }
        ending = Ending::TRUE_END;
        resultPreviewValues = {240.0, 12, 8, 425.0};
        resultPreviewAge = settings["PromptDelay"].get<float>() - settings["HoldTime"].get<float>();
        PrepareSprites(&resultPreviewValues);
        resultPreviewReady = true;
    }
    resultPreviewAge += std::isfinite(deltaTime_) ? std::clamp(deltaTime_, 0.0f, 0.1f) : 0.0f;
}

void BossFinishDirector::DrawResultPreviewControls()
{
    if (!resultPreviewReady)
    {
        return;
    }

    ImGui::PushID("ResultPreview");
    ImGui::SetNextItemWidth(140.0f);
    int endingIndex_ = IsBadEnding() ? 1 : 0;
    bool rebuild_ = ImGui::Combo("##Ending", &endingIndex_, "TRUE END\0BAD END\0");
    ending = endingIndex_ == 0 ? Ending::TRUE_END : Ending::BAD_END;
    ImGui::SameLine();
    if (ImGui::Button("演出を再生"))
    {
        resultPreviewAge = 0.0f;
    }
    ImGui::SameLine();
    if (ImGui::Button("プリセット再読込"))
    {
        rebuild_ |= LoadPreset(PRESET_PATH) || ReloadResultRanks();
    }
    if (status.starts_with("Finish preset fallback:"))
    {
        ImGui::TextWrapped("%s", status.c_str());
    }
    if (ImGui::TreeNode("適用中のランク基準（S / A / B）"))
    {
        const auto& ranks_ = settings.at("ResultRanks");
        const auto show_ = [&](const char* label_, const char* key_)
        {
            const auto& thresholds_ = ranks_.at(key_);
            ImGui::Text("%s  %g / %g / %g", label_, thresholds_[0].get<double>(),
                thresholds_[1].get<double>(), thresholds_[2].get<double>());
        };
        show_("戦闘時間（秒）", "TimeSeconds");
        show_("ジャストアクション", "JustActions");
        show_("被ダメージ", "DamageTaken");
        ImGui::TextWrapped("%s", status.c_str());
        ImGui::TreePop();
    }

    bool valuesChanged_ = false;
    const int columns_ = ImGui::GetContentRegionAvail().x >= 540.0f ? 2 : 1;
    if (ImGui::BeginTable("Values", columns_, ImGuiTableFlags_SizingStretchSame))
    {
        constexpr double MIN_VALUE = 0.0;
        constexpr double MAX_TIME = 36000.0;
        constexpr double MAX_DAMAGE = 1000000.0;
        constexpr auto CLAMP_INPUT = ImGuiSliderFlags_AlwaysClamp;
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.46f);
        valuesChanged_ |= ImGui::DragScalar("戦闘時間（秒）", ImGuiDataType_Double,
            &resultPreviewValues.elapsedSeconds, 1.0f, &MIN_VALUE, &MAX_TIME, "%.2f", CLAMP_INPUT);
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.46f);
        valuesChanged_ |= ImGui::DragScalar("被ダメージ", ImGuiDataType_Double,
            &resultPreviewValues.damageTaken, 1.0f, &MIN_VALUE, &MAX_DAMAGE, "%.2f", CLAMP_INPUT);
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.46f);
        valuesChanged_ |= ImGui::DragInt("ジャスト回避", &resultPreviewValues.justDodges, 1.0f, 0, 999, "%d", CLAMP_INPUT);
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.46f);
        valuesChanged_ |= ImGui::DragInt("ジャストガード", &resultPreviewValues.justGuards, 1.0f, 0, 999, "%d", CLAMP_INPUT);
        ImGui::EndTable();
    }
    if (valuesChanged_)
    {
        if (!std::isfinite(resultPreviewValues.elapsedSeconds))
        {
            resultPreviewValues.elapsedSeconds = 0.0;
        }
        if (!std::isfinite(resultPreviewValues.damageTaken))
        {
            resultPreviewValues.damageTaken = 0.0;
        }
    }
    if (rebuild_)
    {
        PrepareSprites(&resultPreviewValues);
    }
    else if (valuesChanged_)
    {
        RefreshResultSprites(&resultPreviewValues, false);
    }
    ImGui::Separator();
    ImGui::PopID();
}

void BossFinishDirector::PrepareSprites(const GameModeManager::BattleResult* previewResult_)
{
    const auto makeSprite_ = [](const char* name_)
    {
        auto object_ = MakeObject(name_);
        const auto sprite_ = object_->AddComponent<SpriteRenderer>();
        sprite_->Deserialize({{"SdrTexture", true}, {"LinearSampling", true}});
        sprite_->SetPivot(PosOfsset::TOP_L);
        return object_;
    };
    imageSprite = makeSprite_("FinishImage");
    const auto imageEffect_ = imageSprite->AddComponent<UIVisualEffectComponent>(false);
    const nlohmann::json imageSettings_ = IsBadEnding()
        ? nlohmann::json{{"InkStrength", 1.0f}, {"GradientStrength", 0.18f},
            {"GradientTop", {1.0f, 0.76f, 0.81f, 1.0f}}, {"GradientBottom", {0.72f, 0.33f, 0.40f, 1.0f}}}
        : nlohmann::json{{"PaintStrength", 0.95f}, {"Sepia", 0.10f}};
    imageEffect_->Deserialize({{"Settings", imageSettings_}});
    imageEffect_->SetStarted(true);
    imageEffect_->SetProgress(1.0f);
    solidSprite = makeSprite_("FinishFade");
    clearSprite = makeSprite_("FinishClear");
    promptSprite = makeSprite_("FinishPrompt");
    resultLabelsSprite = makeSprite_("ResultLabels");
    resultValuesSprite = makeSprite_("ResultValues");
    resultRanksSprite = makeSprite_("ResultRanks");
    overallLabelSprite = makeSprite_("ResultOverallLabel");
    overallRankSprite = makeSprite_("ResultOverallRank");
    rankBurstSprite.reset();
    overallRank = '\0';
    const auto& titleColor_ = IsBadEnding() ? RESULT_FAILURE_TEXT_COLOR : RESULT_CLEAR_TEXT_COLOR;
    SetResultText(clearSprite, settings.value(IsBadEnding() ? "BadEndText" : "ClearText", std::string()), 64.0f, "LEFT", titleColor_);
    SetResultText(promptSprite, settings.value("PromptText", std::string()), 24.0f);
    SetResultText(resultLabelsSprite, "戦闘時間\nジャストアクション\n被ダメージ", 27.0f);
    SetResultText(overallLabelSprite, preview && !previewResult_ ? "総合ランク（演出確認）" : "総合ランク", 26.0f, "CENTER",
        IsBadEnding() ? DirectX::XMFLOAT4{0.73f, 0.65f, 0.66f, 1.0f} : DirectX::XMFLOAT4{0.83f, 0.85f, 0.80f, 1.0f});

    const auto mode_ = manager.GetGameModeManager();
    const auto result_ = previewResult_ ? previewResult_ : (!preview && mode_ ? &mode_->GetBattleResult() : nullptr);
    RefreshResultSprites(result_, preview && !previewResult_);
}

void BossFinishDirector::RefreshResultSprites(const GameModeManager::BattleResult* result_, bool sampleRank_)
{
    const char previousRank_ = overallRank;
    const auto& titleColor_ = IsBadEnding() ? RESULT_FAILURE_TEXT_COLOR : RESULT_CLEAR_TEXT_COLOR;
    if (!result_)
    {
        SetResultText(resultValuesSprite, "―\n―\n―", 27.0f, "RIGHT");
        SetResultText(resultRanksSprite, "―\n―\n―", 27.0f, "CENTER", titleColor_);
        overallRank = sampleRank_ ? 'S' : '\0';
    }
    else
    {
        const int actions_ = result_->justDodges + result_->justGuards;
        const auto elapsedCentiseconds_ = std::llround(result_->elapsedSeconds * 100.0);
        SetResultText(resultValuesSprite, std::format("{:02}:{:02}.{:02}\n{}回（回避{} / ガード{}）\n{:.2f}",
            elapsedCentiseconds_ / 6000, elapsedCentiseconds_ / 100 % 60, elapsedCentiseconds_ % 100,
            actions_, result_->justDodges, result_->justGuards, result_->damageTaken), 27.0f, "RIGHT");

        const auto& ranks_ = settings.at("ResultRanks");
        const auto getRankIndex_ = [&](const char* key_, double value_, bool higherIsBetter_)
        {
            const auto& thresholds_ = ranks_.at(key_);
            for (int index_ = 0; index_ < 3; ++index_)
            {
                const double threshold_ = thresholds_[index_].get<double>();
                if (higherIsBetter_ ? value_ >= threshold_ : value_ <= threshold_)
                {
                    return index_;
                }
            }
            return 3;
        };
        const int timeRank_ = getRankIndex_("TimeSeconds", result_->elapsedSeconds, false);
        const int actionsRank_ = getRankIndex_("JustActions", actions_, true);
        const int damageRank_ = getRankIndex_("DamageTaken", result_->damageTaken, false);
        SetResultText(resultRanksSprite, std::format("{}\n{}\n{}", RESULT_GRADES[timeRank_],
            RESULT_GRADES[actionsRank_], RESULT_GRADES[damageRank_]), 27.0f, "CENTER", titleColor_);

        // 低いランクほど重く評価する。総合の境界に並んだ場合は低いランクへ丸める。
        std::array<int, 3> rankIndexes_{timeRank_, actionsRank_, damageRank_};
        std::sort(rankIndexes_.begin(), rankIndexes_.end());
        const int weightedRank_ = rankIndexes_[0] * 10 + rankIndexes_[1] * 15 + rankIndexes_[2] * 75;
        overallRank = RESULT_GRADES[(weightedRank_ + 50) / 100];
    }

    const bool badEnding_ = IsBadEnding();
    SetResultText(overallRankSprite, overallRank ? std::string(1, overallRank) : "―", badEnding_ ? 88.0f : 134.0f, "CENTER",
        badEnding_ ? DirectX::XMFLOAT4{0.82f, 0.72f, 0.72f, 1.0f} : DirectX::XMFLOAT4{1, 1, 1, 1});
    if (badEnding_ || !overallRank || overallRank == previousRank_)
    {
        return;
    }

    nlohmann::json effectSettings_ = {{"ShadowColor", {0.02f, 0.03f, 0.055f, 1.0f}}, {"ShadowOpacity", 0.82f},
        {"ShadowOffset", {2.0f, 4.0f}}, {"ShadowSoftness", 5.0f}};
    if (overallRank == 'S')
    {
        effectSettings_.update({{"GlowColor", {0.51f, 0.72f, 1.0f, 0.9f}}, {"GlowIntensity", 2.0f}, {"GlowRadius", 20.0f},
            {"OutlineColor", {0.94f, 0.98f, 1.0f, 0.9f}}, {"OutlineWidth", 2.0f},
            {"SweepColor", {0.86f, 0.96f, 1.0f, 1.0f}}, {"SweepIntensity", 0.58f}, {"SweepSpeed", 0.55f},
            {"SweepWidth", 0.12f}, {"PulseAmount", 0.12f}, {"PulseSpeed", 0.52f}});

        if (!rankBurstSprite)
        {
            const auto burst_ = MakeObject("ResultRankBurst");
            const auto sprite_ = burst_->AddComponent<SpriteRenderer>();
            sprite_->Deserialize({{"SdrTexture", true}, {"LinearSampling", true}});
            if (sprite_->LoadTexture(GraphicsDeviceManager::Instance().GetDevice(), RESULT_RANK_BURST_PATH))
            {
                const auto size_ = sprite_->GetTextureSize();
                sprite_->SetUVRect(0.0f, 0.0f, size_.x, size_.y);
                sprite_->SetPivot(PosOfsset::MID);
                rankBurstSprite = burst_;
            }
        }
    }
    else if (overallRank == 'A')
    {
        effectSettings_.update({{"GlowColor", {1.0f, 0.65f, 0.27f, 0.85f}}, {"GlowIntensity", 1.45f}, {"GlowRadius", 12.0f},
            {"SweepColor", {1.0f, 0.90f, 0.63f, 1.0f}}, {"SweepIntensity", 0.38f}, {"SweepSpeed", 0.32f}});
    }
    else if (overallRank == 'B')
    {
        effectSettings_.update({{"GlowColor", {0.38f, 0.69f, 1.0f, 0.75f}}, {"GlowIntensity", 0.82f}, {"GlowRadius", 8.0f}});
    }
    const auto effect_ = overallRankSprite->GetOrAddComponent<UIVisualEffectComponent>(false);
    effect_->Deserialize({{"Settings", effectSettings_}});
    effect_->SetStarted(true);
    effect_->SetProgress(1.0f);
}

bool BossFinishDirector::Capture(ID3D11RenderTargetView* target_)
{
    BS_PROFILE_SCOPE("BossFinishDirector::Capture");
    auto& graphics_ = GraphicsDeviceManager::Instance();
    Microsoft::WRL::ComPtr<ID3D11Resource> resource_;
    target_->GetResource(resource_.GetAddressOf());
    Microsoft::WRL::ComPtr<ID3D11Texture2D> source_;
    if (FAILED(resource_.As(&source_)))
    {
        return false;
    }
    D3D11_TEXTURE2D_DESC description_{};
    source_->GetDesc(&description_);
    if (description_.SampleDesc.Count != 1)
    {
        return false;
    }
    D3D11_TEXTURE2D_DESC previous_{};
    if (capturedTexture)
    {
        capturedTexture->GetDesc(&previous_);
    }
    if (!capturedView || previous_.Width != description_.Width || previous_.Height != description_.Height ||
        previous_.Format != description_.Format || previous_.MipLevels != description_.MipLevels ||
        previous_.ArraySize != description_.ArraySize)
    {
        // 終盤の加工中は毎フレームコピーするため、解像度と形式が同じ間はバッファを使い回す。
        description_.Usage = D3D11_USAGE_DEFAULT;
        description_.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        description_.CPUAccessFlags = 0;
        description_.MiscFlags = 0;
        capturedView.Reset();
        capturedTexture.Reset();
        if (FAILED(graphics_.GetDevice()->CreateTexture2D(&description_, nullptr, capturedTexture.GetAddressOf())) ||
            FAILED(graphics_.GetDevice()->CreateShaderResourceView(capturedTexture.Get(), nullptr, capturedView.GetAddressOf())))
        {
            BS_LOG_INFO(LOG_CATEGORY::GAME, "Finish capture unavailable; clear and title flow continues.");
            return false;
        }
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
    object_->transform->MarkDirty();
    object_->transform->UpdateWorldMatrix();
    const auto sprite_ = object_->GetComponent<SpriteRenderer>();
    sprite_->SetLocalSize({width_, height_});
    sprite_->SetColor(color_);
    sprites_.Submit(sprite_);
    sprites_.RenderAll(GraphicsDeviceManager::Instance().GetContext());
}

void BossFinishDirector::ApplyPostEffects(GraphicsDeviceManager& graphics_) const
{
    if (IsBadEndingImpactShot() || (trueEndingTime < 0.0f && badEndingTime < 0.0f))
    {
        return;
    }

    auto& bloom_ = graphics_.GetBloomParams();
    auto& filter_ = graphics_.GetFilterParams();
    auto& radial_ = graphics_.GetRadialBlurParams();
    auto& vignette_ = graphics_.GetVignetteParams();
    radial_.blurCenter = {0.5f, 0.45f};
    radial_.blurMaskRadius = 0.48f;
    vignette_ = VignetteSystem::Parameters{};
    vignette_.enabled = true;
    vignette_.center = radial_.blurCenter;
    vignette_.intensity = 1.65f;
    vignette_.smoothness = 0.65f;

    if (trueEndingTime >= 0.0f)
    {
        const float time_ = trueEndingTime;
        const float charge_ = ShotProgress(time_, TRUE_END_CHARGE, TRUE_END_BEAM) *
            (1.0f - ShotProgress(time_, TRUE_END_PILLAR, TRUE_END_PILLAR + 0.6f));
        const float beam_ = ImpactPulse(time_, TRUE_END_BEAM, 0.48f);
        const float pillar_ = ImpactPulse(time_, TRUE_END_PILLAR, 0.76f);
        const float sweep_ = ImpactPulse(time_, TRUE_END_SWEEP_HIT, 0.28f);
        bloom_.enabled = true;
        bloom_.intensity = 0.42f + 0.48f * charge_ + 0.72f * (std::max)(beam_, pillar_);
        filter_.colorBalance = {1.04f + 0.08f * charge_, 1.01f, 0.96f - 0.12f * charge_};
        radial_.blurRadius = 0.07f * sweep_ + 0.16f * (std::max)(beam_, pillar_);
        vignette_.color = {0.25f, 0.16f, 0.10f, 1.0f};
        vignette_.opacity = 0.24f + 0.22f * charge_;
    }
    else
    {
        const float time_ = badEndingTime;
        const float mud_ = ShotProgress(time_, BAD_END_KNEEL, BAD_END_FALL);
        const float rise_ = ShotProgress(time_, BAD_END_RISE, BAD_END_RISE + 1.15f);
        const float hit_ = ImpactPulse(time_, BAD_END_PIERCE, 0.55f);
        const float roar_ = ImpactPulse(time_, BAD_END_RISE + 0.28f, 0.50f);
        filter_.saturation = 0.90f - 0.28f * mud_;
        filter_.colorBalance = {1.04f + 0.12f * rise_ + 0.14f * hit_, 0.98f - 0.10f * rise_, 1.02f - 0.08f * rise_};
        radial_.blurRadius = 0.12f * hit_ + 0.07f * roar_;
        vignette_.color = {0.30f, 0.08f, 0.11f, 1.0f};
        vignette_.opacity = 0.27f + 0.18f * mud_ + 0.20f * hit_;
    }
}

void BossFinishDirector::RenderOutput(SpriteRenderSystem& sprites_, ID3D11RenderTargetView* target_, bool resultPreview_)
{
    BS_PROFILE_SCOPE("BossFinishDirector::RenderOutput");
    if (resultPreview_ ? !resultPreviewReady : !active)
    {
        return;
    }
    auto& graphics_ = GraphicsDeviceManager::Instance();
    auto* context_ = graphics_.GetContext();
    if (!resultPreview_ && captureRequested)
    {
        showFixed = Capture(target_);
        captureRequested = false;
    }
    const float styleStrength_ = IsBadEnding()
        ? ShotProgress(badEndingTime, BAD_END_RISE + 0.1f, BAD_END_RISE + 1.25f)
        : ShotProgress(trueEndingTime, TRUE_END_FINISH - 0.85f, TRUE_END_FINISH - 0.12f);
    const bool drawLiveStyle_ = !resultPreview_ && !showFixed && styleStrength_ > 0.0f &&
        badEndingTime < BAD_END_COVERED && Capture(target_);
    // 撮影要求の有無・成功とは独立して、本番の既定時刻に一度だけ完了させる。
    // 接触診断NGや再撮影で要求を消費しても、クリア文字と決定入力まで必ず進める。
    const float held_ = resultPreview_ ? settings["HoldTime"].get<float>() + resultPreviewAge :
        elapsed - GetResultStartTime(ending);
    if (!resultPreview_ && held_ >= 0.0f)
    {
        CompleteFinish();
    }
    context_->OMSetRenderTargets(1, &target_, nullptr);
    const auto* states_ = graphics_.GetRenderState();
    context_->OMSetBlendState(states_->GetBlendState(BlendState::Transparency), nullptr, 0xffffffff);
    context_->OMSetDepthStencilState(states_->GetDepthStencilState(DepthState::NoTestNoWrite), 0);
    context_->RSSetState(states_->GetRasterizerState(RasterizerState::SolidCullNone));
    D3D11_VIEWPORT viewport_{};
    UINT count_ = 1;
    context_->RSGetViewports(&count_, &viewport_);
    const float width_ = viewport_.Width, height_ = viewport_.Height;
    const float fitWidth_ = (std::min)(width_, height_ * GetAspect()), fitHeight_ = fitWidth_ / GetAspect();
    const float left_ = (width_-fitWidth_)*0.5f, top_ = (height_-fitHeight_)*0.5f;
    if (resultPreview_)
    {
        const DirectX::XMFLOAT4 backdrop_ = IsBadEnding() ? DirectX::XMFLOAT4{0, 0, 0, 1} :
            DirectX::XMFLOAT4{0.055f, 0.059f, 0.086f, 1.0f};
        const float clear_[4] = {backdrop_.x, backdrop_.y, backdrop_.z, backdrop_.w};
        context_->ClearRenderTargetView(target_, clear_);
        RenderResult(sprites_, left_, top_, fitWidth_, fitHeight_, held_);
        return;
    }
    imageSprite->GetComponent<UIVisualEffectComponent>()->SetRuntimeStrength(styleStrength_);
    if (drawLiveStyle_)
    {
        const auto sprite_ = imageSprite->GetComponent<SpriteRenderer>();
        const auto size_ = sprite_->GetTextureSize();
        sprite_->SetUVRect(0.0f, 0.0f, size_.x, size_.y);
        DrawSprite(sprites_, imageSprite, 0.0f, 0.0f, width_, height_, {1, 1, 1, 1});
    }
    if (badEndingTime >= BAD_END_FINISH)
    {
        const float black_[] = {0, 0, 0, 1};
        context_->ClearRenderTargetView(target_, black_);
    }
    else if (badEndingTime >= 0.0f)
    {
        RenderBadEnding(sprites_, target_, width_, height_);
    }
    else if (showFixed && capturedView)
    {
        const float black_[4] = {0,0,0,1};
        context_->ClearRenderTargetView(target_, black_);
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
    if (!IsBadEnding() && top_ > 0)
    {
        DrawSprite(sprites_, solidSprite, 0,0,width_,top_, {0,0,0,1});
        DrawSprite(sprites_, solidSprite,0,top_+fitHeight_,width_,top_,{0,0,0,1});
    }
    if (!IsBadEnding() && left_ > 0)
    {
        DrawSprite(sprites_, solidSprite,0,0,left_,height_,{0,0,0,1});
        DrawSprite(sprites_, solidSprite,left_+fitWidth_,0,left_,height_,{0,0,0,1});
    }
    // 白い閃光も実際の命中時刻に合わせ、攻撃前やプレビュー停止中に再発火させない。
    const float impactAge_ = elapsed - settings["HitStop"].get<float>() - GetImpactTime();
    if (impactConfirmed && impactAge_ >= 0.0f && impactAge_ < 0.12f)
    {
        DrawSprite(sprites_,solidSprite,0,0,width_,height_,{1,1,1,0.65f * std::exp(-impactAge_ / 0.035f)});
    }
    if (trueEndingTime >= 0.0f)
    {
        const float beamAge_ = trueEndingTime - TRUE_END_BEAM;
        if (beamAge_ >= 0.0f && beamAge_ < 0.2f)
        {
            DrawSprite(sprites_, solidSprite, 0, 0, width_, height_, {1.0f, 0.95f, 0.80f, 0.68f * std::exp(-beamAge_ / 0.065f)});
        }
    }
    if ((preview || completionCommitted) && held_ >= settings["HoldTime"].get<float>())
    {
        RenderResult(sprites_, left_, top_, fitWidth_, fitHeight_, held_);
    }
    if (returning)
    {
        DrawSprite(sprites_,solidSprite,0,0,width_,height_,{0,0,0,std::clamp(returnElapsed/settings["ReturnFade"].get<float>(),0.0f,1.0f)});
    }
}

void BossFinishDirector::RenderResult(SpriteRenderSystem& sprites_, float left_, float top_,
    float width_, float height_, float held_)
{
    const float hold_ = settings["HoldTime"].get<float>();
    const float reveal_ = ShotProgress(held_, hold_, hold_ + 0.55f);
    const float rows_ = ShotProgress(held_, hold_ + 0.2f, hold_ + 0.65f);
    const float uiScale_ = (std::min)(width_ / 1600.0f, height_ / 900.0f);
    const auto drawText_ = [&](const std::shared_ptr<GameObject>& object_, float x_, float y_, float pivot_,
        const DirectX::XMFLOAT4& color_, float zoom_ = 1.0f)
    {
        const auto size_ = object_->GetComponent<SpriteRenderer>()->GetTextureSize();
        const float scale_ = uiScale_ * zoom_;
        DrawSprite(sprites_, object_, left_ + width_ * x_ - size_.x * scale_ * pivot_,
            top_ + height_ * y_ - size_.y * scale_ * 0.5f, size_.x * scale_, size_.y * scale_, color_);
    };
    const bool badEnding_ = IsBadEnding();
    DrawSprite(sprites_, solidSprite, left_ + width_ * 0.12f, top_ + height_ * 0.155f,
        width_ * 0.76f, height_ * 0.67f, {0.018f, 0.014f, 0.025f, 0.88f * reveal_});
    const auto& titlePosition_ = settings["ClearPosition"];
    drawText_(clearSprite, titlePosition_[0].get<float>(), std::lerp(titlePosition_[1].get<float>(), 0.265f, reveal_),
        0.5f, {1, 1, 1, reveal_});
    const DirectX::XMFLOAT4 ruleColor_ = badEnding_
        ? DirectX::XMFLOAT4{0.67f, 0.22f, 0.26f, 0.64f * rows_} : DirectX::XMFLOAT4{0.70f, 0.63f, 0.42f, 0.64f * rows_};
    DrawSprite(sprites_, solidSprite, left_ + width_ * 0.165f, top_ + height_ * 0.364f,
        width_ * 0.67f, (std::max)(1.0f, height_ * 0.0018f), ruleColor_);
    DrawSprite(sprites_, solidSprite, left_ + width_ * 0.653f, top_ + height_ * 0.414f,
        (std::max)(1.0f, width_ * 0.001f), height_ * 0.352f, {ruleColor_.x, ruleColor_.y, ruleColor_.z, ruleColor_.w * 0.48f});
    drawText_(resultLabelsSprite, 0.152f, 0.585f, 0.0f, {1, 1, 1, rows_});
    drawText_(resultValuesSprite, 0.582f, 0.585f, 1.0f, {1, 1, 1, rows_});
    drawText_(resultRanksSprite, 0.618f, 0.585f, 0.5f, {1, 1, 1, rows_});

    const float rankAge_ = held_ - hold_ - RESULT_RANK_DELAY;
    const float rankReveal_ = ShotProgress(rankAge_, 0.0f, RESULT_RANK_REVEAL);
    if (rankReveal_ > 0.0f)
    {
        constexpr float RANK_X = 0.766f;
        constexpr float RANK_Y = 0.535f;
        const bool isS_ = overallRank == 'S' && !badEnding_;
        const float cycle_ = std::fmod(rankAge_, 2.35f) / 2.35f * DirectX::XM_2PI;
        DirectX::XMFLOAT4 rankColor_{1, 1, 1, rankReveal_};
        if (isS_)
        {
            rankColor_ = {0.72f + 0.28f * std::cos(cycle_), 0.72f + 0.28f * std::cos(cycle_ - 2.0944f),
                0.72f + 0.28f * std::cos(cycle_ + 2.0944f), rankReveal_};
        }
        else if (!badEnding_ && overallRank == 'A')
        {
            rankColor_ = {1.0f, 0.86f, 0.48f, rankReveal_};
        }
        else if (!badEnding_ && overallRank == 'B')
        {
            rankColor_ = {0.73f, 0.87f, 1.0f, rankReveal_};
        }
        if (const auto effect_ = overallRankSprite->GetComponent<UIVisualEffectComponent>())
        {
            effect_->SetSampleTime(rankAge_);
            effect_->SetRuntimeStrength(rankReveal_);
        }
        if (isS_ && rankBurstSprite)
        {
            // 尖った光を二層に分け、ランクの文字を中央に残したまま外へ広げる。
            const float flare_ = ShotProgress(rankAge_, 0.0f, 0.18f) *
                (1.0f - ShotProgress(rankAge_, 0.19f, 0.86f));
            const float burstSize_ = uiScale_ * std::lerp(115.0f, 270.0f, ShotProgress(rankAge_, 0.0f, 0.8f));
            const float rotation_ = std::fmod(rankAge_, 24.0f);
            const float centerX_ = left_ + width_ * RANK_X;
            const float centerY_ = top_ + height_ * RANK_Y;
            rankBurstSprite->transform->localRotation.z = -15.0f * rotation_;
            DrawSprite(sprites_, rankBurstSprite, centerX_, centerY_, burstSize_, burstSize_,
                {rankColor_.z, rankColor_.x, rankColor_.y, 0.30f * rankReveal_ + 0.30f * flare_});
            rankBurstSprite->transform->localRotation.z = 30.0f + 15.0f * rotation_;
            DrawSprite(sprites_, rankBurstSprite, centerX_, centerY_, burstSize_ * 0.81f, burstSize_ * 0.81f,
                {rankColor_.y, rankColor_.z, rankColor_.x, 0.23f * rankReveal_ + 0.22f * flare_});
        }
        const float rankZoom_ = isS_ ? std::lerp(1.24f, 1.0f, rankReveal_) : 1.0f;
        drawText_(overallRankSprite, RANK_X, RANK_Y, 0.5f, rankColor_, rankZoom_);
        drawText_(overallLabelSprite, RANK_X, 0.718f, 0.5f, {1, 1, 1, rankReveal_});
    }
    if (held_ >= settings["PromptDelay"].get<float>())
    {
        const auto& position_ = settings["PromptPosition"];
        drawText_(promptSprite, position_[0].get<float>(), position_[1].get<float>(), 0.5f, {1, 1, 1, 1});
    }
}

void BossFinishDirector::RenderBadEnding(SpriteRenderSystem& sprites_, ID3D11RenderTargetView* target_, float width_, float height_)
{
    if (width_ <= 0.0f || height_ <= 0.0f)
    {
        return;
    }
    const float time_ = badEndingTime;
    if (time_ >= BAD_END_FLASHBACK && time_ < BAD_END_KNEEL)
    {
        const float attack_ = ShotProgress(time_, BAD_END_FLASHBACK, BAD_END_FLASHBACK + 0.04f);
        const float decay_ = ShotProgress(time_, BAD_END_KNEEL - 0.13f, BAD_END_KNEEL);
        const float alpha_ = attack_ * (1.0f - decay_);
        if (flashbackSprite)
        {
            const auto sprite_ = flashbackSprite->GetComponent<SpriteRenderer>();
            const auto size_ = sprite_->GetTextureSize();
            const float zoom_ = 1.04f + 0.09f * decay_;
            const float cropWidth_ = (std::min)(size_.x, size_.y * width_ / height_) / zoom_;
            const float cropHeight_ = cropWidth_ * height_ / width_;
            sprite_->SetUVRect((size_.x - cropWidth_) * 0.5f, (size_.y - cropHeight_) * 0.5f, cropWidth_, cropHeight_);
            DrawSprite(sprites_, flashbackSprite, 0, 0, width_, height_, {1,1,1,alpha_});
        }
        else
        {
            DrawSprite(sprites_, solidSprite, 0, 0, width_, height_, {1,1,1,alpha_ * 0.4f});
        }
    }
    if (time_ >= BAD_END_WIPE)
    {
        const float progress_ = std::clamp((time_ - BAD_END_WIPE) / (BAD_END_COVERED - BAD_END_WIPE), 0.0f, 1.0f);
        if (badEndingGlass)
        {
            badEndingGlass->SetProgress(progress_);
            badEndingGlass->Update(time_ - BAD_END_WIPE, false);
        }
        if (!badEndingGlass || !badEndingGlass->Render(target_))
        {
            DrawSprite(sprites_, solidSprite, 0, 0, width_, height_, {0.025f,0.008f,0.012f,progress_});
        }
        const float black_ = ShotProgress(time_, BAD_END_COVERED, BAD_END_FINISH - 0.15f);
        if (black_ > 0.0f)
        {
            DrawSprite(sprites_, solidSprite, 0, 0, width_, height_, {0,0,0,black_});
        }
    }
}

void BossFinishDirector::CaptureEditorCamera()
{
    const auto editorCamera_ = manager.GetCameraManager()->GetEditorCamera();
    if (!editorCamera_)
    {
        return;
    }
    const auto anchor_ = GetStageAnchor();
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

#include "PrecompiledHeader.h"
#include "PlayerDeathDirector.h"
#include "GameModeManager.h"
#include "Game/Charcter/Player/PlayerControllerComponent.h"
#include "Engine/Common/DebugLog.h"
#include "Engine/Debug/TracyProfiler.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Camera/CameraManager.h"
#include "Engine/GamePlay/Camera/CameraComponent.h"
#include "Engine/GamePlay/Combat/HealthComponent.h"
#include "Engine/GamePlay/Input/KeyConfig.h"
#include "Engine/GamePlay/Input/MouseInput.h"
#include "Engine/GamePlay/Transform/ModelAttachmentComponent.h"
#include "Engine/Graphics/Resource/Animation/AnimationControllerComponent.h"
#include "Engine/Audio/AudioManager.h"
#include "Engine/GamePlay/Input/ControllerInput.h"
#include "Engine/GamePlay/Serialize/ActionRegistry.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <exception>
#include <imgui.h>

namespace
{
    constexpr const char* SETTINGS_PATH = "Data/Settings/PlayerDeath.json";

    // カメラの移動開始・停止を滑らかにし、死亡時の急な視点変化を抑える。
    float SmoothRatio(float time_, float duration_)
    {
        const float ratio_ = std::clamp(time_ / (std::max)(duration_, 0.01f), 0.0f, 1.0f);
        return ratio_ * ratio_ * (3.0f - 2.0f * ratio_);
    }

    // 演出カメラの視点と注視点を同じ比率で補間する。
    DirectX::XMFLOAT3 LerpPosition(const DirectX::XMFLOAT3& from_, const DirectX::XMFLOAT3& to_, float ratio_)
    {
        return {from_.x + (to_.x - from_.x) * ratio_, from_.y + (to_.y - from_.y) * ratio_,
            from_.z + (to_.z - from_.z) * ratio_};
    }
}

PlayerDeathDirector::PlayerDeathDirector(GameObjectManager& manager_) : manager(manager_)
{
    continueMenu.Initialize(manager);
    LoadSettings();
}

void PlayerDeathDirector::NormalizeSettings()
{
    const auto number_ = [&](const char* key_, float fallback_, float minimum_, float maximum_)
    {
        float value_ = settings.contains(key_) && settings[key_].is_number() ? settings[key_].get<float>() : fallback_;
        settings[key_] = std::clamp(std::isfinite(value_) ? value_ : fallback_, minimum_, maximum_);
    };
    const auto text_ = [&](const char* key_, const char* fallback_)
    {
        if (!settings.contains(key_) || !settings[key_].is_string() || settings[key_].get<std::string>().empty())
            settings[key_] = fallback_;
    };
    if (!settings.is_object()) settings = nlohmann::json::object();
    text_("StageCenterObject", "Stage");
    number_("CameraRise", 5.0f, 0.0f, 30.0f);
    number_("CenterPull", 0.18f, 0.0f, 0.8f);
    number_("CameraMoveTime", 2.0f, 0.1f, 10.0f);
    number_("BgmFadeOutTime", 2.5f, 0.1f, 10.0f);
    number_("DeathHold", 0.35f, 0.0f, 5.0f);
    number_("FadeOutTime", 0.8f, 0.1f, 5.0f);
    number_("ContinueDelay", 0.35f, 0.0f, 3.0f);
    number_("FadeInTime", 0.6f, 0.1f, 5.0f);
    number_("CameraReturnTime", 1.0f, 0.1f, 10.0f);
}

// 不足する項目には既定値を補い、古い設定ファイルでも同じ演出経路を使用する。
void PlayerDeathDirector::LoadSettings()
{
    BS_PROFILE_SCOPE("PlayerDeathDirector::LoadSettings");
    settings = nlohmann::json::object();
    try
    {
        std::ifstream file_(SETTINGS_PATH);
        if (file_) file_ >> settings;
        status = "Settings loaded.";
    }
    catch (const std::exception& error_)
    {
        status = std::string("Settings fallback: ") + error_.what();
    }
    NormalizeSettings();
}

void PlayerDeathDirector::SaveSettings()
{
    BS_PROFILE_SCOPE("PlayerDeathDirector::SaveSettings");
    NormalizeSettings();
    std::ofstream file_(SETTINGS_PATH, std::ios::binary);
    for (const char character_ : settings.dump(4))
    {
        if (character_ == '\n') file_.put('\r');
        file_.put(character_);
    }
    file_ << "\r\n";
    file_.flush();
    status = file_ ? "Settings saved." : "Settings save failed.";
}

void PlayerDeathDirector::RequestDeath(const std::shared_ptr<GameObject>& player_)
{
    if (!IsActive() && player_ && player_->GetComponent<PlayerControllerComponent>()) requestedPlayer = player_;
}

void PlayerDeathDirector::UpdatePlayerPresentation(float deltaTime_)
{
    BS_PROFILE_SCOPE("PlayerDeathDirector::UpdatePlayerPresentation");
    if (!player) return;
    if (const auto controller_ = player->GetComponent<PlayerControllerComponent>()) controller_->Update(deltaTime_);
    // 通常のAnimationControllerを通すため、クリップ、遷移、終了条件はグラフ上の設定がそのまま反映される。
    if (const auto animation_ = player->GetComponent<AnimationControllerComponent>()) animation_->Update(deltaTime_);
    player->UpdateTransform();
    for (const auto& attachment_ : manager.FindComponents<ModelAttachmentComponent>())
        if (attachment_ && attachment_->IsEnabled()) attachment_->SyncAttachment();
}

void PlayerDeathDirector::Begin(const std::shared_ptr<GameObject>& player_)
{
    BS_PROFILE_SCOPE("PlayerDeathDirector::Begin");
    player = player_;
    requestedPlayer.reset();
    NormalizeSettings();
    AudioManager::Instance().ResetPresentationAudio();
    ControllerInput::Instance().StopRumble();
    active = true;
    phase = Phase::DYING;
    elapsed = 0.0f;
    deathFinishedTime = -1.0f;
    fadeAlpha = 0.0f;
    returningToTitle = false;
    continueMenu.Reset();
    previousCursorLock = MouseInput::Instance().IsCursorLockEnabled();
    previousInputCapture = manager.IsPlayInputCaptured();
    if (const auto mode_ = manager.GetGameModeManager())
    {
        previousPlayerLock = mode_->IsPlayerControlLocked(); previousAILock = mode_->IsEnemyAILocked();
        previousGameplayLock = mode_->IsGameplayLocked(); previousPresentation = mode_->IsPresentationPlaying();
        previousBattlePause = mode_->IsBattlePaused();
        mode_->SetPlayerControlLocked(true); mode_->SetEnemyAILocked(true);
        mode_->SetGameplayLocked(true); mode_->SetPresentationPlaying(true); mode_->SetBattlePaused(true);
    }
    if (const auto controller_ = player->GetComponent<PlayerControllerComponent>()) controller_->BeginDeathState();
    // 判断だけを止めても進行中の攻撃が残るため、Sceneの通常更新と全Colliderを演出中だけ停止する。
    colliderStates.clear();
    for (const auto& object_ : manager.GetObjects())
        if (object_ && !object_->IsPendingDestroy())
            for (const auto& collider_ : object_->colliderComponents)
                if (collider_)
                {
                    colliderStates.emplace_back(collider_, collider_->IsRuntimeSuppressed());
                    collider_->SetRuntimeSuppressed(true);
                }
    const auto cameraManager_ = manager.GetCameraManager();
    const auto source_ = cameraManager_ ? cameraManager_->GetActiveCamera() : nullptr;
    camera = std::make_shared<CameraComponent>();
    const auto position_ = player->transform->GetWorldPosition();
    startEye = source_ ? source_->GetEye() : DirectX::XMFLOAT3{position_.x, position_.y + 3.0f, position_.z - 6.0f};
    startFocus = source_ ? source_->GetFocus() : position_;
    cameraUp = source_ ? source_->GetUp() : DirectX::XMFLOAT3{0.0f, 1.0f, 0.0f};
    camera->SetPerspectiveFov(source_ ? source_->GetFovY() : 45.0f, source_ ? source_->GetAspect() : 16.0f / 9.0f,
        source_ ? source_->GetNear() : 0.1f, source_ ? source_->GetFar() : 1000.0f);
    const auto stage_ = manager.Find(settings["StageCenterObject"].get<std::string>());
    const auto center_ = stage_ && stage_->transform ? stage_->transform->GetWorldPosition() : position_;
    const float pull_ = settings["CenterPull"].get<float>();
    endEye = startEye;
    endEye.x += (center_.x - position_.x) * pull_;
    endEye.z += (center_.z - position_.z) * pull_;
    endEye.y += settings["CameraRise"].get<float>();
    endFocus = {position_.x + (center_.x - position_.x) * pull_, position_.y + 0.7f,
        position_.z + (center_.z - position_.z) * pull_};
    camera->SetLookAt(startEye, startFocus, cameraUp);
}

bool PlayerDeathDirector::Update(float deltaTime_)
{
    BS_PROFILE_SCOPE("PlayerDeathDirector::Update");
    if (!active)
    {
        auto actor_ = requestedPlayer.lock();
        if (!actor_)
            for (const auto& controller_ : manager.FindComponents<PlayerControllerComponent>())
            {
                const auto candidate_ = controller_ && controller_->IsEnabled() ? controller_->GetGameObject() : nullptr;
                const auto health_ = candidate_ ? candidate_->GetComponent<HealthComponent>() : nullptr;
                if (candidate_ && !candidate_->IsPendingDestroy() && health_ && health_->IsDead()) { actor_ = candidate_; break; }
            }
        if (!actor_) return false;
        const auto health_ = actor_->GetComponent<HealthComponent>();
        if (!actor_->transform || actor_->IsPendingDestroy() || !health_ || !health_->IsDead())
        {
            requestedPlayer.reset();
            return false;
        }
        Begin(actor_);
    }
    if (!player || player->IsPendingDestroy()) { Stop(); return true; }
    // 死亡演出は戦闘のHitStopやTimeScaleから独立させ、起き上がりの実際の尺まで待つ。
    elapsed += std::isfinite(deltaTime_) ? (std::max)(deltaTime_, 0.0f) : 0.0f;
    UpdatePlayerPresentation(std::isfinite(deltaTime_) ? (std::max)(deltaTime_, 0.0f) : 0.0f);
    const auto controller_ = player->GetComponent<PlayerControllerComponent>();
    if (phase == Phase::DYING)
    {
        // 距離感を音量と高音の減衰で表現し、最終的に無音へ到達させる。
        AudioManager::Instance().SetDeathBgmEffect(elapsed / settings["BgmFadeOutTime"].get<float>());
        const float ratio_ = SmoothRatio(elapsed, settings["CameraMoveTime"].get<float>());
        camera->SetLookAt(LerpPosition(startEye, endEye, ratio_), LerpPosition(startFocus, endFocus, ratio_), cameraUp);
        // 死亡Stateがグラフの実際の完了を確認するまで、暗転とコンテニューへ進まない。
        if (deathFinishedTime < 0.0f && controller_ && controller_->IsDeathAnimationFinished()) deathFinishedTime = elapsed;
        if (deathFinishedTime >= 0.0f)
            fadeAlpha = std::clamp((elapsed - deathFinishedTime - settings["DeathHold"].get<float>()) /
                settings["FadeOutTime"].get<float>(), 0.0f, 1.0f);
        if (fadeAlpha >= 1.0f && elapsed >= (std::max)(settings["CameraMoveTime"].get<float>(), settings["BgmFadeOutTime"].get<float>()))
        {
            phase = Phase::CONTINUE;
            elapsed = 0.0f;
            continueMenu.Reset();
            manager.SetPlayInputCaptured(false);
            MouseInput::Instance().SetCursorLockEnabled(false);
        }
    }
    else if (phase == Phase::CONTINUE)
    {
        if (elapsed >= settings["ContinueDelay"].get<float>())
        {
            continueMenu.Update(deltaTime_);
            const int result_ = continueMenu.TakeResult();
            if (result_ == 0) BeginRecovery();
            else if (result_ == 1 && !returningToTitle)
            {
                // 死亡時の抑制はシーン破棄まで維持し、遷移直前に戦闘を再開しない。
                returningToTitle = true;
                ActionRegistry::Instance().Call("GoSceneTitle");
            }
        }
    }
    else
    {
        AudioManager::Instance().SetDeathBgmEffect(1.0f - elapsed / settings["FadeInTime"].get<float>());
        fadeAlpha = 1.0f - std::clamp(elapsed / settings["FadeInTime"].get<float>(), 0.0f, 1.0f);
        const float duration_ = settings["CameraReturnTime"].get<float>();
        const float ratio_ = SmoothRatio(elapsed, duration_);
        camera->SetLookAt(LerpPosition(endEye, startEye, ratio_), LerpPosition(endFocus, startFocus, ratio_), cameraUp);
        if (controller_ && controller_->IsReviveAnimationFinished() && fadeAlpha <= 0.0f && elapsed >= duration_) Stop();
    }
    // 終了フレームも通常のAI更新へ戻さず、起き上がり中に次の行動を開始させない。
    return true;
}

void PlayerDeathDirector::BeginRecovery()
{
    BS_PROFILE_SCOPE("PlayerDeathDirector::BeginRecovery");
    const auto controller_ = player->GetComponent<PlayerControllerComponent>();
    if (!controller_ || !controller_->IsDeathAnimationFinished()) return;
    controller_->BeginReviveState();
    phase = Phase::RECOVERING;
    elapsed = 0.0f;
}

void PlayerDeathDirector::Stop()
{
    BS_PROFILE_SCOPE("PlayerDeathDirector::Stop");
    requestedPlayer.reset();
    if (!active) return;
    AudioManager::Instance().ResetPresentationAudio();
    if (player && !player->IsPendingDestroy())
        if (const auto controller_ = player->GetComponent<PlayerControllerComponent>()) controller_->ResetDeathPresentationState();
    for (const auto& state_ : colliderStates)
        if (const auto collider_ = state_.first.lock()) collider_->SetRuntimeSuppressed(state_.second);
    colliderStates.clear();
    if (const auto mode_ = manager.GetGameModeManager())
    {
        mode_->SetPlayerControlLocked(previousPlayerLock); mode_->SetEnemyAILocked(previousAILock);
        mode_->SetGameplayLocked(previousGameplayLock); mode_->SetPresentationPlaying(previousPresentation);
        mode_->SetBattlePaused(previousBattlePause);
    }
    manager.SetPlayInputCaptured(previousInputCapture);
    MouseInput::Instance().SetCursorLockEnabled(previousCursorLock);
    active = false;
    fadeAlpha = 0.0f;
    player.reset();
    camera.reset();
}

void PlayerDeathDirector::RenderOutput(SpriteRenderSystem& sprites_)
{
    if (!active) return;
    BS_PROFILE_SCOPE("PlayerDeathDirector::RenderOutput");
    continueMenu.DrawBackdrop(sprites_,fadeAlpha);
    if (phase != Phase::CONTINUE || elapsed < settings["ContinueDelay"].get<float>()) return;
    continueMenu.Draw(sprites_,"YOU DIED","コンテニュー",true);
}

void PlayerDeathDirector::DrawSettingsGUI()
{
    if (!ImGui::CollapsingHeader("Player Death / Continue")) return;
    ImGui::TextWrapped("%s", status.c_str());
    ImGui::BeginDisabled(IsActive());
    const auto number_ = [&](const char* label_, const char* key_, float minimum_, float maximum_)
    {
        float value_ = settings[key_].get<float>();
        if (ImGui::DragFloat(label_, &value_, 0.01f, minimum_, maximum_)) settings[key_] = value_;
    };
    const auto text_ = [&](const char* label_, const char* key_)
    {
        char value_[256]{};
        const auto current_ = settings[key_].get<std::string>();
        const auto length_ = (std::min)(current_.size(), sizeof(value_) - 1);
        std::copy_n(current_.data(), length_, value_);
        if (ImGui::InputText(label_, value_, sizeof(value_))) settings[key_] = value_;
    };
    ImGui::TextUnformatted("Player graph: PlayerDeath -> PlayerRevive -> SS_Idle");
    ImGui::TextWrapped("Edit animation clips and transitions in the Player animation graph.");
    text_("Stage center object", "StageCenterObject");
    number_("Camera rise", "CameraRise", 0.0f, 30.0f); number_("Center pull", "CenterPull", 0.0f, 0.8f);
    number_("Camera move time", "CameraMoveTime", 0.1f, 10.0f); number_("Death hold", "DeathHold", 0.0f, 5.0f);
    number_("Fade out time", "FadeOutTime", 0.1f, 5.0f); number_("Continue delay", "ContinueDelay", 0.0f, 3.0f);
    number_("BGM fade out time", "BgmFadeOutTime", 0.1f, 10.0f);
    number_("Fade in time", "FadeInTime", 0.1f, 5.0f);
    number_("Camera return time", "CameraReturnTime", 0.1f, 10.0f);
    if (ImGui::Button("Save death settings")) SaveSettings();
    ImGui::SameLine();
    if (ImGui::Button("Reload death settings")) LoadSettings();
    ImGui::EndDisabled();
}

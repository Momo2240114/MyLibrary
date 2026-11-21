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
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Graphics/Core/LightManager.h"
#include "Engine/Graphics/Particle/ParticleComponent.h"
#include "Engine/Graphics/Resource/Animation/AnimationControllerComponent.h"
#include "Engine/Graphics/Resource/GlTFModel.h"
#include "Engine/Math/Vector.h"
#include "Engine/Audio/AudioManager.h"
#include "Engine/GamePlay/Input/ControllerInput.h"
#include "Engine/GamePlay/Serialize/ActionRegistry.h"
#include "Engine/UI/BattleHudComponent.h"
#include "Engine/UI/UIProgressBarComponent.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <exception>
#include <imgui.h>

namespace
{
    constexpr const char* SETTINGS_PATH = "Data/Settings/PlayerDeath.json";
    constexpr const char* REVIVE_PARTICLE_PATH = "Data/Particle/PlayerRevive.json";
    constexpr float REVIVE_DISPERSE_TIME = 1.3f;
    constexpr float REVIVE_LIGHT_INTENSITY = 9.0f;
    constexpr float FLASHBACK_CAMERA_BLEND_TIME = 0.25f;

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
        {
            settings[key_] = fallback_;
        }
    };
    if (!settings.is_object())
    {
        settings = nlohmann::json::object();
    }
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
        if (file_)
        {
            file_ >> settings;
        }
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
        if (character_ == '\n')
        {
            file_.put('\r');
        }
        file_.put(character_);
    }
    file_ << "\r\n";
    file_.flush();
    status = file_ ? "Settings saved." : "Settings save failed.";
}

void PlayerDeathDirector::RequestDeath(const std::shared_ptr<GameObject>& player_)
{
    if (!IsActive() && player_ && player_->GetComponent<PlayerControllerComponent>())
    {
        requestedPlayer = player_;
    }
}

void PlayerDeathDirector::UpdatePlayerPresentation(float deltaTime_)
{
    BS_PROFILE_SCOPE("PlayerDeathDirector::UpdatePlayerPresentation");
    if (!player)
    {
        return;
    }
    if (const auto controller_ = player->GetComponent<PlayerControllerComponent>())
    {
        controller_->Update(deltaTime_);
    }
    // 通常のAnimationControllerを通すため、クリップ、遷移、終了条件はグラフ上の設定がそのまま反映される。
    if (const auto animation_ = player->GetComponent<AnimationControllerComponent>())
    {
        animation_->Update(deltaTime_);
    }
    player->UpdateTransform();
    for (const auto& attachment_ : manager.FindComponents<ModelAttachmentComponent>())
    {
        if (attachment_ && attachment_->IsEnabled())
        {
            attachment_->SyncAttachment();
        }
    }
}

void PlayerDeathDirector::PrepareFlashbackShot()
{
    constexpr std::array<const char*, 6> FRAME_NODES{"head", "pelvis", "hand_l", "hand_r", "foot_l", "foot_r"};
    std::array<Vector3, FRAME_NODES.size()> points_{};
    std::size_t pointCount_ = 0;
    if (const auto model_ = player->GetComponent<GltfModelCommponennt>(); model_ && model_->IsModelReady())
    {
        model_->UpdateTransformIfDirty();
        for (const auto name_ : FRAME_NODES)
        {
            const int index_ = model_->GetNodeIndex(name_);
            if (index_ < 0)
            {
                continue;
            }
            const auto& world_ = model_->GetNodes()[index_].worldTransform;
            if (std::isfinite(world_._41) && std::isfinite(world_._42) && std::isfinite(world_._43))
            {
                points_[pointCount_++] = {world_._41, world_._42, world_._43};
            }
        }
    }

    Vector3 focus_ = Vector3{player->transform->GetWorldPosition()} + Vector3{0.0f, 0.55f, 0.0f};
    if (pointCount_ >= 2)
    {
        Vector3 min_ = points_[0];
        Vector3 max_ = points_[0];
        for (std::size_t index_ = 1; index_ < pointCount_; ++index_)
        {
            min_ = Vector3::Min(min_, points_[index_]);
            max_ = Vector3::Max(max_, points_[index_]);
        }
        focus_ = (min_ + max_) * 0.5f;
    }

    const auto stage_ = manager.Find(settings["StageCenterObject"].get<std::string>());
    Vector3 inside_ = Vector3{stage_ && stage_->transform ? stage_->transform->GetWorldPosition() : camera->GetEye()} - focus_;
    inside_.y = 0.0f;
    Vector3 side_ = inside_;
    if (pointCount_ >= 2)
    {
        const auto bodyAxis_ = points_[pointCount_ - 1] - points_[0];
        side_ = Vector3{bodyAxis_.z, 0.0f, -bodyAxis_.x};
        if (side_.Dot(inside_) < 0.0f)
        {
            side_ = -side_;
        }
        if (side_.LengthSquared() < 0.01f)
        {
            side_ = inside_;
        }
    }
    if (side_.LengthSquared() < 0.01f)
    {
        side_ = {0.0f, 0.0f, -1.0f};
    }
    side_.Normalize();
    auto viewFrom_ = Vector3::Up + side_ * 0.42f;
    viewFrom_.Normalize();

    // 倒れた姿勢の頭と手足を画面の縦横に収め、輪郭が切れないよう余白を取る。
    auto right_ = Vector3::Up.Cross(-viewFrom_);
    right_.Normalize();
    const auto viewUp_ = (-viewFrom_).Cross(right_);
    const float halfFov_ = std::tan(DirectX::XMConvertToRadians(std::clamp(camera->GetFovY(), 10.0f, 120.0f)) * 0.5f);
    const float halfWidth_ = halfFov_ * (std::max)(camera->GetAspect(), 0.1f);
    float distance_ = (std::max)(2.25f, camera->GetNear() + 1.0f);
    for (std::size_t index_ = 0; index_ < pointCount_; ++index_)
    {
        const auto offset_ = points_[index_] - focus_;
        const float width_ = (std::abs(offset_.Dot(right_)) + 0.35f) / halfWidth_;
        const float height_ = (std::abs(offset_.Dot(viewUp_)) + 0.35f) / halfFov_;
        distance_ = (std::max)(distance_, offset_.Dot(viewFrom_) + (std::max)(width_, height_));
    }
    flashbackFocus = focus_;
    flashbackEye = focus_ + viewFrom_ * distance_;
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
    frameFlashback = flashbackCaptureAvailable;
    elapsed = 0.0f;
    deathFinishedTime = -1.0f;
    fadeAlpha = 0.0f;
    returningToTitle = false;
    continueMenu.Reset();
    previousCursorLock = MouseInput::Instance().IsCursorLockEnabled();
    previousInputCapture = manager.IsPlayInputCaptured();
    if (const auto mode_ = manager.GetGameModeManager())
    {
        previousPlayerLock = mode_->IsPlayerControlLocked();
        previousAILock = mode_->IsEnemyAILocked();
        previousGameplayLock = mode_->IsGameplayLocked();
        previousPresentation = mode_->IsPresentationPlaying();
        previousBattlePause = mode_->IsBattlePaused();
        mode_->SetPlayerControlLocked(true);
        mode_->SetEnemyAILocked(true);
        mode_->SetGameplayLocked(true);
        mode_->SetPresentationPlaying(true);
        mode_->SetBattlePaused(true);
    }
    if (const auto controller_ = player->GetComponent<PlayerControllerComponent>())
    {
        controller_->BeginDeathState();
    }
    // 判断だけを止めても進行中の攻撃が残るため、Sceneの通常更新と全Colliderを演出中だけ停止する。
    colliderStates.clear();
    for (const auto& object_ : manager.GetObjects())
    {
        if (object_ && !object_->IsPendingDestroy())
        {
            for (const auto& collider_ : object_->colliderComponents)
            {
                if (collider_)
                {
                    colliderStates.emplace_back(collider_, collider_->IsRuntimeSuppressed());
                    collider_->SetRuntimeSuppressed(true);
                }
            }
        }
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
        {
            for (const auto& controller_ : manager.FindComponents<PlayerControllerComponent>())
            {
                const auto candidate_ = controller_ && controller_->IsEnabled() ? controller_->GetGameObject() : nullptr;
                const auto health_ = candidate_ ? candidate_->GetComponent<HealthComponent>() : nullptr;
                if (candidate_ && !candidate_->IsPendingDestroy() && health_ && health_->IsDead())
                {
                    actor_ = candidate_;
                    break;
                }
            }
        }
        if (!actor_)
        {
            return false;
        }
        const auto health_ = actor_->GetComponent<HealthComponent>();
        if (!actor_->transform || actor_->IsPendingDestroy() || !health_ || !health_->IsDead())
        {
            requestedPlayer.reset();
            return false;
        }
        requestedPlayer = actor_;
        const auto barObject_ = manager.Find("PlayerHpBar");
        const auto bar_ = barObject_ && !barObject_->IsPendingDestroy() ?
            barObject_->GetComponent<UIProgressBarComponent>() : nullptr;
        if (bar_ && bar_->IsEnabled() && bar_->IsStarted())
        {
            bar_->SetTargetRate(0.0f);
            // HUDが遅延ゲージを描き終えるまで、Sceneの通常更新を続ける。
            if (bar_->HasRemainingFill())
            {
                if (bar_->ShouldUpdate())
                {
                    return false;
                }
                for (const auto& hud_ : manager.FindComponents<BattleHudComponent>())
                {
                    if (hud_->UpdatesPlayerHealthBar(bar_.get()))
                    {
                        return false;
                    }
                }
            }
        }
        Begin(actor_);
    }
    if (!player || player->IsPendingDestroy())
    {
        Stop();
        return true;
    }
    // 死亡演出は戦闘のHitStopやTimeScaleから独立させ、起き上がりの実際の尺まで待つ。
    const float presentationDeltaTime_ = std::isfinite(deltaTime_) ? (std::max)(deltaTime_, 0.0f) : 0.0f;
    elapsed += presentationDeltaTime_;
    UpdatePlayerPresentation(presentationDeltaTime_);
    const auto controller_ = player->GetComponent<PlayerControllerComponent>();
    if (phase == Phase::DYING)
    {
        // 距離感を音量と高音の減衰で表現し、最終的に無音へ到達させる。
        AudioManager::Instance().SetDeathBgmEffect(elapsed / settings["BgmFadeOutTime"].get<float>());
        // 死亡Stateがグラフの実際の完了を確認するまで、暗転とコンテニューへ進まない。
        if (deathFinishedTime < 0.0f && controller_ && controller_->IsDeathAnimationFinished())
        {
            deathFinishedTime = elapsed;
            if (frameFlashback)
            {
                PrepareFlashbackShot();
            }
        }
        const float ratio_ = SmoothRatio(elapsed, settings["CameraMoveTime"].get<float>());
        const float shotRatio_ = frameFlashback && deathFinishedTime >= 0.0f ?
            SmoothRatio(elapsed - deathFinishedTime, FLASHBACK_CAMERA_BLEND_TIME) : 0.0f;
        camera->SetLookAt(LerpPosition(LerpPosition(startEye, endEye, ratio_), flashbackEye, shotRatio_),
            LerpPosition(LerpPosition(startFocus, endFocus, ratio_), flashbackFocus, shotRatio_),
            LerpPosition(cameraUp, Vector3::Up, shotRatio_));
        if (deathFinishedTime >= 0.0f)
        {
            const float hold_ = (std::max)(settings["DeathHold"].get<float>(), frameFlashback ? FLASHBACK_CAMERA_BLEND_TIME : 0.0f);
            fadeAlpha = std::clamp((elapsed - deathFinishedTime - hold_) /
                settings["FadeOutTime"].get<float>(), 0.0f, 1.0f);
        }
        if (fadeAlpha >= 1.0f && elapsed >= (std::max)(settings["CameraMoveTime"].get<float>(), settings["BgmFadeOutTime"].get<float>()))
        {
            endEye = camera->GetEye();
            endFocus = camera->GetFocus();
            phase = Phase::CONTINUE;
            reachedGameOver = true;
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
            if (result_ == 0)
            {
                BeginRecovery();
            }
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
        camera->SetLookAt(LerpPosition(endEye, startEye, ratio_), LerpPosition(endFocus, startFocus, ratio_),
            frameFlashback ? LerpPosition(Vector3::Up, cameraUp, ratio_) : cameraUp);

        if (!recoveryEffect || recoveryEffect->IsPendingDestroy())
        {
            Stop();
            return true;
        }
        recoveryEffect->transform->localPosition = player->transform->GetWorldPosition();
        recoveryEffect->transform->MarkDirty();
        recoveryEffect->UpdateTransform();
        const auto particles_ = recoveryEffect->GetComponent<ParticleComponent>();
        const auto light_ = recoveryEffect->GetComponent<LightComponent>();
        if (reviveFinishedTime < 0.0f && controller_ && controller_->IsReviveAnimationFinished() && fadeAlpha <= 0.0f)
        {
            reviveFinishedTime = elapsed;
            if (particles_ && particles_->IsEnabled())
            {
                // 起き上がったら発生を止め、残った粒子の寿命を進めながら外側へ光を散らす。
                particles_->Stop();
                particles_->TriggerEmitter("ReviveScatter");
            }
        }

        const float disperseTime_ = reviveFinishedTime < 0.0f ? 0.0f : elapsed - reviveFinishedTime;
        if (light_ && light_->IsEnabled())
        {
            light_->intensity = REVIVE_LIGHT_INTENSITY * SmoothRatio(elapsed, 0.5f) *
                (1.0f - SmoothRatio(disperseTime_, REVIVE_DISPERSE_TIME));
            light_->Update(presentationDeltaTime_);
        }
        if (particles_ && particles_->IsEnabled())
        {
            particles_->Update(presentationDeltaTime_);
        }
        if (reviveFinishedTime >= 0.0f && disperseTime_ >= REVIVE_DISPERSE_TIME && elapsed >= duration_)
        {
            Stop();
        }
    }
    // 終了フレームも通常のAI更新へ戻さず、起き上がり中に次の行動を開始させない。
    return true;
}

void PlayerDeathDirector::BeginRecovery()
{
    BS_PROFILE_SCOPE("PlayerDeathDirector::BeginRecovery");
    const auto controller_ = player->GetComponent<PlayerControllerComponent>();
    if (!controller_ || !controller_->IsDeathAnimationFinished())
    {
        return;
    }
    controller_->BeginReviveState();
    phase = Phase::RECOVERING;
    elapsed = 0.0f;
    reviveFinishedTime = -1.0f;

    recoveryEffect = manager.Create("PlayerReviveEffect");
    recoveryEffect->saveFlg = false;
    const auto light_ = recoveryEffect->AddComponent<LightComponent>();
    light_->type = LightComponent::Type::Point;
    light_->color = {0.62f, 0.82f, 1.0f};
    light_->offsetPosition = {0.0f, 1.1f, 0.0f};
    light_->range = 5.0f;
    light_->intensity = 0.0f;

    const auto particles_ = recoveryEffect->AddComponent<ParticleComponent>();
    try
    {
        if (particles_->LoadAsset(REVIVE_PARTICLE_PATH) && particles_->HasRuntimeResources())
        {
            particles_->PlayEmitter("ReviveSparks");
            particles_->PlayEmitter("ReviveMist");
            return;
        }
        BS_LOG_WARNING(LOG_CATEGORY::GAME, "Revive particle asset unavailable.");
    }
    catch (const std::exception& error_)
    {
        BS_LOG_WARNING(LOG_CATEGORY::GAME, std::string("Revive particle load failed: ") + error_.what());
    }
    particles_->SetEnabled(false);
}

void PlayerDeathDirector::Stop()
{
    BS_PROFILE_SCOPE("PlayerDeathDirector::Stop");
    requestedPlayer.reset();
    if (!active)
    {
        return;
    }
    flashbackCaptureAvailable = false;
    if (recoveryEffect)
    {
        manager.Destroy(recoveryEffect);
        recoveryEffect.reset();
    }
    AudioManager::Instance().ResetPresentationAudio();
    if (player && !player->IsPendingDestroy())
    {
        if (const auto controller_ = player->GetComponent<PlayerControllerComponent>())
        {
            controller_->ResetDeathPresentationState();
        }
    }
    for (const auto& state_ : colliderStates)
    {
        if (const auto collider_ = state_.first.lock())
        {
            collider_->SetRuntimeSuppressed(state_.second);
        }
    }
    colliderStates.clear();
    if (const auto mode_ = manager.GetGameModeManager())
    {
        mode_->SetPlayerControlLocked(previousPlayerLock);
        mode_->SetEnemyAILocked(previousAILock);
        mode_->SetGameplayLocked(previousGameplayLock);
        mode_->SetPresentationPlaying(previousPresentation);
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
    if (!active)
    {
        return;
    }
    BS_PROFILE_SCOPE("PlayerDeathDirector::RenderOutput");
    continueMenu.DrawBackdrop(sprites_,fadeAlpha);
    if (phase != Phase::CONTINUE || elapsed < settings["ContinueDelay"].get<float>())
    {
        return;
    }
    continueMenu.Draw(sprites_,"YOU DIED","コンテニュー",true);
}

void PlayerDeathDirector::CaptureFlashback(ID3D11RenderTargetView* target_)
{
    if (!target_ || !active || phase == Phase::RECOVERING || deathFinishedTime < 0.0f || !flashbackCaptureAvailable)
    {
        return;
    }
    if (phase == Phase::DYING && elapsed - deathFinishedTime < FLASHBACK_CAMERA_BLEND_TIME)
    {
        return;
    }
    BS_PROFILE_SCOPE("PlayerDeathDirector::CaptureFlashback");
    flashbackCaptureAvailable = false;

    auto& graphics_ = GraphicsDeviceManager::Instance();
    auto* device_ = graphics_.GetDevice();
    auto* context_ = graphics_.GetContext();
    Microsoft::WRL::ComPtr<ID3D11Resource> resource_;
    target_->GetResource(resource_.GetAddressOf());
    Microsoft::WRL::ComPtr<ID3D11Texture2D> source_;
    if (FAILED(resource_.As(&source_)))
    {
        return;
    }

    D3D11_TEXTURE2D_DESC desc_{};
    source_->GetDesc(&desc_);
    // 通常画面とドッキング表示はいずれもRGBA8の最終出力を使う。
    if (desc_.Format != DXGI_FORMAT_R8G8B8A8_UNORM || desc_.SampleDesc.Count != 1)
    {
        return;
    }
    desc_.Usage = D3D11_USAGE_STAGING;
    desc_.BindFlags = 0;
    desc_.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc_.MiscFlags = 0;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> staging_;
    if (FAILED(device_->CreateTexture2D(&desc_, nullptr, staging_.GetAddressOf())))
    {
        return;
    }

    const UINT rowBytes_ = desc_.Width * 4;
    std::vector<std::uint8_t> pixels_(static_cast<std::size_t>(rowBytes_) * desc_.Height);
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depth_;
    context_->OMGetRenderTargets(0, nullptr, depth_.GetAddressOf());
    context_->OMSetRenderTargets(0, nullptr, nullptr);
    context_->CopyResource(staging_.Get(), source_.Get());
    context_->OMSetRenderTargets(1, &target_, depth_.Get());
    D3D11_MAPPED_SUBRESOURCE mapped_{};
    if (FAILED(context_->Map(staging_.Get(), 0, D3D11_MAP_READ, 0, &mapped_)))
    {
        return;
    }
    for (UINT y_ = 0; y_ < desc_.Height; ++y_)
    {
        const auto* sourceRow_ = static_cast<const std::uint8_t*>(mapped_.pData) + static_cast<std::size_t>(y_) * mapped_.RowPitch;
        auto* outputRow_ = pixels_.data() + static_cast<std::size_t>(y_) * rowBytes_;
        for (UINT x_ = 0; x_ < rowBytes_; x_ += 4)
        {
            const auto gray_ = static_cast<std::uint8_t>(
                (299 * sourceRow_[x_] + 587 * sourceRow_[x_ + 1] + 114 * sourceRow_[x_ + 2] + 500) / 1000);
            outputRow_[x_] = outputRow_[x_ + 1] = outputRow_[x_ + 2] = gray_;
            outputRow_[x_ + 3] = 255;
        }
    }
    context_->Unmap(staging_.Get(), 0);

    desc_.Usage = D3D11_USAGE_IMMUTABLE;
    desc_.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc_.CPUAccessFlags = 0;
    const D3D11_SUBRESOURCE_DATA data_{pixels_.data(), rowBytes_, 0};
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view_;
    if (SUCCEEDED(device_->CreateTexture2D(&desc_, &data_, texture_.GetAddressOf())) &&
        SUCCEEDED(device_->CreateShaderResourceView(texture_.Get(), nullptr, view_.GetAddressOf())))
    {
        flashbackTexture = std::move(view_);
    }
}

void PlayerDeathDirector::ClearDeathHistory()
{
    reachedGameOver = false;
    flashbackCaptureAvailable = true;
    flashbackTexture.Reset();
}

void PlayerDeathDirector::DrawSettingsGUI()
{
    if (!ImGui::CollapsingHeader("Player Death / Continue"))
    {
        return;
    }
    ImGui::TextWrapped("%s", status.c_str());
    ImGui::BeginDisabled(IsActive());
    const auto number_ = [&](const char* label_, const char* key_, float minimum_, float maximum_)
    {
        float value_ = settings[key_].get<float>();
        if (ImGui::DragFloat(label_, &value_, 0.01f, minimum_, maximum_))
        {
            settings[key_] = value_;
        }
    };
    const auto text_ = [&](const char* label_, const char* key_)
    {
        char value_[256]{};
        const auto current_ = settings[key_].get<std::string>();
        const auto length_ = (std::min)(current_.size(), sizeof(value_) - 1);
        std::copy_n(current_.data(), length_, value_);
        if (ImGui::InputText(label_, value_, sizeof(value_)))
        {
            settings[key_] = value_;
        }
    };
    ImGui::TextUnformatted("Player graph: PlayerDeath -> PlayerRevive -> SS_Idle");
    ImGui::TextWrapped("Edit animation clips and transitions in the Player animation graph.");
    text_("Stage center object", "StageCenterObject");
    number_("Camera rise", "CameraRise", 0.0f, 30.0f);
    number_("Center pull", "CenterPull", 0.0f, 0.8f);
    number_("Camera move time", "CameraMoveTime", 0.1f, 10.0f);
    number_("Death hold", "DeathHold", 0.0f, 5.0f);
    number_("Fade out time", "FadeOutTime", 0.1f, 5.0f);
    number_("Continue delay", "ContinueDelay", 0.0f, 3.0f);
    number_("BGM fade out time", "BgmFadeOutTime", 0.1f, 10.0f);
    number_("Fade in time", "FadeInTime", 0.1f, 5.0f);
    number_("Camera return time", "CameraReturnTime", 0.1f, 10.0f);
    if (ImGui::Button("Save death settings"))
    {
        SaveSettings();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload death settings"))
    {
        LoadSettings();
    }
    ImGui::EndDisabled();
}

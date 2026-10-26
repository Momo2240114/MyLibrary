#include "PrecompiledHeader.h"
#include "ScenePresentationComponent.h"
#include "MenuNavigationComponent.h"
#include "ScreenFadeComponent.h"
#include "Engine/Audio/AudioManager.h"
#include "Engine/Core/GameRuntimeSettings.h"
#include "Engine/GamePlay/Camera/CameraComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Input/KeyConfig.h"
#include "Engine/GamePlay/Input/MouseInput.h"
#include "Engine/GamePlay/Transform/ModelAttachmentComponent.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Graphics/Resource/Animation/AnimationControllerComponent.h"
#include "Engine/Graphics/Resource/GlTFModel.h"
#include "Engine/Graphics/Resource/Sprite.h"
#include "Engine/Physics/PhysXSystem.h"
#include <algorithm>
#include <cmath>
#include <fstream>

const std::string& ScenePresentationComponent::GetName() const
{
    static const std::string NAME = "ScenePresentationComponent";
    return NAME;
}

void ScenePresentationComponent::Start()
{
    BS_PROFILE_SCOPE("ScenePresentationComponent::Start");
    elapsedTime = 0.0f;
    animationSwitchTime = 0.0f;
    animationChoice = GetTickCount();
    musicStarted = false;
    playingMusicPath.clear();
    showSettings = false;
    GetManager().SetShapeRenderEnabled(settings.value("ShowDebugShapes", true));
    if (settings.value("ReleaseCursor", false))
    {
        previousCursorLock = MouseInput::Instance().IsCursorLockEnabled();
        cursorChanged = true;
        MouseInput::Instance().SetCursorLockEnabled(false);
    }
    if (settings.value("StopPreviousMusic", false)) AudioManager::Instance().StopBGM();
    if (settings.value("ResetPhysicsTimeScale", false)) PhysXSystem::SetTimeScale(1.0f);
}

void ScenePresentationComponent::OnDestroy()
{
    BS_PROFILE_SCOPE("ScenePresentationComponent::OnDestroy");
    EndRender();
    if (cursorChanged) MouseInput::Instance().SetCursorLockEnabled(previousCursorLock);
    cursorChanged = false;
    if (musicStarted && !playingMusicPath.empty()) AudioManager::Instance().StopByPath(playingMusicPath);
    playingMusicPath.clear();
    musicStarted = false;
}

std::shared_ptr<CameraComponent> ScenePresentationComponent::GetCamera() const
{
    const auto owner_ = gameObject.lock();
    const auto object_ = owner_ && owner_->manager ? owner_->GetManager().Find(settings.value("CameraObject", std::string())) : nullptr;
    return object_ ? object_->GetComponent<CameraComponent>() : nullptr;
}

DirectX::XMFLOAT4 ScenePresentationComponent::GetClearColor() const
{
    const auto color_ = settings.value("ClearColor", std::vector<float>{0.0f, 0.0f, 0.0f, 1.0f});
    return color_.size() == 4 ? DirectX::XMFLOAT4{color_[0], color_[1], color_[2], color_[3]} : DirectX::XMFLOAT4{0,0,0,1};
}

void ScenePresentationComponent::BeginRender()
{
    BS_PROFILE_SCOPE("ScenePresentationComponent::BeginRender");
    if (rendering) return;
    auto& graphics_ = GraphicsDeviceManager::Instance();
    previousBloom = graphics_.GetBloomParams();
    previousVignette = graphics_.GetVignetteParams();
    rendering = true;
    if (settings.contains("Bloom"))
    {
        const auto& bloom_ = settings.at("Bloom");
        graphics_.GetBloomParams().enabled = bloom_.value("Enabled", previousBloom.enabled);
        graphics_.GetBloomParams().intensity = bloom_.value("Intensity", previousBloom.intensity);
        graphics_.GetBloomParams().threshold = bloom_.value("Threshold", previousBloom.threshold);
    }
    if (settings.contains("Vignette"))
    {
        const auto& vignette_ = settings.at("Vignette");
        graphics_.GetVignetteParams().enabled = vignette_.value("Enabled", previousVignette.enabled);
        graphics_.GetVignetteParams().intensity = vignette_.value("Intensity", previousVignette.intensity);
        const auto color_ = vignette_.value("Color", std::vector<float>{0,0,0,1});
        if (color_.size() == 4) graphics_.GetVignetteParams().color = {color_[0],color_[1],color_[2],color_[3]};
    }
}

void ScenePresentationComponent::EndRender()
{
    if (!rendering) return;
    GraphicsDeviceManager::Instance().GetBloomParams() = previousBloom;
    GraphicsDeviceManager::Instance().GetVignetteParams() = previousVignette;
    rendering = false;
}

void ScenePresentationComponent::Update(float deltaTime_)
{
    BS_PROFILE_SCOPE("ScenePresentationComponent::Update");
    if (GetManager().GetSceneRuntimeState() != SCENE_RUNTIME_STATE::RUNNING) return;
    elapsedTime += (std::max)(0.0f, deltaTime_);
    // 表示対象と色をScene JSONから復元し、装備にも同じ単色表示を適用する。
    if (settings.contains("Silhouette"))
    {
        const auto& silhouette_ = settings.at("Silhouette");
        const auto color_ = silhouette_.value("Color", std::vector<float>{1,1,1,1});
        if (color_.size() == 4 && silhouette_.contains("Objects"))
            for (const auto& name_ : silhouette_.at("Objects"))
            {
                const auto object_ = GetManager().Find(name_.get<std::string>());
                const auto model_ = object_ ? object_->GetComponent<GltfModelCommponennt>() : nullptr;
                if (model_) model_->SetRuntimeSilhouette(silhouette_.value("Enabled", true),
                    {color_[0], color_[1], color_[2], color_[3]});
            }
    }
    // 候補名と再生間隔はJSONで指定し、既存Controllerのクロスフェードを使用する。
    if (settings.contains("AnimationCycle"))
    {
        const auto& cycle_ = settings.at("AnimationCycle");
        const auto object_ = GetManager().Find(cycle_.value("Object", std::string()));
        const auto controller_ = object_ ? object_->GetComponent<AnimationControllerComponent>() : nullptr;
        animationSwitchTime -= (std::max)(0.0f, deltaTime_);
        if (controller_ && animationSwitchTime <= 0.0f)
        {
            const auto states_ = cycle_.value("States", std::vector<std::string>{});
            animationSwitchTime = (std::max)(0.01f, cycle_.value("Interval", 4.0f));
            if (!states_.empty())
            {
                animationChoice = animationChoice * 1664525u + 1013904223u;
                const auto& name_ = states_[(animationChoice >> 16) % states_.size()];
                if (controller_->GetStates().find(name_) != controller_->GetStates().end())
                    controller_->ChangeState(name_, (std::max)(0.0f, cycle_.value("BlendDuration", 0.3f)));
            }
        }
    }
    if (!musicStarted)
    {
        const auto path_ = settings.value("MusicPath", std::string());
        if (!path_.empty())
        {
            AudioManager::Instance().PlayBGMByPath(path_);
            playingMusicPath = path_;
        }
        musicStarted = true;
    }
    if (settings.contains("AttachmentObjects"))
        for (const auto& name_ : settings.at("AttachmentObjects"))
        {
            const auto object_ = GetManager().Find(name_.get<std::string>());
            const auto attachment_ = object_ ? object_->GetComponent<ModelAttachmentComponent>() : nullptr;
            if (attachment_ && attachment_->IsStarted() && attachment_->IsEnabled()) attachment_->SyncAttachment();
        }
    auto& graphics_ = GraphicsDeviceManager::Instance();
    const float width_ = static_cast<float>((std::max)(1, graphics_.GetRenderWidth()));
    const float height_ = static_cast<float>((std::max)(1, graphics_.GetRenderHeight()));
    const auto& presentation_ = settings.at("Presentation");
    if (const auto camera_ = GetCamera())
    {
        const auto position_ = presentation_.value("CameraPosition", std::vector<float>{0,0,-5});
        const auto target_ = presentation_.value("LookAtPosition", std::vector<float>{0,0,0});
        if (position_.size() == 3 && target_.size() == 3)
        {
            DirectX::XMFLOAT3 eye_{position_[0],position_[1],position_[2]};
            DirectX::XMFLOAT3 focus_{target_[0],target_[1],target_[2]};
            eye_.x += std::sin(elapsedTime * presentation_.value("CameraMotionSpeed", 0.0f)) * presentation_.value("CameraMotionAmplitude", 0.0f);
            if (std::abs(focus_.x - eye_.x) + std::abs(focus_.z - eye_.z) < 0.01f) focus_.z = eye_.z + 1.0f;
            const float aspect_ = width_ / height_;
            // 狭い画面でもSceneの構図を保つよう、基準Aspectから縦FOVを補正する。
            const float fieldOfView_ = DirectX::XMConvertToDegrees(2.0f * std::atan(
                std::tan(DirectX::XMConvertToRadians(presentation_.value("FieldOfView", 60.0f)) * 0.5f) *
                (std::max)(1.0f, presentation_.value("ReferenceAspect", 16.0f / 9.0f) / aspect_)));
            camera_->SetPerspectiveFov(fieldOfView_, aspect_, camera_->GetNear(), camera_->GetFar());
            if (presentation_.value("Orthographic", false))
            {
                // 平行投影では奥行きによる大小差をなくし、シルエットの構図を固定する。
                const float height_ = (std::max)(0.01f, presentation_.value("OrthographicHeight", 10.0f));
                DirectX::XMFLOAT4X4 projection_;
                DirectX::XMStoreFloat4x4(&projection_, DirectX::XMMatrixOrthographicLH(
                    height_ * aspect_, height_, camera_->GetNear(), camera_->GetFar()));
                camera_->SetProjectionMatrix(projection_);
            }
            camera_->SetLookAt(eye_, focus_, {0,1,0});
        }
    }
    const auto logo_ = GetManager().Find(settings.value("LogoObject", std::string()));
    const auto sprite_ = logo_ ? logo_->GetComponent<SpriteRenderer>() : nullptr;
    const auto fadeObject_ = GetManager().Find(settings.value("FadeObject", std::string()));
    const auto fade_ = fadeObject_ ? fadeObject_->GetComponent<ScreenFadeComponent>() : nullptr;
    if (sprite_)
    {
        const auto size_ = sprite_->GetTextureSize();
        if (size_.x > 0.0f && size_.y > 0.0f)
        {
            const float center_ = presentation_.value("LogoCenterX", 0.5f);
            const float top_ = presentation_.value("LogoTop", 0.0f);
            const float availableHeight_ = (std::max)(1.0f, (presentation_.value("MenuTop", 0.64f) - top_) * height_ - 32.0f * (std::min)(width_ / 1920.0f, height_ / 1080.0f));
            const float availableWidth_ = width_ * (std::min)(presentation_.value("LogoWidth", 0.56f), 2.0f * (std::min)(center_, 1.0f - center_));
            const float scale_ = (std::min)(availableWidth_ / size_.x, (std::min)(availableHeight_, height_ * presentation_.value("LogoMaxHeight", 0.55f)) / size_.y);
            sprite_->SetLocalSize({size_.x * scale_, size_.y * scale_});
            sprite_->SetUVRect(0,0,size_.x,size_.y);
            Vector3 position_(width_ * center_ - size_.x * scale_ * 0.5f, height_ * top_, 0);
            logo_->transform->SetLocalPosition(position_);
            logo_->UpdateTransform();
        }
        const float fadeOut_ = fade_ && fade_->IsFadingOut() ? 1.0f - std::clamp((fade_->GetFadeOutTime() - 0.2f) / 0.3f, 0.0f, 1.0f) : 1.0f;
        auto color_ = sprite_->GetConfiguredColor();
        color_.w *= std::clamp((elapsedTime - 0.35f) / 0.9f, 0.0f, 1.0f) * fadeOut_;
        sprite_->SetRuntimeColor(color_);
    }
    const auto owner_ = GetGameObject();
    const auto menu_ = owner_ ? owner_->GetComponent<MenuNavigationComponent>() : nullptr;
    if (!GameRuntimeSettings::IsGameOnly() && KeyConfig::Instance().ActionPressedIn(settings.value("SettingsInputAction", std::string()))) showSettings = !showSettings;
    if (menu_) menu_->SetInputSuppressed(showSettings);
    if (showSettings && !GameRuntimeSettings::IsGameOnly())
    {
        if (ImGui::Begin("Scene presentation", &showSettings))
        {
            Gui();
            if (menu_) menu_->Gui();
            if (ImGui::Button("Save Scene"))
            {
                const auto sceneName_ = settings.value("SceneName", std::string());
                if (!sceneName_.empty()) GetManager().SaveScene(sceneName_);
            }
            ImGui::SameLine();
            if (ImGui::Button("Reload"))
            {
                // Scene Objectの設定だけを再読込し、実行中の時間や選択状態は維持する。
                std::ifstream file_(settings.value("SettingsObjectPath", std::string()));
                if (file_)
                {
                    const auto object_ = nlohmann::json::parse(file_, nullptr, false);
                    if (!object_.is_discarded() && object_.contains("Components"))
                        for (const auto& component_ : object_.at("Components"))
                        {
                            const auto type_ = component_.value("TypeName", std::string());
                            if (type_ == GetName()) Deserialize(component_.at("Data"));
                            else if (menu_ && type_ == menu_->GetName()) menu_->Deserialize(component_.at("Data"));
                        }
                }
            }
        }
        ImGui::End();
    }
}

void ScenePresentationComponent::Gui()
{
    auto& presentation_ = settings["Presentation"];
    bool orthographic_ = presentation_.value("Orthographic", false);
    if (ImGui::Checkbox("Orthographic", &orthographic_)) presentation_["Orthographic"] = orthographic_;
    if (orthographic_)
    {
        float height_ = presentation_.value("OrthographicHeight", 10.0f);
        if (ImGui::DragFloat("Orthographic Height", &height_, 0.05f, 0.01f, 1000.0f) && std::isfinite(height_))
            presentation_["OrthographicHeight"] = (std::max)(0.01f, height_);
    }
    if (settings.contains("Silhouette"))
    {
        auto& silhouette_ = settings["Silhouette"];
        bool enabled_ = silhouette_.value("Enabled", true);
        if (ImGui::Checkbox("Silhouette", &enabled_)) silhouette_["Enabled"] = enabled_;
        auto color_ = silhouette_.value("Color", std::vector<float>{1,1,1,1});
        if (color_.size() == 4 && ImGui::ColorEdit4("Silhouette Color", color_.data())) silhouette_["Color"] = color_;
    }
    if (settings.contains("AnimationCycle"))
    {
        auto& cycle_ = settings["AnimationCycle"];
        float interval_ = cycle_.value("Interval", 4.0f);
        float blendDuration_ = cycle_.value("BlendDuration", 0.3f);
        if (ImGui::DragFloat("Animation Interval", &interval_, 0.05f, 0.01f, 60.0f) && std::isfinite(interval_))
            cycle_["Interval"] = (std::max)(0.01f, interval_);
        if (ImGui::DragFloat("Animation Blend", &blendDuration_, 0.01f, 0.0f, 10.0f) && std::isfinite(blendDuration_))
            cycle_["BlendDuration"] = (std::max)(0.0f, blendDuration_);
    }
    for (const char* key_ : {"CameraPosition", "LookAtPosition"})
    {
        auto value_ = presentation_.value(key_, std::vector<float>{0,0,0});
        if (value_.size() == 3 && ImGui::DragFloat3(key_, value_.data(), 0.05f)) presentation_[key_] = value_;
    }
    for (const char* key_ : {"FieldOfView", "CameraMotionAmplitude", "CameraMotionSpeed", "LogoCenterX", "LogoTop", "LogoWidth", "LogoMaxHeight", "MenuTop"})
    {
        float value_ = presentation_.value(key_, 0.0f);
        if (ImGui::DragFloat(key_, &value_, 0.01f) && std::isfinite(value_))
        {
            if (std::string(key_) == "FieldOfView") value_ = std::clamp(value_, 20.0f, 80.0f);
            else value_ = std::clamp(value_, 0.0f, 1.0f);
            presentation_[key_] = value_;
        }
    }
    ImGui::TextUnformatted("Menu layout: edit the scene UIWidget RectTransform.");
}

void ScenePresentationComponent::Serialize(nlohmann::json& json_) const
{
    json_ = settings;
}

void ScenePresentationComponent::Deserialize(const nlohmann::json& json_)
{
    BS_PROFILE_SCOPE("ScenePresentationComponent::Deserialize");
    settings = json_.is_object() ? json_ : nlohmann::json::object();
    if (!settings.contains("Presentation") || !settings["Presentation"].is_object()) settings["Presentation"] = nlohmann::json::object();
}

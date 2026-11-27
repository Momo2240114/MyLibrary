#include "PrecompiledHeader.h"
#include "GameMenuComponent.h"
#include "ScreenTransitionManager.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Input/KeyConfig.h"
#include "Engine/GamePlay/Input/MouseInput.h"
#include "Engine/GamePlay/Input/ControllerInput.h"
#include <imgui.h>
#include <imgui_stdlib.h>

const std::string& GameMenuComponent::GetName() const
{
    static const std::string NAME = "GameMenuComponent";
    return NAME;
}

void GameMenuComponent::Start()
{
    BS_PROFILE_SCOPE("GameMenuComponent::Start");
    menu.Initialize(GetManager());
    const auto owner_ = GetGameObject();
    if (!owner_)
    {
        return;
    }
    const std::weak_ptr<GameMenuComponent> weak_ = owner_->GetComponent<GameMenuComponent>();
    owner_->RegisterGlobalAction("UI","OpenMenu",[weak_]()
    {
        if (const auto component_ = weak_.lock())
        {
            component_->Open();
        }
    });
    owner_->RegisterGlobalAction("UI","CloseMenu",[weak_]()
    {
        if (const auto component_ = weak_.lock())
        {
            component_->Close();
        }
    });
}

void GameMenuComponent::OnDestroy()
{
    Close();
    if (pauseWorld)
    {
        GetManager().SetGameMenuPaused(false);
    }
}

void GameMenuComponent::Open()
{
    if (active || ScreenTransitionManager::Instance().IsInputBlocked())
    {
        return;
    }
    previousInputCapture = GetManager().IsPlayInputCaptured();
    previousCursorLock = MouseInput::Instance().IsCursorLockEnabled();
    active = true;
    submitted = false;
    menu.Reset();
    GetManager().SetPlayInputCaptured(false);
    MouseInput::Instance().SetCursorLockEnabled(false);
    ControllerInput::Instance().StopRumble();
    if (pauseWorld)
    {
        GetManager().SetGameMenuPaused(true);
    }
}

void GameMenuComponent::Close()
{
    if (!active)
    {
        return;
    }
    GetManager().SetPlayInputCaptured(previousInputCapture);
    MouseInput::Instance().SetCursorLockEnabled(previousCursorLock);
    // 戦闘とPhysicsの再開は次のManager更新へ揃え、閉じたフレームだけ物理が進むことを防ぐ。
    active = false;
}

// ゲーム時計を止めてもUI時間は進め、再開フレームの入力を戦闘へ持ち越さない。
void GameMenuComponent::UpdateUnscaled(float deltaTime_)
{
    BS_PROFILE_SCOPE("GameMenuComponent::UpdateUnscaled");
    if (GetManager().SupportsEditorPlayMode() || GetManager().GetSceneRuntimeState() != SCENE_RUNTIME_STATE::RUNNING)
    {
        return;
    }
    const bool focused_ = GetForegroundWindow() == GraphicsDeviceManager::Instance().GetWindowHandle();
    const bool skippingPresentation_ = !active && GetManager().GetGameModeManager()->CanSkipPresentation();
    const bool toggle_ = focused_ && !skippingPresentation_ &&
        KeyConfig::Instance().ActionPressedIn(toggleInputAction);
    if (!active)
    {
        if (toggle_ && !GetManager().IsPlayerDeathActive() && !GetManager().IsBossFinishActive())
        {
            Open();
        }
        return;
    }
    menu.Update(deltaTime_);
    if (toggle_)
    {
        menu.Confirm(0);
    }
    if (submitted || ScreenTransitionManager::Instance().IsInputBlocked())
    {
        return;
    }
    const int result_ = menu.TakeResult(toggleInputAction);
    if (result_ < 0)
    {
        return;
    }
    submitted = true;
    const auto owner_ = GetGameObject();
    const auto action_ = result_ == 0 ? firstAction : secondAction;
    if (result_ == 0)
    {
        Close();
    }
    if (owner_ && !action_.empty())
    {
        owner_->ExecuteAction(action_);
    }
}

void GameMenuComponent::RenderMenu(SpriteRenderSystem& sprites_)
{
    BS_PROFILE_SCOPE("GameMenuComponent::RenderMenu");
    if (active)
    {
        menu.Draw(sprites_,title.c_str(),firstLabel.c_str(),false,secondLabel.c_str());
    }
}

void GameMenuComponent::Gui()
{
    ImGui::InputText("Title",&title);
    ImGui::InputText("First Label",&firstLabel);
    ImGui::InputText("Second Label",&secondLabel);
    ImGui::InputText("First Action",&firstAction);
    ImGui::InputText("Second Action",&secondAction);
    ImGui::Checkbox("Pause World",&pauseWorld);
    ImGui::InputText("Toggle Input Action", &toggleInputAction);
    if (ImGui::Button("Open Menu"))
    {
        Open();
    }
    ImGui::SameLine();
    if (ImGui::Button("Close Menu"))
    {
        Close();
    }
}

void GameMenuComponent::Serialize(nlohmann::json& json_) const
{
    json_ = {{"Title",title},{"FirstLabel",firstLabel},{"SecondLabel",secondLabel},{"FirstAction",firstAction},{"SecondAction",secondAction},{"PauseWorld",pauseWorld},{"ToggleInputAction",toggleInputAction}};
}

void GameMenuComponent::Deserialize(const nlohmann::json& json_)
{
    title = json_.value("Title",std::string("PAUSED"));
    firstLabel = json_.value("FirstLabel",std::string("再開"));
    secondLabel = json_.value("SecondLabel",std::string("タイトルに戻る"));
    firstAction = json_.value("FirstAction",std::string());
    secondAction = json_.value("SecondAction",std::string("GoSceneTitle"));
    pauseWorld = json_.value("PauseWorld",true);
    toggleInputAction = json_.value("ToggleInputAction", std::string("UI.ToggleGameMenu"));
}

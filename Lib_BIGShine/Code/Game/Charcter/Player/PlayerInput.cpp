#include "PlayerInput.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "PlayerAttack.h"
#include <algorithm>
#include <cmath>
#include <DirectXMath.h>
#include "Engine/GamePlay/Transform/Transform.h"
#include "Engine/GamePlay/Core/Component/Movement.h"
#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"
#include <Windows.h>
#include "Engine//GamePlay/Camera/CameraManager.h"

using namespace DirectX;

void PlayerInput::Start()
{
    SetupInputMap();
    BindDefaultActions();

    auto obj_ = GetGameObject();
    mainCamera = obj_->GetManager().GetCameraManager()->GetActiveCamera()->gameObject.lock();

    RegisterComboAction(PLAYER_ACTION::ATTACK, 1);
}

void PlayerInput::Update(float deltaTime_)
{
    static_cast<void>(deltaTime_);

    auto obj_ = GetGameObject();
    auto move_ = obj_->GetComponent<Movement>();
    if (!move_) return;

    Vector3 moveDir_ = CalculateCameraRelativeInput();
    move_->SetVelocity(moveDir_);
}

void PlayerInput::SetupInputMap()
{
    inputMap[PLAYER_ACTION::MOVE_FORWARD] = "MoveForward";
    inputMap[PLAYER_ACTION::MOVE_BACKWARD] = "MoveBackward";
    inputMap[PLAYER_ACTION::MOVE_LEFT] = "MoveLeft";
    inputMap[PLAYER_ACTION::MOVE_RIGHT] = "MoveRight";
    inputMap[PLAYER_ACTION::ATTACK] = "Attack";
}

void PlayerInput::BindDefaultActions()
{
    auto& config_ = KeyConfig::Instance();

    for (const auto& inputPair_ : inputMap)
    {
        config_.ClearBinds(inputPair_.second);
    }

    BindAction(PLAYER_ACTION::MOVE_FORWARD, { { DeviceType::KBM, 'W' } });
    BindAction(PLAYER_ACTION::MOVE_BACKWARD, { { DeviceType::KBM, 'S' } });
    BindAction(PLAYER_ACTION::MOVE_LEFT, { { DeviceType::KBM, 'A' } });
    BindAction(PLAYER_ACTION::MOVE_RIGHT, { { DeviceType::KBM, 'D' } });

    BindAction(PLAYER_ACTION::MOVE_FORWARD, { { DeviceType::Gamepad, static_cast<int>(PadCode::StickL_Up) } });
    BindAction(PLAYER_ACTION::MOVE_BACKWARD, { { DeviceType::Gamepad, static_cast<int>(PadCode::StickL_Down) } });
    BindAction(PLAYER_ACTION::MOVE_LEFT, { { DeviceType::Gamepad, static_cast<int>(PadCode::StickL_Left) } });
    BindAction(PLAYER_ACTION::MOVE_RIGHT, { { DeviceType::Gamepad, static_cast<int>(PadCode::StickL_Right) } });

    BindAction(PLAYER_ACTION::ATTACK, { { DeviceType::KBM, 'F' } });
    BindAction(PLAYER_ACTION::ATTACK, { { DeviceType::KBM, 0x01 } });
    BindAction(PLAYER_ACTION::ATTACK, { { DeviceType::Gamepad, static_cast<int>(PadCode::X) } });
}

void PlayerInput::BindAction(PLAYER_ACTION action_, const InputCombo& combo_)
{
    KeyConfig::Instance().BindAction(GetActionName(action_), combo_);
}

bool PlayerInput::ActionPressed(PLAYER_ACTION action_) const
{
    return KeyConfig::Instance().ActionPressed(GetActionName(action_));
}

bool PlayerInput::ActionPressedIn(PLAYER_ACTION action_) const
{
    return KeyConfig::Instance().ActionPressedIn(GetActionName(action_));
}

bool PlayerInput::ActionPressedOut(PLAYER_ACTION action_) const
{
    return KeyConfig::Instance().ActionPressedOut(GetActionName(action_));
}

const std::string& PlayerInput::GetActionName(PLAYER_ACTION action_) const
{
    auto it_ = inputMap.find(action_);
    if (it_ != inputMap.end()) return it_->second;

    static const std::string emptyActionName_ = "";
    return emptyActionName_;
}

Vector3 PlayerInput::CalculateCameraRelativeInput()
{
    Vector3 rawInput_{ 0, 0, 0 };

    if (ActionPressed(PLAYER_ACTION::MOVE_FORWARD)) rawInput_.z += 1.0f;
    if (ActionPressed(PLAYER_ACTION::MOVE_BACKWARD)) rawInput_.z -= 1.0f;
    if (ActionPressed(PLAYER_ACTION::MOVE_LEFT)) rawInput_.x -= 1.0f;
    if (ActionPressed(PLAYER_ACTION::MOVE_RIGHT)) rawInput_.x += 1.0f;

    if (rawInput_.LengthSquared() < 0.001f) return { 0, 0, 0 };
    rawInput_.Normalize();

    if (auto camPtr_ = mainCamera.lock())
    {
        Vector3 camFront_ = camPtr_->transform->GetFront();
        camFront_.y = 0.0f;
        camFront_.Normalize();

        Vector3 camRight_ = camPtr_->transform->GetRight();
        camRight_.y = 0.0f;
        camRight_.Normalize();

        return (camFront_ * rawInput_.z) + (camRight_ * rawInput_.x);
    }

    return rawInput_;
}

void PlayerInput::RegisterComboAction(PLAYER_ACTION action_, int nextAnimIndex_)
{
    auto obj_ = GetGameObject();
    auto& localReg_ = obj_->localRegistry;
    const std::string cat_ = "InputWindow";
    const std::string& actionName_ = GetActionName(action_);

    std::string startLabel_ = actionName_ + "_Start";
    std::string updateLabel_ = actionName_ + "_Update";
    std::string endLabel_ = actionName_ + "_End";

    localReg_.RegisterAction(cat_, startLabel_, [this, action_]() {
        this->ClearInput(action_);
        });

    localReg_.RegisterAction(cat_, updateLabel_, [this, action_]() {
        this->RecordInput(action_);
        });

    localReg_.RegisterAction(cat_, endLabel_, [this, action_, nextAnimIndex_]() {
        static_cast<void>(nextAnimIndex_);
        if (this->HasInput(action_)) {
            this->ClearInput(action_);
        }
        });
}

void PlayerInput::Gui()
{
    ImGui::Text("Key Config (Action Status)");

    auto drawStatus_ = [this](PLAYER_ACTION action_) {
        const std::string& label_ = GetActionName(action_);
        bool active_ = ActionPressed(action_);
        ImGui::Selectable(label_.c_str(), active_);
        };

    drawStatus_(PLAYER_ACTION::MOVE_FORWARD);
    drawStatus_(PLAYER_ACTION::MOVE_BACKWARD);
    drawStatus_(PLAYER_ACTION::MOVE_LEFT);
    drawStatus_(PLAYER_ACTION::MOVE_RIGHT);
    drawStatus_(PLAYER_ACTION::ATTACK);
}

void PlayerInput::RecordInput(const std::string& actionName_)
{
    if (KeyConfig::Instance().ActionPressedIn(actionName_)) {
        m_inputFlags[actionName_] = true;
    }
}

void PlayerInput::RecordInput(PLAYER_ACTION action_)
{
    RecordInput(GetActionName(action_));
}

void PlayerInput::ClearInput(const std::string& actionName_)
{
    m_inputFlags[actionName_] = false;
}

void PlayerInput::ClearInput(PLAYER_ACTION action_)
{
    ClearInput(GetActionName(action_));
}

bool PlayerInput::HasInput(const std::string& actionName_)
{
    return m_inputFlags[actionName_];
}

bool PlayerInput::HasInput(PLAYER_ACTION action_)
{
    return HasInput(GetActionName(action_));
}

#include "PlayerInput.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "PlayerAttack.h"
#include <algorithm>
#include <cmath>
#include <DirectXMath.h>
#include "Engine/GamePlay/Transform/Transform.h"
#include "Movement.h"
#include <Windows.h>
#include <AnimationController.h>

using namespace DirectX;

void PlayerInput::Start()
{
    auto& config = KeyConfig::Instance();

    // --- 移動 (Keyboard) ---
    config.BindAction("MoveForward", { { DeviceType::KBM, 'W' } });
    config.BindAction("MoveBackward", { { DeviceType::KBM, 'S' } });
    config.BindAction("MoveLeft", { { DeviceType::KBM, 'A' } });
    config.BindAction("MoveRight", { { DeviceType::KBM, 'D' } });

    // --- 移動 (Gamepad スティック倒し判定) ---
    // 先ほど PadCode に追加した StickL_Up などを利用します
    config.BindAction("MoveForward", { { DeviceType::Gamepad, (int)PadCode::StickL_Up } });
    config.BindAction("MoveBackward", { { DeviceType::Gamepad, (int)PadCode::StickL_Down } });
    config.BindAction("MoveLeft", { { DeviceType::Gamepad, (int)PadCode::StickL_Left } });
    config.BindAction("MoveRight", { { DeviceType::Gamepad, (int)PadCode::StickL_Right } });

    // --- アクション ---
    // Attack: Fキー または マウス左クリック
    config.BindAction("Attack", { { DeviceType::KBM, 'F' } });
    config.BindAction("Attack", { { DeviceType::KBM, 0x01 } }); // Mouse Left

    // Attack: ゲームパッドの Xボタン (PSの□)
    config.BindAction("Attack", { { DeviceType::Gamepad, (int)PadCode::X } });


    auto obj = GetGameObject();
    auto& localReg = obj->localRegistry;

    // --- 「先行入力」カテゴリとして登録 ---
    const std::string cat = "InputWindow";

    // 攻撃（Attack）用
    RegisterComboAction("Attack", 1);
}


void PlayerInput::Update(float deltaTime)
{
    auto obj = GetGameObject();
    auto move = obj->GetComponent<Movement>();
    if (!move) return;

    // 1. 抽象化された移動入力の計算
    Vector3 moveDir = CalculateCameraRelativeInput();
    move->SetVelocity(moveDir);

    // 2. アクション名による先行入力判定
    auto& config = KeyConfig::Instance();
}


Vector3 PlayerInput::CalculateCameraRelativeInput()
{
    auto& config = KeyConfig::Instance();
    Vector3 rawInput{ 0, 0, 0 };

    // アクション名で判定
    if (config.ActionPressed("MoveForward"))  rawInput.z += 1.0f;
    if (config.ActionPressed("MoveBackward")) rawInput.z -= 1.0f;
    if (config.ActionPressed("MoveLeft"))     rawInput.x -= 1.0f;
    if (config.ActionPressed("MoveRight"))    rawInput.x += 1.0f;

    if (rawInput.LengthSquared() < 0.001f) return { 0, 0, 0 };
    rawInput.Normalize();

    if (auto camPtr = mainCamera.lock())
    {
        Vector3 camFront = camPtr->transform->GetFront();
        camFront.y = 0.0f;
        camFront.Normalize();

        Vector3 camRight = camPtr->transform->GetRight();
        camRight.y = 0.0f;
        camRight.Normalize();

        return (camFront * rawInput.z) + (camRight * rawInput.x);
    }

    return rawInput;
}

void PlayerInput::RegisterComboAction(const std::string& actionName, int nextAnimIndex)
{
    auto obj = GetGameObject();
    auto& localReg = obj->localRegistry;
    const std::string cat = "InputWindow"; // カテゴリは固定

    // ラベル名（イベント側で設定する名前）
    std::string startLabel = actionName + "_Start";
    std::string updateLabel = actionName + "_Update";
    std::string endLabel = actionName + "_End";

    // --- Start: フラグを掃除 ---
    localReg.RegisterAction(cat, startLabel, [this, actionName]() {
        this->ClearInput(actionName);
        });

    // --- Update: 入力を監視して記録 ---
    localReg.RegisterAction(cat, updateLabel, [this, actionName]() {
        this->RecordInput(actionName);
        });

    // --- End: 入力があれば次のアニメへ ---
    localReg.RegisterAction(cat, endLabel, [this, obj, actionName, nextAnimIndex]() {
        if (this->HasInput(actionName)) {
            if (auto anim = obj->GetComponent<AnimationController>()) {
                anim->Play(nextAnimIndex);
            }
            this->ClearInput(actionName);
        }
        });
}


void PlayerInput::Gui()
{
    ImGui::Text("Key Config (Action Status)");
    auto& config = KeyConfig::Instance();

    // デバッグ用に現在の入力状態を可視化
    auto DrawStatus = [&](const char* label) {
        bool active = config.ActionPressed(label);
        ImGui::Selectable(label, active);
        };

    DrawStatus("MoveForward");
    DrawStatus("MoveBackward");
    DrawStatus("MoveLeft");
    DrawStatus("MoveRight");
    DrawStatus("Attack");
}

void PlayerInput::RecordInput(const std::string& actionName)
{
    if (KeyConfig::Instance().ActionPressedIn(actionName)) {
        m_inputFlags[actionName] = true;
    }
}

void PlayerInput::ClearInput(const std::string& actionName)
{
    m_inputFlags[actionName] = false;
}

bool PlayerInput::HasInput(const std::string& actionName)
{
    return m_inputFlags[actionName];
}
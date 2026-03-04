#pragma once
#include <Windows.h>
#include <Xinput.h>
#include <vector>
#include <DirectXMath.h>

#pragma comment(lib, "xinput.lib")

// ボタン定数の抽象化
enum class PadCode {
    A = XINPUT_GAMEPAD_A,             // PS: ×
    B = XINPUT_GAMEPAD_B,             // PS: ○
    X = XINPUT_GAMEPAD_X,             // PS: □
    Y = XINPUT_GAMEPAD_Y,             // PS: △
    L1 = XINPUT_GAMEPAD_LEFT_SHOULDER,
    R1 = XINPUT_GAMEPAD_RIGHT_SHOULDER,
    L3 = XINPUT_GAMEPAD_LEFT_THUMB,
    R3 = XINPUT_GAMEPAD_RIGHT_THUMB,
    Start = XINPUT_GAMEPAD_START,     // PS: Options
    Back = XINPUT_GAMEPAD_BACK,       // PS: Share / Touchpad(Click)
    Up = XINPUT_GAMEPAD_DPAD_UP,
    Down = XINPUT_GAMEPAD_DPAD_DOWN,
    Left = XINPUT_GAMEPAD_DPAD_LEFT,
    Right = XINPUT_GAMEPAD_DPAD_RIGHT,

    // 特殊拡張：XInputの16ビット範囲(0xFFFF)を超えた値を割り当てる
    L2 = 0x10000,
    R2 = 0x20000,
    StickL_Up = 0x40000,
    StickL_Down = 0x80000,
    StickL_Left = 0x100000,
    StickL_Right = 0x200000,
    StickR_Up = 0x400000,
    StickR_Down = 0x800000,
    StickR_Left = 0x1000000,
    StickR_Right = 0x2000000
};

class ControllerInput 
{
public:
    static ControllerInput& Instance() {
        static ControllerInput instance;
        return instance;
    }

    void Update() {
        m_prevState = m_state;
        DWORD result = XInputGetState(0, &m_state); // 1番目のコントローラ
        m_connected = (result == ERROR_SUCCESS);
    }

    bool IsConnected() const { return m_connected; }

    // ボタン判定
    bool ButtonPressed(PadCode code) const;

    bool ButtonPressedIn(PadCode code) const {
        return ButtonPressed(code) && !PrevButtonPressed(code);
    }

    bool ButtonPressedOut(PadCode code) const {
        return !ButtonPressed(code) && PrevButtonPressed(code);
    }

    // 左スティックの傾きを取得 (-1.0f ～ 1.0f)
    DirectX::XMFLOAT2 GetLeftStick() const {
        return CalculateStickValue(m_state.Gamepad.sThumbLX, m_state.Gamepad.sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
    }

    // 右スティックの傾きを取得 (-1.0f ～ 1.0f)
    DirectX::XMFLOAT2 GetRightStick() const {
        return CalculateStickValue(m_state.Gamepad.sThumbRX, m_state.Gamepad.sThumbRY, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
    }

private:
    bool PrevButtonPressed(PadCode code) const {
        if (code == PadCode::L2) return m_prevState.Gamepad.bLeftTrigger > 128;
        if (code == PadCode::R2) return m_prevState.Gamepad.bRightTrigger > 128;
        return (m_prevState.Gamepad.wButtons & (int)code) != 0;
    }

    // スティック計算の共通ロジック
    DirectX::XMFLOAT2 CalculateStickValue(short rawX, short rawY, short deadzoneThreshold) const;

    ControllerInput() : m_connected(false) { ZeroMemory(&m_state, sizeof(XINPUT_STATE)); }
    XINPUT_STATE m_state;
    XINPUT_STATE m_prevState;
    bool m_connected;
};
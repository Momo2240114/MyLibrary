#pragma once

#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_init.h>
#include <DirectXMath.h>

#include <array>
#include <string>


enum class PadCode
{
    // SDLのGamepad入力を、ゲーム側で扱いやすい仮想ボタンへまとめる。
    A = 0x1000,
    B = 0x2000,
    X = 0x4000,
    Y = 0x8000,
    L1 = 0x0100,
    R1 = 0x0200,
    L3 = 0x0040,
    R3 = 0x0080,
    START = 0x0010,
    BACK = 0x0020,
    UP = 0x0001,
    DOWN = 0x0002,
    LEFT = 0x0004,
    RIGHT = 0x0008,
    L2 = 0x10000,
    R2 = 0x20000,
    STICK_L_UP = 0x40000,
    STICK_L_DOWN = 0x80000,
    STICK_L_LEFT = 0x100000,
    STICK_L_RIGHT = 0x200000,
    STICK_R_UP = 0x400000,
    STICK_R_DOWN = 0x800000,
    STICK_R_LEFT = 0x1000000,
    STICK_R_RIGHT = 0x2000000,
    TOUCHPAD = 0x4000000,
    GUIDE = 0x8000000
};

class ControllerInput
{
public:
    // アプリ全体で共有するコントローラー入力管理。
    static ControllerInput& Instance();

    // SDL Gamepadサブシステムを初期化し、接続済みPadがあれば開く。
    bool Initialize();

    // Gamepadを閉じ、初期化状態を解除する。
    void Shutdown();

    // 現在フレームのボタン/スティック状態を取得し、previousStateも更新する。
    void Update();

    bool IsConnected() const { return connected; }
    const std::string& GetName() const { return gamepadName; }

    bool ButtonPressed(PadCode code_) const;

    // 今フレーム押された瞬間だけtrue。
    bool ButtonPressedIn(PadCode code_) const;

    // 今フレーム離された瞬間だけtrue。
    bool ButtonPressedOut(PadCode code_) const;

    // デッドゾーン処理済みの左右スティック値。
    DirectX::XMFLOAT2 GetLeftStick() const;
    DirectX::XMFLOAT2 GetRightStick() const;
    float GetLeftTrigger() const;
    float GetRightTrigger() const;

private:
    struct ControllerState
    {
        // SDLから読んだ生のボタン・軸状態。
        std::array<bool, SDL_GAMEPAD_BUTTON_COUNT> buttons{};
        std::array<Sint16, SDL_GAMEPAD_AXIS_COUNT> axes{};
    };

    ControllerInput() = default;
    ~ControllerInput();
    ControllerInput(const ControllerInput&) = delete;
    ControllerInput& operator=(const ControllerInput&) = delete;

    bool TryOpenGamepad();
    void CloseGamepad();

    // SDLの現在状態をControllerStateへ保存する。
    void CaptureState();

    // PadCodeを実ボタンまたは仮想スティック方向として評価する。
    bool ButtonPressed(const ControllerState& state_, PadCode code_) const;

    // 生のAxis値を -1.0～1.0 に正規化し、デッドゾーンを適用する。
    DirectX::XMFLOAT2 CalculateStickValue(Sint16 rawX_, Sint16 rawY_) const;
    float NormalizeTrigger(Sint16 rawValue_) const;
    SDL_GamepadButton ConvertButton(PadCode code_) const;

    // 現在開いているSDL Gamepad。
    SDL_Gamepad* gamepad = nullptr;

    // 現在フレームと前フレームの入力状態。
    ControllerState state{};
    ControllerState previousState{};
    std::string gamepadName;
    bool initialized = false;
    bool connected = false;
    bool gamepadEventsWereEnabled = false;
    float stickDeadZone = 0.20f;
    float virtualButtonThreshold = 0.50f;
    float triggerThreshold = 0.50f;
};

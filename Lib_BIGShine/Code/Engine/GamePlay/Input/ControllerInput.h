#pragma once

#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_init.h>
#include <DirectXMath.h>

#include <array>
#include <string>


enum class PadCode
{
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
    static ControllerInput& Instance();

    bool Initialize();
    void Shutdown();
    void Update();

    bool IsConnected() const { return connected; }
    const std::string& GetName() const { return gamepadName; }

    bool ButtonPressed(PadCode code_) const;
    bool ButtonPressedIn(PadCode code_) const;
    bool ButtonPressedOut(PadCode code_) const;

    DirectX::XMFLOAT2 GetLeftStick() const;
    DirectX::XMFLOAT2 GetRightStick() const;
    float GetLeftTrigger() const;
    float GetRightTrigger() const;

private:
    struct ControllerState
    {
        std::array<bool, SDL_GAMEPAD_BUTTON_COUNT> buttons{};
        std::array<Sint16, SDL_GAMEPAD_AXIS_COUNT> axes{};
    };

    ControllerInput() = default;
    ~ControllerInput();
    ControllerInput(const ControllerInput&) = delete;
    ControllerInput& operator=(const ControllerInput&) = delete;

    bool TryOpenGamepad();
    void CloseGamepad();
    void CaptureState();
    bool ButtonPressed(const ControllerState& state_, PadCode code_) const;
    DirectX::XMFLOAT2 CalculateStickValue(Sint16 rawX_, Sint16 rawY_) const;
    float NormalizeTrigger(Sint16 rawValue_) const;
    SDL_GamepadButton ConvertButton(PadCode code_) const;

    SDL_Gamepad* gamepad = nullptr;
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

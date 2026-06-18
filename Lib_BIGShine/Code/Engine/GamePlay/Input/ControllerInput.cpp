#include "ControllerInput.h"

#include <algorithm>
#include <cmath>

ControllerInput& ControllerInput::Instance()
{
    static ControllerInput instance_;
    return instance_;
}

ControllerInput::~ControllerInput()
{
    Shutdown();
}

bool ControllerInput::Initialize()
{
    if (initialized)
    {
        return true;
    }

    initialized = SDL_InitSubSystem(SDL_INIT_GAMEPAD);
    if (!initialized)
    {
        return false;
    }

    // This engine reads state every frame instead of consuming SDL events.
    gamepadEventsWereEnabled = SDL_GamepadEventsEnabled();
    SDL_SetGamepadEventsEnabled(false);
    SDL_UpdateGamepads();
    TryOpenGamepad();
    CaptureState();
    previousState = state;
    return true;
}

void ControllerInput::Shutdown()
{
    CloseGamepad();
    state = {};
    previousState = {};

    if (initialized)
    {
        SDL_SetGamepadEventsEnabled(gamepadEventsWereEnabled);
        SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
        initialized = false;
    }
}

void ControllerInput::Update()
{
    previousState = state;

    if (!initialized && !Initialize())
    {
        state = {};
        return;
    }

    SDL_UpdateGamepads();

    if (gamepad != nullptr && !SDL_GamepadConnected(gamepad))
    {
        CloseGamepad();
    }

    if (gamepad == nullptr)
    {
        TryOpenGamepad();
    }

    CaptureState();
}

bool ControllerInput::TryOpenGamepad()
{
    int gamepadCount_ = 0;
    SDL_JoystickID* gamepadIds_ = SDL_GetGamepads(&gamepadCount_);

    if (gamepadIds_ == nullptr || gamepadCount_ == 0)
    {
        SDL_free(gamepadIds_);
        return false;
    }

    for (int index_ = 0; index_ < gamepadCount_; ++index_)
    {
        gamepad = SDL_OpenGamepad(gamepadIds_[index_]);
        if (gamepad != nullptr)
        {
            break;
        }
    }

    SDL_free(gamepadIds_);

    connected = gamepad != nullptr;
    if (connected)
    {
        const char* name_ = SDL_GetGamepadName(gamepad);
        gamepadName = name_ != nullptr ? name_ : "Unknown Gamepad";
    }

    return connected;
}

void ControllerInput::CloseGamepad()
{
    if (gamepad != nullptr)
    {
        SDL_CloseGamepad(gamepad);
        gamepad = nullptr;
    }

    connected = false;
    gamepadName.clear();
}

void ControllerInput::CaptureState()
{
    state = {};
    if (gamepad == nullptr || !SDL_GamepadConnected(gamepad))
    {
        connected = false;
        return;
    }

    connected = true;

    for (int buttonIndex_ = 0; buttonIndex_ < SDL_GAMEPAD_BUTTON_COUNT; ++buttonIndex_)
    {
        state.buttons[buttonIndex_] = SDL_GetGamepadButton(
            gamepad, static_cast<SDL_GamepadButton>(buttonIndex_));
    }

    for (int axisIndex_ = 0; axisIndex_ < SDL_GAMEPAD_AXIS_COUNT; ++axisIndex_)
    {
        state.axes[axisIndex_] = SDL_GetGamepadAxis(
            gamepad, static_cast<SDL_GamepadAxis>(axisIndex_));
    }
}

bool ControllerInput::ButtonPressed(PadCode code_) const
{
    return connected && ButtonPressed(state, code_);
}

bool ControllerInput::ButtonPressedIn(PadCode code_) const
{
    return ButtonPressed(state, code_) && !ButtonPressed(previousState, code_);
}

bool ControllerInput::ButtonPressedOut(PadCode code_) const
{
    return !ButtonPressed(state, code_) && ButtonPressed(previousState, code_);
}

bool ControllerInput::ButtonPressed(const ControllerState& state_, PadCode code_) const
{
    if (code_ == PadCode::L2)
    {
        return NormalizeTrigger(state_.axes[SDL_GAMEPAD_AXIS_LEFT_TRIGGER]) >= triggerThreshold;
    }
    if (code_ == PadCode::R2)
    {
        return NormalizeTrigger(state_.axes[SDL_GAMEPAD_AXIS_RIGHT_TRIGGER]) >= triggerThreshold;
    }

    DirectX::XMFLOAT2 leftStick_ = CalculateStickValue(
        state_.axes[SDL_GAMEPAD_AXIS_LEFTX], state_.axes[SDL_GAMEPAD_AXIS_LEFTY]);
    DirectX::XMFLOAT2 rightStick_ = CalculateStickValue(
        state_.axes[SDL_GAMEPAD_AXIS_RIGHTX], state_.axes[SDL_GAMEPAD_AXIS_RIGHTY]);

    switch (code_)
    {
    case PadCode::STICK_L_UP: return leftStick_.y >= virtualButtonThreshold;
    case PadCode::STICK_L_DOWN: return leftStick_.y <= -virtualButtonThreshold;
    case PadCode::STICK_L_LEFT: return leftStick_.x <= -virtualButtonThreshold;
    case PadCode::STICK_L_RIGHT: return leftStick_.x >= virtualButtonThreshold;
    case PadCode::STICK_R_UP: return rightStick_.y >= virtualButtonThreshold;
    case PadCode::STICK_R_DOWN: return rightStick_.y <= -virtualButtonThreshold;
    case PadCode::STICK_R_LEFT: return rightStick_.x <= -virtualButtonThreshold;
    case PadCode::STICK_R_RIGHT: return rightStick_.x >= virtualButtonThreshold;
    default: break;
    }

    SDL_GamepadButton button_ = ConvertButton(code_);
    return button_ != SDL_GAMEPAD_BUTTON_INVALID && state_.buttons[button_];
}

DirectX::XMFLOAT2 ControllerInput::GetLeftStick() const
{
    if (!connected)
    {
        return {};
    }

    return CalculateStickValue(
        state.axes[SDL_GAMEPAD_AXIS_LEFTX], state.axes[SDL_GAMEPAD_AXIS_LEFTY]);
}

DirectX::XMFLOAT2 ControllerInput::GetRightStick() const
{
    if (!connected)
    {
        return {};
    }

    return CalculateStickValue(
        state.axes[SDL_GAMEPAD_AXIS_RIGHTX], state.axes[SDL_GAMEPAD_AXIS_RIGHTY]);
}

float ControllerInput::GetLeftTrigger() const
{
    return connected ? NormalizeTrigger(state.axes[SDL_GAMEPAD_AXIS_LEFT_TRIGGER]) : 0.0f;
}

float ControllerInput::GetRightTrigger() const
{
    return connected ? NormalizeTrigger(state.axes[SDL_GAMEPAD_AXIS_RIGHT_TRIGGER]) : 0.0f;
}

DirectX::XMFLOAT2 ControllerInput::CalculateStickValue(Sint16 rawX_, Sint16 rawY_) const
{
    constexpr float axisMax_ = 32767.0f;

    float x_ = std::clamp(static_cast<float>(rawX_) / axisMax_, -1.0f, 1.0f);
    float y_ = std::clamp(-static_cast<float>(rawY_) / axisMax_, -1.0f, 1.0f);
    float magnitude_ = std::sqrt(x_ * x_ + y_ * y_);

    if (magnitude_ <= stickDeadZone)
    {
        return {};
    }

    float normalizedMagnitude_ = std::min(
        1.0f, (magnitude_ - stickDeadZone) / (1.0f - stickDeadZone));
    float scale_ = normalizedMagnitude_ / magnitude_;
    return { x_ * scale_, y_ * scale_ };
}

float ControllerInput::NormalizeTrigger(Sint16 rawValue_) const
{
    return std::clamp(static_cast<float>(rawValue_) / 32767.0f, 0.0f, 1.0f);
}

SDL_GamepadButton ControllerInput::ConvertButton(PadCode code_) const
{
    switch (code_)
    {
    case PadCode::A: return SDL_GAMEPAD_BUTTON_SOUTH;
    case PadCode::B: return SDL_GAMEPAD_BUTTON_EAST;
    case PadCode::X: return SDL_GAMEPAD_BUTTON_WEST;
    case PadCode::Y: return SDL_GAMEPAD_BUTTON_NORTH;
    case PadCode::L1: return SDL_GAMEPAD_BUTTON_LEFT_SHOULDER;
    case PadCode::R1: return SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER;
    case PadCode::L3: return SDL_GAMEPAD_BUTTON_LEFT_STICK;
    case PadCode::R3: return SDL_GAMEPAD_BUTTON_RIGHT_STICK;
    case PadCode::START: return SDL_GAMEPAD_BUTTON_START;
    case PadCode::BACK: return SDL_GAMEPAD_BUTTON_BACK;
    case PadCode::UP: return SDL_GAMEPAD_BUTTON_DPAD_UP;
    case PadCode::DOWN: return SDL_GAMEPAD_BUTTON_DPAD_DOWN;
    case PadCode::LEFT: return SDL_GAMEPAD_BUTTON_DPAD_LEFT;
    case PadCode::RIGHT: return SDL_GAMEPAD_BUTTON_DPAD_RIGHT;
    case PadCode::TOUCHPAD: return SDL_GAMEPAD_BUTTON_TOUCHPAD;
    case PadCode::GUIDE: return SDL_GAMEPAD_BUTTON_GUIDE;
    default: return SDL_GAMEPAD_BUTTON_INVALID;
    }
}

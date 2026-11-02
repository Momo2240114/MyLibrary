#include "PrecompiledHeader.h"

#include "ControllerInput.h"
#include "Engine/Debug/TracyProfiler.h"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_timer.h>

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

void ControllerInput::PlayRumble(float lowStrength_, float highStrength_, float duration_, int priority_)
{
    BS_PROFILE_SCOPE("ControllerInput::PlayRumble");
    if (!gamepad || !connected || !std::isfinite(lowStrength_) ||
        !std::isfinite(highStrength_) || !std::isfinite(duration_) || duration_ <= 0.0f) return;
    const Uint64 now_ = SDL_GetTicks();
    if (now_ < rumbleEndTime && priority_ < rumblePriority) return;
    const Uint32 durationMs_ = static_cast<Uint32>(std::clamp(duration_, 0.001f, 10.0f) * 1000.0f);
    if (SDL_RumbleGamepad(gamepad,
        static_cast<Uint16>(std::clamp(lowStrength_, 0.0f, 1.0f) * 65535.0f),
        static_cast<Uint16>(std::clamp(highStrength_, 0.0f, 1.0f) * 65535.0f),
        durationMs_))
    {
        rumbleEndTime = now_ + durationMs_;
        rumblePriority = priority_;
    }
}

void ControllerInput::StopRumble()
{
    rumbleEndTime = 0;
    rumblePriority = 0;
    if (gamepad) SDL_RumbleGamepad(gamepad, 0, 0, 0);
}

bool ControllerInput::Initialize()
{
    // SDLのGamepadサブシステムを必要になった時だけ起動し、最初の接続状態を即キャプチャする。
    if (initialized)
    {
        return true;
    }

    // DualShock 4のUSB/Bluetooth出力をHIDAPIで扱い、振動に必要な拡張レポートを有効にする。
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_ENHANCED_REPORTS, "1");
    initialized = SDL_InitSubSystem(SDL_INIT_GAMEPAD);
    if (!initialized)
    {
        return false;
    }

    gamepadEventsWereEnabled = SDL_GamepadEventsEnabled();
    // 元のイベント有効状態はShutdownで戻すため保持しておく。
    SDL_SetGamepadEventsEnabled(true);
    SDL_PumpEvents();
    SDL_UpdateGamepads();
    TryOpenGamepad();
    CaptureState();
    previousState = state;
    return true;
}

void ControllerInput::Shutdown()
{
    // 開いているGamepadを閉じ、SDLサブシステムと入力状態を初期化前へ戻す。
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
    // 押した瞬間/離した瞬間を判定できるよう、更新前の状態をpreviousへ退避する。
    previousState = state;

    if (!initialized && !Initialize())
    {
        state = {};
        return;
    }

    // Win32側がメッセージループを持つため、SDLの接続イベントはここで明示的に進める。
    SDL_PumpEvents();
    SDL_UpdateGamepads();

    if (gamepad != nullptr && !SDL_GamepadConnected(gamepad))
    {
        // 実行中に抜かれた場合はハンドルを閉じ、次の更新で再接続を試せる状態にする。
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
    // SDLが認識しているGamepad候補を列挙し、最初に開けた1台を現在の入力元として使う。
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
        // デバッグ表示用にデバイス名を保持し、取得できない場合はUnknownとして扱う。
        const char* name_ = SDL_GetGamepadName(gamepad);
        gamepadName = name_ != nullptr ? name_ : "Unknown Gamepad";
        gamepadType = SDL_GetGamepadType(gamepad);
    }

    return connected;
}

void ControllerInput::CloseGamepad()
{
    if (gamepad != nullptr)
    {
        StopRumble();
        SDL_CloseGamepad(gamepad);
        gamepad = nullptr;
    }

    connected = false;
    gamepadName.clear();
    gamepadType = SDL_GAMEPAD_TYPE_UNKNOWN;
}

bool ControllerInput::HasMeaningfulInput() const
{
    BS_PROFILE_SCOPE("ControllerInput::HasMeaningfulInput");
    if (!connected) return false;
    for (std::size_t index_ = 0; index_ < state.buttons.size(); ++index_)
        if (state.buttons[index_] && !previousState.buttons[index_]) return true;
    // 軸の絶対量とフレーム間変化の両方を条件にして、静止中の微小変動を除外する。
    for (std::size_t index_ = 0; index_ < state.axes.size(); ++index_)
    {
        const float value_ = static_cast<float>(state.axes[index_]) / 32767.0f;
        const float previous_ = static_cast<float>(previousState.axes[index_]) / 32767.0f;
        const float threshold_ = index_ >= SDL_GAMEPAD_AXIS_LEFT_TRIGGER ? triggerThreshold : stickDeadZone + 0.08f;
        if (std::abs(value_) > threshold_ && (std::abs(previous_) <= threshold_ || std::abs(value_ - previous_) > 0.04f)) return true;
    }
    return false;
}

void ControllerInput::CaptureState()
{
    // 現在フレームのボタン/軸を丸ごと取り直し、未接続時は空状態にする。
    state = {};
    if (gamepad == nullptr || !SDL_GamepadConnected(gamepad))
    {
        connected = false;
        return;
    }

    connected = true;

    for (int buttonIndex_ = 0; buttonIndex_ < SDL_GAMEPAD_BUTTON_COUNT; ++buttonIndex_)
    {
        // SDLのボタン配列をそのまま保持し、PadCode変換は参照時に行う。
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
    // 現在押されていて前フレーム押されていない状態を「押した瞬間」とする。
    return ButtonPressed(state, code_) && !ButtonPressed(previousState, code_);
}

bool ControllerInput::ButtonPressedOut(PadCode code_) const
{
    // 現在離れていて前フレーム押されていた状態を「離した瞬間」とする。
    return !ButtonPressed(state, code_) && ButtonPressed(previousState, code_);
}

bool ControllerInput::ButtonPressed(const ControllerState& state_, PadCode code_) const
{
    // L2/R2はSDL上では軸なので、しきい値以上を仮想ボタンとして扱う。
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
    // スティック方向もPadCodeとして扱えるよう、一定以上倒した方向を仮想ボタン化する。
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
    // SDLの生値を-1から1へ正規化し、Y軸はゲーム内の上方向が正になるよう反転する。
    float y_ = std::clamp(-static_cast<float>(rawY_) / axisMax_, -1.0f, 1.0f);
    float magnitude_ = std::sqrt(x_ * x_ + y_ * y_);

    if (magnitude_ <= stickDeadZone)
    {
        // デッドゾーン内は完全な0にして、微小なドリフトを移動入力へ伝えない。
        return {};
    }

    float normalizedMagnitude_ = std::min(
        1.0f, (magnitude_ - stickDeadZone) / (1.0f - stickDeadZone));
    // デッドゾーン外の残り範囲を0から1へ再スケールし、倒し量の感触を保つ。
    float scale_ = normalizedMagnitude_ / magnitude_;
    return { x_ * scale_, y_ * scale_ };
}

float ControllerInput::NormalizeTrigger(Sint16 rawValue_) const
{
    return std::clamp(static_cast<float>(rawValue_) / 32767.0f, 0.0f, 1.0f);
}

SDL_GamepadButton ControllerInput::ConvertButton(PadCode code_) const
{
    // Engine側のPadCodeをSDLの標準Gamepadボタンへ変換する。
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

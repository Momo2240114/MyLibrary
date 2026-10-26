#include "PrecompiledHeader.h"

#include "KeyInput.h"
#include "Engine/Debug/TracyProfiler.h"

std::array<bool, 256> KeyInput::oldState{};
std::array<bool, 256> KeyInput::currentState{};

// 判定の呼び出し順や回数によらず、同じフレームでは同じ入力結果を返す。
void KeyInput::Update()
{
    BS_PROFILE_SCOPE("KeyInput::Update");
    oldState = currentState;
    for (int key_ = 0; key_ < static_cast<int>(currentState.size()); ++key_)
    {
        currentState[key_] = (GetAsyncKeyState(key_) & 0x8000) != 0;
    }
}

// 現在フレームでキーが押されているかを取得する。
bool KeyInput::KeyPressed(int key_)
{
    return key_ >= 0 && key_ < static_cast<int>(currentState.size()) && currentState[key_];
}

// 前フレームから押下へ変化した入力を、消費せずに取得する。
bool KeyInput::KeyPressedIn(int key_)
{
    return KeyPressed(key_) && !oldState[key_];
}

// 前フレームから解放へ変化した入力を、消費せずに取得する。
bool KeyInput::KeyPressedOut(int key_)
{
    return key_ >= 0 && key_ < static_cast<int>(currentState.size()) &&
        oldState[key_] && !currentState[key_];
}

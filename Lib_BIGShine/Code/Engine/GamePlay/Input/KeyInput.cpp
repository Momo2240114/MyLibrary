#include "PrecompiledHeader.h"

#include "KeyInput.h"

std::unordered_map<int, bool> KeyInput::oldState;

//キーが押されているかを検知
bool KeyInput::KeyPressed(int key)
{
    return (GetAsyncKeyState(key) & 0x8000) != 0;
}

// 押した瞬間を検知
bool KeyInput::KeyPressedIn(int key) {
    bool current = KeyPressed(key);
    bool prev = oldState[key];
    oldState[key] = current;
    return (!prev && current);
}

// 離した瞬間を検知
bool KeyInput::KeyPressedOut(int key) {
    bool current = KeyPressed(key);
    bool prev = oldState[key];
    oldState[key] = current;
    return (prev && !current);
}



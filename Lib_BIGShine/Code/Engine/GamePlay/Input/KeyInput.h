#pragma once

#include <Windows.h>
#include <array>

// フレームごとのキー状態を保持し、複数の処理へ同じ入力結果を提供する。
class KeyInput
{
public:
    // Scene更新前に全キーを取得し、押下と解放の差分をフレーム単位で確定する。
    static void Update();

    //キーが押されているかの検知
    // 現在フレームでキーが押されている間 true。
    static bool KeyPressed(int key_);
    //キー離された時
    // 前フレームは押されていて、現在フレームで離された瞬間だけ true。
    static bool KeyPressedOut(int key_);
    //キー押された時
    // 前フレームは離されていて、現在フレームで押された瞬間だけ true。
    static bool KeyPressedIn(int key_);

    //使いかた　KeyPut::KeyPressed(KeyPut::Key::K_A)
    //名前の衝突を避けるためK_を使う
    enum Key {
        K_A = 0x41, K_B, K_C, K_D, K_E, K_F, K_G, K_H, K_I,
        K_J, K_K, K_L, K_M, K_N, K_O, K_P, K_Q,
        K_R, K_S, K_T, K_U, K_V, K_W, K_X, K_Y, K_Z,

        K_Num0 = 0x30, K_Num1, K_Num2, K_Num3, K_Num4,
        K_Num5, K_Num6, K_Num7, K_Num8, K_Num9,

        K_F1 = 0x70, K_F2, K_F3, K_F4, K_F5, K_F6,
        K_F7, K_F8, K_F9, K_F10, K_F11, K_F12,
        K_Left = 0x25, K_Up, K_Right, K_Down,

        K_Escape = 0x1B, K_Space = 0x20, K_Enter = 0x0D,
        K_Tab = 0x09, K_Backspace = 0x08,
        K_Shift = 0x10, K_Control = 0x11, K_Alt = 0x12,
        K_Ctrl  = 0xA2,

        K_Delete = 0x2E, K_Insert = 0x2D, K_Home = 0x24,
        K_End = 0x23, K_PageUp = 0x21, K_PageDown = 0x22,

        K_NumPad0 = 0x60, K_NumPad1, K_NumPad2, K_NumPad3,
        K_NumPad4, K_NumPad5, K_NumPad6, K_NumPad7,
        K_NumPad8, K_NumPad9,
        K_NumPadAdd = 0x6B, K_NumPadSubtract = 0x6D,
        K_NumPadMultiply = 0x6A, K_NumPadDivide = 0x6F,
        K_NumPadEnter = 0x0D // 区別したいなら別値を割り振る
    };

    // 全キーのうち、いずれかが押されていればtrue
    static bool AnyKeyPressed()
    {
        for (Key key : allKeys)
        {
            if (KeyPressed(key))
            {
                return true;
            }
        }
        return false;
    }


private:
    // 判定時には更新せず、前回と今回の状態を次のフレームまで保持する。
    static std::array<bool, 256> oldState;
    static std::array<bool, 256> currentState;
    // enum Key に定義された全キーを列挙
    static constexpr Key allKeys[] = {
        K_A,K_B,K_C,K_D,K_E,K_F,K_G,K_H,K_I,K_J,K_K,K_L,K_M,K_N,K_O,K_P,K_Q,
        K_R,K_S,K_T,K_U,K_V,K_W,K_X,K_Y,K_Z,
        K_Num0,K_Num1,K_Num2,K_Num3,K_Num4,K_Num5,K_Num6,K_Num7,K_Num8,K_Num9,
        K_F1,K_F2,K_F3,K_F4,K_F5,K_F6,K_F7,K_F8,K_F9,K_F10,K_F11,K_F12,
        K_Left,K_Up,K_Right,K_Down,
        K_Escape,K_Space,K_Enter,K_Tab,K_Backspace,
        K_Shift,K_Control,K_Alt,
        K_Delete,K_Insert,K_Home,K_End,K_PageUp,K_PageDown,
        K_NumPad0,K_NumPad1,K_NumPad2,K_NumPad3,K_NumPad4,K_NumPad5,K_NumPad6,K_NumPad7,
        K_NumPad8,K_NumPad9,K_NumPadAdd,K_NumPadSubtract,K_NumPadMultiply,K_NumPadDivide,K_NumPadEnter
    };
};


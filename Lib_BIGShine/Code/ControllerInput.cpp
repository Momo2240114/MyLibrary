#include "ControllerInput.h"

bool ControllerInput::ButtonPressed(PadCode code) const
{
    if (!m_connected) return false;

    // トリガー判定
    if (code == PadCode::L2) return m_state.Gamepad.bLeftTrigger > 128;
    if (code == PadCode::R2) return m_state.Gamepad.bRightTrigger > 128;

    // 左スティック仮想ボタン判定
    auto leftStick = GetLeftStick();
    if (code == PadCode::StickL_Up)    return leftStick.y >= 0.5f;
    if (code == PadCode::StickL_Down)  return leftStick.y <= -0.5f;
    if (code == PadCode::StickL_Left)  return leftStick.x <= -0.5f;
    if (code == PadCode::StickL_Right) return leftStick.x >= 0.5f;

    // 右スティック仮想ボタン判定
    auto rightStick = GetRightStick();
    if (code == PadCode::StickR_Up)    return rightStick.y >= 0.5f;
    if (code == PadCode::StickR_Down)  return rightStick.y <= -0.5f;
    if (code == PadCode::StickR_Left)  return rightStick.x <= -0.5f;
    if (code == PadCode::StickR_Right) return rightStick.x >= 0.5f;

    // 通常の物理ボタン判定
    return (m_state.Gamepad.wButtons & (int)code) != 0;
}

DirectX::XMFLOAT2 ControllerInput::CalculateStickValue(short rawX, short rawY, short deadzoneThreshold) const
{
    if (!m_connected) return { 0.0f, 0.0f };

    // short型 (-32768 to 32767) を float に変換
    float x = static_cast<float>(rawX);
    float y = static_cast<float>(rawY);

    // ベクトルの長さを計算
    float magnitude = sqrtf(x * x + y * y);

    // デッドゾーン判定
    if (magnitude < static_cast<float>(deadzoneThreshold)) {
        return { 0.0f, 0.0f };
    }

    // 32767.0f を最大値として正規化し、デッドゾーン分を差し引いてスムーズに繋げる
    // (これにより、デッドゾーンを抜けた瞬間に値が跳ねるのを防ぐ)
    float normalizedMag = fminf(1.0f, (magnitude - deadzoneThreshold) / (32767.0f - deadzoneThreshold));

    return { (x / magnitude) * normalizedMag, (y / magnitude) * normalizedMag };
}
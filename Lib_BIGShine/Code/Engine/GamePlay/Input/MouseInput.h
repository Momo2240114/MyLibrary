#pragma once

#include <Windows.h>
#include <DirectXMath.h>

#include "Engine/Math/Vector.h"

class MouseInput
{
public:
    // シングルトン取得
    static MouseInput& Instance()
    {
        static MouseInput instance;
        return instance;
    }

    // 毎フレーム更新 (WndProc またはフレームループで呼ぶ)
    void Update(HWND hwnd);

    // ==========================
    // クリック情報
    // ==========================
    bool LeftPressed()   const { return m_leftPressed; }
    bool LeftDown()      const { return m_leftDown; }
    bool LeftReleased()  const { return m_leftReleased; }

    bool RightPressed()  const { return m_rightPressed; }
    bool RightDown()     const { return m_rightDown; }
    bool RightReleased() const { return m_rightReleased; }

    bool GetButtonState(int buttonCode, int type) const;

    // ==========================
    // マウス座標取得
    // ==========================
    // Windows API クライアント座標 (左上 0,0)
    POINT GetClientPos() const { return m_clientPos; }

    // DirectX スクリーン座標 (左上 0,0, 右下 width,height)
    DirectX::XMFLOAT2 GetScreenPos() const { return m_screenPos; }

    // ==========================
    //　ホイール処理
    // ==========================

    // フレーム開始で更新される
    int GetWheelDelta() const { return m_wheelDelta; }
    // 公開関数でフレーム内ホイール取得
    int GetWheelDeltaThisFrame() const { return m_wheelDeltaThisFrame; }

    void OnMouseWheel(int delta)
    {
        m_wheelDelta = delta; // WM_MOUSEWHEEL で呼ぶ
    }

    float ConsumeWheelDelta()
    {
        int temp = m_wheelDelta;
        m_wheelDelta = 0; // フレームごとにリセット
        return static_cast<float>(temp);
    }

    //マウスの表示の有無
    void ShowCursor(bool flag)
    {
        ShowCursor(flag);
    }

    Vector2 GetSubMousePos()
    {
        return m_SubMousePos;
    }

    void SetSubMousePos(Vector2 delta)
    {
        m_SubMousePos = delta;
    }

    /*void GetWorldRay(const HWND hwnd);*/
    DirectX::XMFLOAT3 GetRayStart() { return RayStart; }
    DirectX::XMFLOAT3 GetRayEnd() { return RayEnd; }

    //mouseの位置を固定するかどうかのフラグ
    void SetDebugFlag(bool flg) { m_DebugFlag = flg; }

private:

    MouseInput() = default;
    ~MouseInput() = default;

    // 状態
    bool m_leftPressed = false;
    bool m_leftDown = false;
    bool m_leftReleased = false;

    bool m_rightPressed = false;
    bool m_rightDown = false;
    bool m_rightReleased = false;

    int m_wheelDelta = 0;
    int m_wheelDeltaThisFrame = 0;

    // 座標
    POINT m_clientPos = { 0, 0 };
    DirectX::XMFLOAT2 m_screenPos = { 0.0f, 0.0f };

    DirectX::XMFLOAT3 RayStart;
    DirectX::XMFLOAT3 RayEnd;

    // 前フレーム状態
    bool m_prevLeft = false;
    bool m_prevRight = false;

    int width;
    int height;

    Vector2 m_CurrentMousePos;
    Vector2 m_SubMousePos;

    bool m_DebugFlag = false;
};
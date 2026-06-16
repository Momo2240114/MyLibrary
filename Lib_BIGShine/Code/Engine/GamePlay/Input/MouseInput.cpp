#include "MouseInput.h"

#include "DirectXMath.h"
#include"imgui.h"


#include"KeyInput.h"

void MouseInput::Update(HWND hwnd)
{
    // デバッグ切り替え
    if (KeyInput::KeyPressedIn(KeyInput::Key::K_Enter))
    {
        m_DebugFlag = !m_DebugFlag;
    }

    // マウス座標取得
    POINT p_;
    GetCursorPos(&p_);
    ScreenToClient(hwnd, &p_);

    m_clientPos = p_;
    m_screenPos = Vector2(
        static_cast<float>(p_.x),
        static_cast<float>(p_.y)
    );

    // 現在位置保存
    m_CurrentMousePos = m_screenPos;

    // 初回のみ差分を0にする
    if (m_IsFirstMouseUpdate)
    {
        m_SubMousePos = Vector2(0.0f, 0.0f);
        m_IsFirstMouseUpdate = false;
    }
    else
    {
        // 前フレームとの差分
        m_SubMousePos = m_CurrentMousePos - m_PrevMousePos;
    }

    // 次フレーム用に保存
    m_PrevMousePos = m_CurrentMousePos;

    // ウィンドウサイズ取得
    RECT rc_;
    GetClientRect(hwnd, &rc_);

    width = rc_.right - rc_.left;
    height = rc_.bottom - rc_.top;

    // マウスボタン状態
    SHORT leftState_ = GetAsyncKeyState(VK_LBUTTON);
    SHORT rightState_ = GetAsyncKeyState(VK_RBUTTON);

    bool leftNow_ = (leftState_ & 0x8000) != 0;
    bool rightNow_ = (rightState_ & 0x8000) != 0;

    // 左クリック
    m_leftPressed = leftNow_ && !m_prevLeft;
    m_leftDown = leftNow_;
    m_leftReleased = !leftNow_ && m_prevLeft;

    // 右クリック
    m_rightPressed = rightNow_ && !m_prevRight;
    m_rightDown = rightNow_;
    m_rightReleased = !rightNow_ && m_prevRight;

    // 状態保存
    m_prevLeft = leftNow_;
    m_prevRight = rightNow_;

    // ホイール値
    m_wheelDeltaThisFrame = m_wheelDelta;
}

bool MouseInput::GetButtonState(int buttonCode, int type) const
{
    // type: 0=Down(Hold), 1=Pressed(In), 2=Released(Out)
    switch (buttonCode) {
    case 0x01: // VK_LBUTTON
        if (type == 0) return LeftDown();
        if (type == 1) return LeftPressed();
        return LeftReleased();
    case 0x02: // VK_RBUTTON
        if (type == 0) return RightDown();
        if (type == 1) return RightPressed();
        return RightReleased();
        // 必要に応じて中クリック(VK_MBUTTON)なども追加
    }
    return false;
}

//void MouseInput::GetWorldRay(const HWND hwnd)
//{
//    auto* cam = CameraController::GetInstance().GetCamera();
//    if (!cam)return;
//    // マウスカーソル位置の取得
//    POINT cursor;
//    ::GetCursorPos(&cursor);
//    ::ScreenToClient(hwnd, &cursor);
//
//    // 各行列を取得
//    DirectX::XMMATRIX View = DirectX::XMLoadFloat4x4(&CameraController::GetInstance().GetCamera()->GetView());
//    DirectX::XMMATRIX Projection = DirectX::XMLoadFloat4x4(&CameraController::GetInstance().GetCamera()->GetProjection());
//    DirectX::XMMATRIX World = DirectX::XMMatrixIdentity();
//
//    // スクリーン座標の設定
//    m_screenPos.x = static_cast<float>(cursor.x);
//    m_screenPos.y = static_cast<float>(cursor.y);
//
//    // マウスのスクリーン座標 (Z=0.0f はニア平面、Z=1.0f はファーフラスト)
//    DirectX::XMVECTOR ScreenPositionNear = DirectX::XMVectorSet(m_screenPos .x, m_screenPos .y, 0.0f, 1.0f);
//    DirectX::XMVECTOR ScreenPositionFar  = DirectX::XMVectorSet(m_screenPos .x, m_screenPos .y, 1.0f, 1.0f);
//
//    // スクリーン空間からワールド空間への変換 (ニア平面の点)
//    DirectX::XMVECTOR WorldPositionNear = DirectX::XMVector3Unproject(
//        ScreenPositionNear,
//        0.0f, 0.0f, width, height, 0.0f, 1.0f,
//        Projection, View, World
//    );
//
//    // スクリーン空間からワールド空間への変換 (ファーフラストの点)
//    DirectX::XMVECTOR WorldPositionFar = DirectX::XMVector3Unproject(
//        ScreenPositionFar,
//        0.0f, 0.0f, width, height, 0.0f, 1.0f,
//        Projection, View, World
//    );
//
//    // レイの始点と終点を格納
//    DirectX::XMStoreFloat3(&RayStart, WorldPositionNear);
//    DirectX::XMStoreFloat3(&RayEnd, WorldPositionFar);
//}
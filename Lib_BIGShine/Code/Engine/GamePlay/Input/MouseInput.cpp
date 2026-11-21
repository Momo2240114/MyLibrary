#include "PrecompiledHeader.h"

#include "MouseInput.h"

void MouseInput::RegisterRawMouse(HWND hwnd_)
{
    // INPUTSINKを使わず、フォーカスのあるゲームウィンドウだけで受信する。
    const RAWINPUTDEVICE mouse_{ 0x01, 0x02, 0, hwnd_ };
    rawMouseRegistered = RegisterRawInputDevices(&mouse_, 1, sizeof(mouse_)) != FALSE;
}

void MouseInput::OnRawMouseInput(HRAWINPUT input_)
{
    if (!m_IsCursorLocked || !rawMouseRegistered)
    {
        return;
    }

    RAWINPUT raw_{};
    UINT size_ = sizeof(raw_);
    const UINT copied_ = GetRawInputData(input_, RID_INPUT, &raw_, &size_, sizeof(RAWINPUTHEADER));
    if (copied_ == static_cast<UINT>(-1) || raw_.header.dwType != RIM_TYPEMOUSE)
    {
        return;
    }
    if (raw_.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE)
    {
        return;
    }

    pendingRawDelta.x += static_cast<float>(raw_.data.mouse.lLastX);
    pendingRawDelta.y += static_cast<float>(raw_.data.mouse.lLastY);
}

void MouseInput::Update(HWND hwnd_)
{
    RECT rect_{};
    POINT clientPos_{};
    const bool hasPosition_ = GetCursorPos(&clientPos_) && ScreenToClient(hwnd_, &clientPos_);
    const bool canLock_ = m_IsCursorLockEnabled && hasPosition_ &&
        GetForegroundWindow() == hwnd_ && GetFocus() == hwnd_ && !IsIconic(hwnd_) &&
        (GetAsyncKeyState(VK_LWIN) & 0x8000) == 0 && (GetAsyncKeyState(VK_RWIN) & 0x8000) == 0 &&
        GetClientRect(hwnd_, &rect_) && rect_.right > 0 && rect_.bottom > 0;

    const POINT clientCenter_{ rect_.right / 2, rect_.bottom / 2 };
    POINT screenCenter_ = clientCenter_;
    const bool wasLocked_ = m_IsCursorLocked;
    const bool locked_ = canLock_ && ClientToScreen(hwnd_, &screenCenter_) &&
        GetForegroundWindow() == hwnd_ && SetCursorPos(screenCenter_.x, screenCenter_.y);

    if (locked_)
    {
        // 再捕捉直後の移動と中央に戻す操作はカメラ回転へ渡さない。
        m_SubMousePos = Vector2{};
        if (wasLocked_)
        {
            m_SubMousePos = rawMouseRegistered ? pendingRawDelta : Vector2(
                static_cast<float>(clientPos_.x - clientCenter_.x),
                static_cast<float>(clientPos_.y - clientCenter_.y));
        }
        m_IsCursorLocked = true;
        clientPos_ = clientCenter_;
        SetCursor(nullptr);
    }
    else
    {
        if (wasLocked_)
        {
            ReleaseCursorLock();
        }
        m_SubMousePos = !hasPosition_ || m_IsFirstMouseUpdate ? Vector2{} :
            Vector2(static_cast<float>(clientPos_.x), static_cast<float>(clientPos_.y)) - m_PrevMousePos;
    }

    pendingRawDelta = Vector2{};
    m_IsFirstMouseUpdate = !hasPosition_;
    m_clientPos = clientPos_;
    m_screenPos = { static_cast<float>(clientPos_.x), static_cast<float>(clientPos_.y) };
    m_PrevMousePos = Vector2(m_screenPos);

    // マウスボタン状態
    // 中ボタンとサイドボタンも既存の状態取得経路で一度だけ読み取る。
    previousExtraButtons = extraButtons;
    for (std::size_t index_ = 0; index_ < extraButtons.size(); ++index_)
    {
        extraButtons[index_] = (GetAsyncKeyState(VK_MBUTTON + static_cast<int>(index_)) & 0x8000) != 0;
    }
    SHORT leftState_ = GetAsyncKeyState(VK_LBUTTON);
    SHORT rightState_ = GetAsyncKeyState(VK_RBUTTON);

    bool leftNow_ = (leftState_ & 0x8000) != 0;
    bool rightNow_ = (rightState_ & 0x8000) != 0;

    // 左クリック
    m_leftPressed = leftNow_ && !m_prevLeft && !(locked_ && !wasLocked_);
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
        if (type == 0)
        {
            return LeftDown();
        }
        if (type == 1)
        {
            return LeftPressed();
        }
        return LeftReleased();
    case 0x02: // VK_RBUTTON
        if (type == 0)
        {
            return RightDown();
        }
        if (type == 1)
        {
            return RightPressed();
        }
        return RightReleased();
    case VK_MBUTTON:
    case VK_XBUTTON1:
    case VK_XBUTTON2:
    {
        const auto index_ = static_cast<std::size_t>(buttonCode - VK_MBUTTON);
        if (type == 0)
        {
            return extraButtons[index_];
        }
        if (type == 1)
        {
            return extraButtons[index_] && !previousExtraButtons[index_];
        }
        return !extraButtons[index_] && previousExtraButtons[index_];
    }
    }
    return false;
}

void MouseInput::ReleaseCursorLock()
{
    if (m_IsCursorLocked)
    {
        SetCursor(LoadCursor(nullptr, IDC_ARROW));
    }
    m_IsCursorLocked = false;
    m_IsFirstMouseUpdate = true;
    m_SubMousePos = Vector2{};
    pendingRawDelta = Vector2{};
}

void MouseInput::SetCursorLockEnabled(bool enabled_)
{
    if (m_IsCursorLockEnabled == enabled_)
    {
        return;
    }
    m_IsCursorLockEnabled = enabled_;
    if (!enabled_)
    {
        ReleaseCursorLock();
    }
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

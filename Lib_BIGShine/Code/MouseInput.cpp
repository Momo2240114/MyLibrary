#include "MouseInput.h"

#include "DirectXMath.h"
#include"imgui.h"


#include"KeyInput.h"

void MouseInput::Update(HWND hwnd)
{
    //画面固定をなくす
    if (KeyInput::KeyPressedIn(KeyInput::Key::K_Enter))
    {
        if (m_DebugFlag)
        {
            m_DebugFlag = false;
        }
        else
        {
            m_DebugFlag = true;
        }
    }


    // マウス座標をクライアント座標で取得
    POINT p;
    GetCursorPos(&p);
    ScreenToClient(hwnd, &p);
    m_clientPos = p;
    m_screenPos = DirectX::XMFLOAT2((float)p.x, (float)p.y); // これを外に出す


    // ウィンドウサイズを取得 (DirectX座標系用)
    RECT rc;
    GetClientRect(hwnd, &rc);
    width  = rc.right - rc.left;
    height = rc.bottom - rc.top;

    //POINT型にする
    POINT windowCenter = { (float)width / 2.0f,(float)height / 2.0f };

    //計算しやすいVector2型にする
    Vector2 center = { (float)windowCenter.x,(float)windowCenter.y };

    //クライアント座標をスクリーン座標に変換する
    ClientToScreen(hwnd, &windowCenter);



    //現在のマウスの位置を取得する
    m_CurrentMousePos = m_screenPos;

    // マウスボタン状態
    SHORT leftState = GetAsyncKeyState(VK_LBUTTON);
    SHORT rightState = GetAsyncKeyState(VK_RBUTTON);

    bool leftNow = (leftState & 0x8000) != 0;
    bool rightNow = (rightState & 0x8000) != 0;

    // 左クリック
    m_leftPressed = (leftNow && !m_prevLeft);
    m_leftDown = leftNow;
    m_leftReleased = (!leftNow && m_prevLeft);

    // 右クリック
    m_rightPressed = (rightNow && !m_prevRight);
    m_rightDown = rightNow;
    m_rightReleased = (!rightNow && m_prevRight);

    // 状態保存
    m_prevLeft = leftNow;
    m_prevRight = rightNow;

    m_wheelDeltaThisFrame = m_wheelDelta; // フレーム単位で保持

    //デバッグ状態ではなければマウスカーソルを固定する
    if (!m_DebugFlag)
    {
        //マウスの差分を求める
        m_SubMousePos = m_CurrentMousePos - center;

        SetCursorPos(windowCenter.x, windowCenter.y);

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
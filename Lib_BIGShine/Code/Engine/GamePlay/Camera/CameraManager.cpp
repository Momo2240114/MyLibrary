#include "CameraManager.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"

using namespace DirectX;

void CameraManager::Initialize()
{
    // マネージャー生成時に、レンダリング用のダミーオブジェクトを作成
    m_proxyObject = std::make_shared<GameObject>("CameraProxy");
    m_proxyCamera = m_proxyObject->AddComponent<CameraComponent>();
}


void CameraManager::Update(float et, const std::vector<std::shared_ptr<GameObject>>& _Objects) 
{
    // 1. 優先度探索
    std::shared_ptr<CameraComponent> highestCam = nullptr;
    int highestPriority = -2147483648;

    for (auto& obj : _Objects) {
        if (!obj) continue;
        auto cam = obj->GetComponent<CameraComponent>();
        if (cam && cam->IsEnabled()) {
            if (cam->GetPriority() >= highestPriority) {
                highestPriority = cam->GetPriority();
                highestCam = cam;
            }
        }
    }

    // 切り替え判定
    if (highestCam != m_activeCameraComponent && highestCam != m_targetCamera) {
        StartTransition(highestCam);
    }

    // 遷移中の補間処理
    if (m_isTransitioning)
    {
        m_transitionTimer += et;
        float t = m_transitionTimer / m_transitionDuration;


        // 1. 始点と終点の位置・回転を取得
        XMMATRIX startView = XMLoadFloat4x4(&m_startViewMatrix);
        XMMATRIX endView = m_targetCamera->GetViewProjection();

        // 2. 視界を滑らかに補間 (Slerp等を用いて行列をブレンド)
        // 実務的には位置(Vector)と回転(Quaternion)を分けて補間し、行列を再構築するのがベストだ
        XMVECTOR startPos, startRotQ, startScale;
        XMVECTOR endPos, endRotQ, endScale;
        XMMatrixDecompose(&startScale, &startRotQ, &startPos, startView);
        XMMatrixDecompose(&endScale, &endRotQ, &endPos, endView);

        XMVECTOR currentPos = XMVectorLerp(startPos, endPos, t);
        XMVECTOR currentRot = XMQuaternionSlerp(startRotQ, endRotQ, t);

        // 3. 現在のビュー行列を更新
        XMMATRIX currentView = XMMatrixAffineTransformation(startScale, XMVectorZero(), currentRot, currentPos);
        XMStoreFloat4x4(&m_currentView, currentView);
        // プロキシに行列をセット！
        m_proxyCamera->SetViewMatrix(m_currentView);

        if (t >= 1.0f) {
            m_activeCameraComponent = m_targetCamera;
            m_isTransitioning = false;
        }
    }
    else
    {
        // 通常時
        m_activeCameraComponent = highestCam;

        // ★通常時はプロキシを本物と同期させる
        if (m_activeCameraComponent) {
            m_proxyCamera->SetViewMatrix(m_activeCameraComponent->GetView());
            m_proxyCamera->SetPerspectiveFov(
                m_activeCameraComponent->GetFovY(),
                m_activeCameraComponent->GetAspect(),
                m_activeCameraComponent->GetNear(),
                m_activeCameraComponent->GetFar()
            );
        }
    }
}

std::shared_ptr<CameraComponent> CameraManager::GetActiveCamera() const
{
    return  m_proxyCamera;
}

void CameraManager::StartTransition(std::shared_ptr<CameraComponent> nextCam)
{
    if (!nextCam) return;

    m_targetCamera = nextCam;
    m_transitionTimer = 0.0f;
    m_isTransitioning = true;

    // 現在の視点を始点として保存
    if (m_activeCameraComponent) {
        m_startViewMatrix = m_activeCameraComponent->GetView();
    }
    else {
        // もし最初からカメラがなかった場合などは、今のカメラの行列をそのまま入れる
        m_startViewMatrix = nextCam->GetView();
    }
}
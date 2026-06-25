#include "CameraManager.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"

using namespace DirectX;

void CameraManager::Initialize()
{
    // マネージャー生成時に、レンダリング用のダミーオブジェクトを作成
    m_proxyObject = std::make_shared<GameObject>("CameraProxy");
    m_proxyCamera = m_proxyObject->AddComponent<CameraComponent>();
}


void CameraManager::Update(float et_, const std::vector<std::shared_ptr<GameObject>>& objects_)
{
    // 1. 優先度探索
    std::shared_ptr<CameraComponent> highestCam_ = nullptr;
    int highestPriority_ = -2147483648;

    for (auto& obj : objects_) {
        if (!obj) continue;
        auto cam = obj->GetComponent<CameraComponent>();
        if (cam && cam->IsEnabled()) {
            if (cam->GetPriority() >= highestPriority_) {
                highestPriority_ = cam->GetPriority();
                highestCam_ = cam;
            }
        }
    }

    // 切り替え判定
    if (highestCam_ != activeCameraComponent && highestCam_ != targetCamera) {
        StartTransition(highestCam_);
    }

    // 遷移中の補間処理
    if (isTransitioning)
    {
        transitionTimer += et_;
        float t_ = transitionTimer / transitionDuration;


        // 1. 始点と終点の位置・回転を取得
        XMMATRIX startView_ = XMLoadFloat4x4(&startViewMatrix);
        XMMATRIX endView_ = targetCamera->GetViewProjection();

        // 2. 視界を滑らかに補間 (Slerp等を用いて行列をブレンド)
        // 実務的には位置(Vector)と回転(Quaternion)を分けて補間し、行列を再構築するのがベストだ
        XMVECTOR startPos_, startRotQ_, startScale_;
        XMVECTOR endPos_, endRotQ_, endScale_;
        XMMatrixDecompose(&startScale_, &startRotQ_, &startPos_, startView_);
        XMMatrixDecompose(&endScale_, &endRotQ_, &endPos_, endView_);

        XMVECTOR currentPos_ = XMVectorLerp(startPos_, endPos_, t_);
        XMVECTOR currentRot_ = XMQuaternionSlerp(startRotQ_, endRotQ_, t_);

        // 3. 現在のビュー行列を更新
        XMMATRIX currentView_ = XMMatrixAffineTransformation(startScale_, XMVectorZero(), currentRot_, currentPos_);
        XMStoreFloat4x4(&currentView, currentView_);
        // プロキシに行列をセット！
        m_proxyCamera->SetViewMatrix(currentView);

        if (t_ >= 1.0f) {
            activeCameraComponent = targetCamera;
            isTransitioning = false;
        }
    }
    else
    {
        // 通常時
        activeCameraComponent = highestCam_;

        // ★通常時はプロキシを本物と同期させる
        if (activeCameraComponent) {
            m_proxyCamera->CopyFrom(activeCameraComponent.get());
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

    targetCamera = nextCam;
    transitionTimer = 0.0f;
    isTransitioning = true;

    // 現在の視点を始点として保存
    if (activeCameraComponent) {
        startViewMatrix = activeCameraComponent->GetView();
    }
    else {
        // もし最初からカメラがなかった場合などは、今のカメラの行列をそのまま入れる
        startViewMatrix = nextCam->GetView();
    }
}


#include "PrecompiledHeader.h"

#include "CameraManager.h"
#include "Engine/Debug/TracyProfiler.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"

using namespace DirectX;

void CameraManager::Initialize()
{
    // マネージャー生成時に、レンダリング用のダミーオブジェクトを作成
    m_proxyObject = std::make_shared<GameObject>("CameraProxy");
    m_proxyCamera = m_proxyObject->AddComponent<CameraComponent>();
    m_proxyCamera->SetPerspectiveFov(60.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
    m_proxyCamera->SetLookAt(
        { 0.0f, 1.0f, -5.0f },
        { 0.0f, 1.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f });

    // Editor用の別視点カメラはScene内CameraComponentの優先度や遷移へ参加させず、Managerが単独で所有する。
    editorCameraObject = std::make_shared<GameObject>("EditorSceneViewCamera");
    editorCameraComponent = editorCameraObject->AddComponent<CameraComponent>();
    editorCameraComponent->SetPerspectiveFov(45.0f, 16.0f / 9.0f, 0.01f, 1000.0f);
    editorCameraComponent->SetLookAt(
        { 0.0f, 2.0f, -8.0f },
        { 0.0f, 1.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f });
}


void CameraManager::ResetRuntimeState()
{
    BS_PROFILE_SCOPE("CameraManager::ResetRuntimeState");
    activeCameraComponent.reset();
    targetCamera.reset();
    isTransitioning = false;
    transitionTimer = 0.0f;
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
        // 優先度切り替えではView同士を補間し、Projectionを姿勢の分解へ混ぜない。
        XMMATRIX endView_ = XMLoadFloat4x4(&targetCamera->GetView());

        // 2. 視界を滑らかに補間 (Slerp等を用いて行列をブレンド)
        // 実務的には位置(Vector)と回転(Quaternion)を分けて補間し、行列を再構築するのがベスト
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


        // ★FOVも補間してプロキシに反映
        float currentFov_ = startFov + (endFov - startFov) * t_;
        m_proxyCamera->SetFov(currentFov_);

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

std::shared_ptr<CameraComponent> CameraManager::GetSourceCamera() const
{
    return isTransitioning && targetCamera ? targetCamera : activeCameraComponent;
}

void CameraManager::CopyActiveCameraToEditorCamera()
{
    if (!editorCameraComponent) return;

    const std::shared_ptr<CameraComponent> sourceCamera_ = GetActiveCamera();
    if (!sourceCamera_) return;

    // Runtime側のCameraComponentは変更せず、Editor用Cameraへ現在の視点だけ複製する。
    editorCameraComponent->CopyFrom(sourceCamera_.get());
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
        startFov = activeCameraComponent->GetFovY();
    }
    else {
        // もし最初からカメラがなかった場合などは、今のカメラの行列をそのまま入れる
        startViewMatrix = nextCam->GetView();
        startFov = nextCam->GetFovY();
    }

    endFov = nextCam->GetFovY();
}

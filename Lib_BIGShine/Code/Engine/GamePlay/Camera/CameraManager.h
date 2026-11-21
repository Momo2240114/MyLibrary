#pragma once
#include <memory>
#include <vector>
#include <DirectXMath.h>
#include "CameraComponent.h"

class CameraManager {
public:
    CameraManager() = default;
    void Initialize();
    // 編集カメラを保持し、古い実行カメラへの参照と補間状態を破棄する。
    void ResetRuntimeState();
    // 全カメラを探索し、最も優先度の高いものを決定
    void Update(float et_, const std::vector<std::shared_ptr<GameObject>>& objects_);

    // レンダリングで呼び出す「今使うべきカメラ」
    std::shared_ptr<CameraComponent> GetActiveCamera() const;
    std::shared_ptr<CameraComponent> GetSourceCamera() const;

    std::shared_ptr<CameraComponent> GetEditorCamera() const
    {
        return editorCameraComponent;
    }

    void CopyActiveCameraToEditorCamera();

private:
    void StartTransition(std::shared_ptr<CameraComponent> nextCam);

    std::shared_ptr<CameraComponent> activeCameraComponent; // 最終的な視点
    std::shared_ptr<CameraComponent> targetCamera;          // 遷移の目的地

    // マネージャーが所有する「プロキシ（代理）オブジェクト」
    std::shared_ptr<GameObject> m_proxyObject;
    // そのオブジェクトに紐づくコンポーネント（これらをレンダラーに渡す）
    std::shared_ptr<CameraComponent> m_proxyCamera;

    // Editor専用SceneViewで使うランタイム非干渉のカメラ。
    std::shared_ptr<GameObject> editorCameraObject;
    std::shared_ptr<CameraComponent> editorCameraComponent;

    // 遷移用パラメータ
    bool isTransitioning = false;
    float transitionTimer = 0.0f;
    float transitionDuration = 1.0f;

    float startFov = 0.0f;
    float endFov = 0.0f;

    // 補間計算用の行列キャッシュ
    DirectX::XMFLOAT4X4 startViewMatrix;    // 遷移開始時のビュー行列（始点）
    DirectX::XMFLOAT4X4 currentView;        // 補間計算中の現在のビュー行列
};

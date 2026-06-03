#pragma once
#include <memory>
#include <vector>
#include <DirectXMath.h>
#include "CameraComponent.h"

class CameraManager {
public:
    CameraManager() = default;
    void Initialize();
    // 全カメラを探索し、最も優先度の高いものを決定
    void Update(float et, const std::vector<std::shared_ptr<GameObject>>& objects);

    // レンダリングで呼び出す「今使うべきカメラ」
    std::shared_ptr<CameraComponent> GetActiveCamera() const;

private:
    void StartTransition(std::shared_ptr<CameraComponent> nextCam);

    std::shared_ptr<CameraComponent> m_activeCameraComponent; // 最終的な視点
    std::shared_ptr<CameraComponent> m_targetCamera;          // 遷移の目的地

    // マネージャーが所有する「プロキシ（代理）オブジェクト」
    std::shared_ptr<GameObject> m_proxyObject;
    // そのオブジェクトに紐づくコンポーネント（これらをレンダラーに渡す）
    std::shared_ptr<CameraComponent> m_proxyCamera;

    // 遷移用パラメータ
    bool m_isTransitioning = false;
    float m_transitionTimer = 0.0f;
    float m_transitionDuration = 1.0f;

    // 補間計算用の行列キャッシュ
    DirectX::XMFLOAT4X4 m_startViewMatrix;    // 遷移開始時のビュー行列（始点）
    DirectX::XMFLOAT4X4 m_currentView;        // 補間計算中の現在のビュー行列
};
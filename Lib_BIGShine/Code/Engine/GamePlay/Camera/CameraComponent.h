#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"

// ===========================
// CameraComponent
// ===========================
class CameraComponent : public Component
{
public:
    // =====================
    // コンストラクタ
    // =====================
    CameraComponent() = default;

    // カメラの演出用優先度を切り替えるActionを登録する。
    void Start() override;

    // 指定した優先度の間だけ、このカメラの演出Actionを許可する。
    bool CanPlayEventAction() const
    {
        return !useEventPriority || GetPriority() == eventPriority;
    }

    void ScreenToWorld(const Vector2& screen, Vector3& world);

    void WorldToScreen(Vector2& screen, const Vector3& world);


    virtual const std::string& GetName() const override
    {
        static std::string name = "CameraComponent";
        return name;
    }

    // =====================
    // LookAt 設定
    // ============================
    void SetLookAt(const DirectX::XMFLOAT3& eye_, const DirectX::XMFLOAT3& focus_, const DirectX::XMFLOAT3& up_)
    {
        eye = eye_;
        focus = focus_;
        up = up_;
        UpdateView();
    }

    // ==============
    // パースペクティブ設定
    // =====================
    void SetPerspectiveFov(float fovY_, float aspect_, float nearZ_, float farZ_)
    {
        fovY = fovY_;
        aspect = aspect_;
        nearZ = nearZ_;
        farZ = farZ_;
        UpdateProjection();
    }

    // =====================
    // 更新
    // =====================
    void LateUpdate(float deltaTime) override
    {
        // カメラの「動き」を確定させる
        UpdateViewProjection(deltaTime);
    }

    //ビュープロジェクションを更新する処理
    void UpdateViewProjection(float deltaTime)
    {
        if (auto t = gameObject.lock()->transform)
        {
            // Transform がある場合、eye と focus を Transform から計算
            DirectX::XMFLOAT3 pos = t->GetWorldPosition();
            eye = pos;

            // デフォルト前方向
            DirectX::XMFLOAT3 defaultFront{ 0,0,1 };
            DirectX::XMMATRIX rot = t->GetWorldMatrix();
            DirectX::XMVECTOR f = DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&defaultFront), rot);
            DirectX::XMStoreFloat3(&front, DirectX::XMVector3Normalize(f));

            // 右方向
            DirectX::XMFLOAT3 defaultRight{ 1,0,0 };
            DirectX::XMVECTOR r = DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&defaultRight), rot);
            DirectX::XMStoreFloat3(&right, DirectX::XMVector3Normalize(r));

            // 注視点 = eye + front
            focus = { eye.x + front.x, eye.y + front.y, eye.z + front.z };

            up = GetGameObject()->transform->GetUp();

            UpdateView();
        }
    }

    void Gui()
    {
        // === Projection Settings ===
        ImGui::Text("Projection Settings");
        bool projChanged = false;
        projChanged |= ImGui::DragFloat("FOV (deg)", &fovY,0.01f, 10.0f, 120.0f, "%.1f");
        projChanged |= ImGui::DragFloat("Aspect", &aspect, 0.001f, 0.1f, 3.0f, "%.2f");
        projChanged |= ImGui::DragFloatRange2("Near/Far", &nearZ, &farZ, 0.001f, 0.001f, 5000.0f, "Near: %.2f", "Far: %.2f");

        ImGui::DragInt("Priority", &priority);
        ImGui::Checkbox("Use Event Priority", &useEventPriority);
        if (useEventPriority)
        {
            ImGui::DragInt("Event Priority", &eventPriority);
        }
        ImGui::Text("Effective Priority: %d", GetPriority());

        ImGui::Separator();

        // === Debug info ===
        ImGui::Text("Front: (%.2f, %.2f, %.2f)", front.x, front.y, front.z);
        ImGui::Text("Right: (%.2f, %.2f, %.2f)", right.x, right.y, right.z);

        if (projChanged)
        {
            UpdateProjection();
        }
    }

    void SetFov(float fov)
    {
        fovY = fov;
        UpdateProjection();
    }

    void Serialize(nlohmann::json& j) const override;
    void Deserialize(const nlohmann::json& j) override;
   

    // =====================
    // ビュー／プロジェクション取得
    // =====================
    const DirectX::XMFLOAT4X4& GetView() const
    {
        return view;
    }

    const DirectX::XMFLOAT4X4& GetProjection() const
    {
        return projection;
    }

    const DirectX::XMFLOAT3& GetEye() const
    {
        return eye;
    }

    const DirectX::XMFLOAT3& GetFocus() const
    {
        return focus;
    }

    const DirectX::XMFLOAT3& GetUp() const
    {
        return up;
    }

    const DirectX::XMFLOAT3& GetFront() const
    {
        return front;
    }

    const DirectX::XMFLOAT3& GetRight() const
    {
        return right;
    }


    float GetNear() const
    {
        return nearZ;
    }

    float GetFar()  const
    {
        return farZ;
    }

    float GetFovY() const
    {
        return fovY;
    }

    float GetAspect() const
    {
        return aspect;
    }

    // ビュー行列とプロジェクション行列を掛け合わせた行列を取得
    DirectX::XMMATRIX GetViewProjection() const
    {
        return DirectX::XMLoadFloat4x4(&view) * DirectX::XMLoadFloat4x4(&projection);
    }

    // 保存用の通常優先度を保ったまま、演出中だけ選択用の優先度を上書きする。
    int GetPriority() const
    {
        return useEventPriority && eventPriorityActive ? eventPriority : priority;
    }

    // 現在の状態
    bool IsTransitioning() const
    {
        return _isMoving;
    }

    // --- 行列を直接上書きするセッター ---
    // これにより、Managerが計算した「補間中の行列」をレンダラーへ渡せる
    void SetViewMatrix(const DirectX::XMFLOAT4X4& view_) {
        view = view_;
    }

    void SetProjectionMatrix(const DirectX::XMFLOAT4X4& proj_) {
        projection = proj_;
    }

    // --- 他のカメラの全状態をコピーする機能 ---
    // 通常時に「最高優先度のカメラ」の値をまるごとクローンする際に便利
    void CopyFrom(const CameraComponent* other) {
        if (!other)
        {
            return;
        }
        eye = other->eye;
        focus = other->focus;
        up = other->up;
        front = other->front;
        right = other->right;
        fovY = other->fovY;
        aspect = other->aspect;
        nearZ = other->nearZ;
        farZ = other->farZ;
        view = other->view;
        projection = other->projection;
    }

private:
    void UpdateView()
    {
        DirectX::XMMATRIX mat = DirectX::XMMatrixLookAtLH(
            DirectX::XMLoadFloat3(&eye),
            DirectX::XMLoadFloat3(&focus),
            DirectX::XMLoadFloat3(&up)
        );
        DirectX::XMStoreFloat4x4(&view, mat);
    }

    void UpdateProjection()
    {
        DirectX::XMMATRIX mat = DirectX::XMMatrixPerspectiveFovLH(
            DirectX::XMConvertToRadians(fovY), aspect, nearZ, farZ
        );
        DirectX::XMStoreFloat4x4(&projection, mat);
    }

private:
    // ビュー／プロジェクション
    DirectX::XMFLOAT4X4 view{};
    DirectX::XMFLOAT4X4 projection{};

    // カメラ位置と方向
    DirectX::XMFLOAT3 eye{ 0,0,-5 };
    DirectX::XMFLOAT3 focus{ 0,0,0 };
    DirectX::XMFLOAT3 up{ 0,1,0 };

    // 前／右方向
    DirectX::XMFLOAT3 front{ 0,0,1 };
    DirectX::XMFLOAT3 right{ 1,0,0 };

    // パース設定
    float fovY = 60.0f;
    float aspect = 16.0f / 9.0f;
    float nearZ = 0.1f;
    float farZ = 1000.0f;
    float transitionSpeed = 2.0f; // 速度
    int transitionType = 0;       // 0: 線形, 1: 緩急あり(EaseIn/Out) など
    bool _isMoving;

    int priority = 0;
    // 通常優先度と演出優先度を分離し、演出中のScene保存で通常値を失わないようにする。
    bool useEventPriority = false;
    int eventPriority = 8;
    bool eventPriorityActive = false; // 実行時だけの状態。Sceneには保存しない。
};


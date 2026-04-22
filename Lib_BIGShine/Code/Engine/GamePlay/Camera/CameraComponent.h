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
    void Update(float deltaTime) override
    {
        UpdateProjection();
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
        bool changed = false;

        // === Projection Settings ===
        ImGui::Text("Projection Settings");
        bool projChanged = false;
        projChanged |= ImGui::DragFloat("FOV (deg)", &fovY,0.01f, 10.0f, 120.0f, "%.1f");
        projChanged |= ImGui::DragFloat("Aspect", &aspect, 0.001f, 0.1f, 3.0f, "%.2f");
        projChanged |= ImGui::DragFloatRange2("Near/Far", &nearZ, &farZ, 0.001f, 0.001f, 5000.0f, "Near: %.2f", "Far: %.2f");


        ImGui::Separator();

        // === Debug info ===
        ImGui::Text("Front: (%.2f, %.2f, %.2f)", front.x, front.y, front.z);
        ImGui::Text("Right: (%.2f, %.2f, %.2f)", right.x, right.y, right.z);


    }

    void SetFov(float fov) { fovY = fov; }

    void Serialize(nlohmann::json& j) const override;
    void Deserialize(const nlohmann::json& j) override;
   

    // =====================
    // ビュー／プロジェクション取得
    // =====================
    const DirectX::XMFLOAT4X4& GetView() const { return view; }
    const DirectX::XMFLOAT4X4& GetProjection() const { return projection; }
    const DirectX::XMFLOAT3& GetEye() const { return eye; }
    const DirectX::XMFLOAT3& GetFocus() const { return focus; }
    const DirectX::XMFLOAT3& GetUp() const { return up; }
    const DirectX::XMFLOAT3& GetFront() const { return front; }
    const DirectX::XMFLOAT3& GetRight() const { return right; }


    float GetNear() const { return nearZ; }
    float GetFar()  const { return farZ; }
    float GetFovY() const { return fovY; }
    float GetAspect() const { return aspect; }
    // ビュー行列とプロジェクション行列を掛け合わせた行列を取得
    DirectX::XMMATRIX GetViewProjection() const
    {
        return DirectX::XMLoadFloat4x4(&view) * DirectX::XMLoadFloat4x4(&projection);
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
};

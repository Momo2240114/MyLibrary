#include "PrecompiledHeader.h"

#include "CameraComponent.h"
#include "Engine/Debug/TracyProfiler.h"

void CameraComponent::Start()
{
    BS_PROFILE_SCOPE("CameraComponent::Start");
    eventPriorityActive = false;
    // 描画用・Editor用カメラはRegistry初期化前に生成されるため、演出用カメラだけActionを登録する。
    if (!useEventPriority) return;
    const auto owner_ = GetGameObject();
    if (!owner_) return;
    const std::weak_ptr<GameObject> weakOwner_ = owner_;
    // Object名付きActionにして、Mainカメラの優先度を変更せず演出へ切り替える。
    owner_->RegisterGlobalAction("Camera", "ActivateEventPriority", [weakOwner_]()
    {
        BS_PROFILE_SCOPE("CameraComponent::ActivateEventPriority");
        const auto owner_ = weakOwner_.lock();
        const auto camera_ = owner_ ? owner_->GetComponent<CameraComponent>() : nullptr;
        if (camera_) camera_->eventPriorityActive = true;
    });
    owner_->RegisterGlobalAction("Camera", "RestorePriority", [weakOwner_]()
    {
        BS_PROFILE_SCOPE("CameraComponent::RestorePriority");
        const auto owner_ = weakOwner_.lock();
        const auto camera_ = owner_ ? owner_->GetComponent<CameraComponent>() : nullptr;
        if (camera_) camera_->eventPriorityActive = false;
    });
}


void CameraComponent::ScreenToWorld(const Vector2& screen, Vector3& world)
{
    //スクリーン座標をワールド座標に変換
    float viewportX = 0.0f;
    float viewportY = 0.0f;

    //画面の幅,高さを固定にする
    float viewportW = 1920;
    float viewportH = 1080;

    float viewportMinZ = 0.0f;
    float ViewportMaxZ = 1.0f;

    Matrix W = DirectX::XMMatrixIdentity();
    Matrix V = view;
    Matrix P = projection;
    //NDC座標からワールド座標へ変換
    Matrix WVP = W * V * P;
    Matrix IWVP = WVP.Invert();

    //スクリーン座標をNDC座標に変換する
    Vector4 ndcPosition =
    {
        2.0f * (screen.x - viewportX) / viewportW - 1.0f,
        1.0f - 2.0f * (screen.y - viewportY) / viewportH,
        1.0f,
        1.0f,
    };

    //NDC座標をワールド座標に変換する
    world = DirectX::XMVector3TransformCoord(ndcPosition,IWVP);
}

void CameraComponent::WorldToScreen(Vector2& screen, const Vector3& world)
{
    //スクリーン座標をワールド座標に変換
    float viewportX = 0.0f;
    float viewportY = 0.0f;

    //画面の幅,高さを固定にする
    float viewportW = 1920;
    float viewportH = 1080;

    float viewportMinZ = 0.0f;
    float viewportMaxZ = 1.0f;

    //ビュープロジェクション行列を求める
    Matrix W = DirectX::XMMatrixIdentity();
    Matrix V = view;
    Matrix P = projection;

    Matrix WVP = W * V * P;

    //NDC座標に変換する
    Vector3 ndcPosition = DirectX::XMVector3TransformCoord(world, WVP);

    //スクリーン座標に変換する
    Vector3 screenPosition;
    screenPosition.x = viewportX + (1.0f + ndcPosition.x) * viewportW * 0.5f;
    screenPosition.y = viewportY + (1.0f - ndcPosition.y) * viewportH * 0.5f;
    screenPosition.z = viewportMinZ + ndcPosition.z * (viewportMaxZ - viewportMinZ);

    screen.x = screenPosition.x;
    screen.y = screenPosition.y;
}                                           

void CameraComponent::Serialize(nlohmann::json& j) const
{
    j["fovY"] = fovY;
    j["aspect"] = aspect;
    j["nearZ"] = nearZ;
    j["farZ"] = farZ;
    // 演出で一時的に選択されていても、JSONにはInspectorの通常優先度だけを保存する。
    j["priority"] = priority;
    j["useEventPriority"] = useEventPriority;
    j["eventPriority"] = eventPriority;
    // 注視点モードを使っている場合のために eye, focus, up も保存
    j["eye"] = { eye.x, eye.y, eye.z };
    j["focus"] = { focus.x, focus.y, focus.z };
    j["up"] = { up.x, up.y, up.z };


}

void CameraComponent::Deserialize(const nlohmann::json& j)
{
    fovY = j.value("fovY", 60.0f);
    aspect = j.value("aspect", 16.0f / 9.0f);
    nearZ = j.value("nearZ", 0.001f);
    farZ = j.value("farZ", 1000.0f);
    priority = j.value("priority", 0);
    eventPriorityActive = false;
    // 旧JSONは優先度条件を無効にして、既存のカメラ演出を維持する。
    useEventPriority = j.value("useEventPriority", false);
    eventPriority = j.value("eventPriority", 8);

    if (j.contains("eye")) {
        eye = { j["eye"][0], j["eye"][1], j["eye"][2] };
        focus = { j["focus"][0], j["focus"][1], j["focus"][2] };
        up = { j["up"][0], j["up"][1], j["up"][2] };
    }

    // ロードした値に基づいて行列を再計算
    UpdateProjection();
    UpdateView();
}


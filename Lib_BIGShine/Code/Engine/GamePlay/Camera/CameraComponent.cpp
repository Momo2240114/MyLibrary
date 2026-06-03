#include "CameraComponent.h"

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
    j["priority"] = priority;
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
    priority = j.value("priority", 0.0f);

    if (j.contains("eye")) {
        eye = { j["eye"][0], j["eye"][1], j["eye"][2] };
        focus = { j["focus"][0], j["focus"][1], j["focus"][2] };
        up = { j["up"][0], j["up"][1], j["up"][2] };
    }

    // ロードした値に基づいて行列を再計算
    UpdateProjection();
    UpdateView();
}
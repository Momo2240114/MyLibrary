#include "LightManager.h"
#include "GraphicsDeviceManager.h"
#include "Engine/GamePlay/Transform/Transform.h"

void  LightComponent::Update(float et)
{
    auto t = gameObject.lock()->transform;
    if (!t) return;

    // 1. 最新のワールド行列を取得 (isDirtyなら内部で再計算される)
    DirectX::XMMATRIX worldMat = t->GetWorldMatrix();

    // 2. オフセット位置をワールド空間へ変換
    // TransformCoordは w=1 として扱うため、移動（Translation）も適用される
    DirectX::XMVECTOR localPos = DirectX::XMLoadFloat3(&offsetPosition);
    DirectX::XMVECTOR worldPos = DirectX::XMVector3TransformCoord(localPos, worldMat);
    DirectX::XMStoreFloat3(&finalWorldPosition, worldPos);

    // 3. 方向の計算 (Directional または Spot)
    if (type == Type::Directional || type == Type::Spot)
    {
        // ライトのローカル回転をクォータニオンに変換
        DirectX::XMVECTOR qLocal = DirectX::XMQuaternionRotationRollPitchYaw(
            DirectX::XMConvertToRadians(localRotation.x),
            DirectX::XMConvertToRadians(localRotation.y),
            DirectX::XMConvertToRadians(localRotation.z)
        );

        // トランスフォームから「ワールド回転」成分だけを抽出
        DirectX::XMVECTOR s, r, pos;
        DirectX::XMMatrixDecompose(&s, &r, &pos, worldMat);

        // 親の回転とライトのローカル回転を合成
        DirectX::XMVECTOR qFinal = DirectX::XMQuaternionMultiply(qLocal, r);

        // 基本となる向き（前方：0, 0, 1）を回転させる
        DirectX::XMVECTOR forward = DirectX::XMVectorSet(0, 0, 1, 0);
        DirectX::XMVECTOR worldDir = DirectX::XMVector3Rotate(forward, qFinal);
        DirectX::XMStoreFloat3(&finalWorldDirection, worldDir);
    }

    // 開発時のみ、投影範囲をワイヤーフレームで表示する
    if (type == Type::Point)
    {
        if (showDebugBounds)
        {
            GraphicsDeviceManager::Instance().GetShapeRendererSystem().DrawSphere(
                finalWorldPosition,
                range,
                DirectX::XMFLOAT4(1, 1, 0, 0.5f) // 半透明の黄色い枠
            );
        }
    }
}
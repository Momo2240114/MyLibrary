#include "PrecompiledHeader.h"

#include "Movement.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"


void Movement::Start()
{
    auto obj = GetGameObject();
    // レジストリに移動停止を登録
    obj->RegisterAction("Movement", "Stop", [this]() {
        this->velocity = { 0, 0, 0 };
        });
}

void Movement::Update(float deltaTime)
{
    auto tf = GetGameObject()->transform;

    // 移動方向のベクトル (Vector3型として扱う)
    Vector3 dir = velocity;
    float lengthSq = dir.LengthSquared();

    if (lengthSq > 0.001f) {
        // 1. 移動の適用
        tf->AddMoveOffset(dir * moveSpeed * deltaTime);

        // 2. 回転の適用
        if (rotationType == RotationType::LookAtVelocity) {
            ApplyRotation(deltaTime);
        }
    }

    // 毎フレームリセット
    velocity = { 0, 0, 0 };
}

void Movement::ApplyRotation(float deltaTime)
{
    auto tf = GetGameObject()->transform;

    // 目標方向（移動方向）
    Vector3 targetDir = velocity;
    targetDir.y = 0; // 地面と平行に回転
    targetDir.Normalize();

    // 現在の向き（Transformの前方ベクトル）
    Vector3 currentFront = tf->GetFront();
    currentFront.y = 0;
    currentFront.Normalize();

    // すでに目標方向を向いているなら何もしない
    if (currentFront.Dot(targetDir) > 0.999f) return;

    // --- クォータニオンによる補間 ---

    // 1. 現在の回転をクォータニオンで取得
    Vector3 curEulerRad = {
        DirectX::XMConvertToRadians(tf->localRotation.x),
        DirectX::XMConvertToRadians(tf->localRotation.y),
        DirectX::XMConvertToRadians(tf->localRotation.z)
    };
    DirectX::XMVECTOR curQuat = DirectX::XMQuaternionRotationRollPitchYawFromVector(curEulerRad);

    // 2. 目標の回転をクォータニオンで作成 (LookAt)
    // DirectXMath には直接的な LookRotation がないので、行列を経由
    DirectX::XMVECTOR up = { 0, 1, 0, 0 };
    DirectX::XMMATRIX lookAtMat = DirectX::XMMatrixLookToLH({ 0,0,0,0 }, targetDir, up);
    // LookToLHは「カメラがそこを見る」行列なので、逆行列にする必要がある場合が多い
    DirectX::XMVECTOR targetQuat = DirectX::XMQuaternionRotationMatrix(DirectX::XMMatrixInverse(nullptr, lookAtMat));

    // 3. 球面線形補間 (Slerp)
    // rotateSpeed * deltaTime が 1.0 に近づくほど速く向く
    float t = (std::min)(rotateSpeed * deltaTime, 1.0f);
    DirectX::XMVECTOR slerpQuat = DirectX::XMQuaternionSlerp(curQuat, targetQuat, t);

    // 4. クォータニオンをオイラー角に戻して反映
    // Vector3::ToEuler() または自前の変換関数を使用
    Vector3 nextEuler = Quaternion(slerpQuat).ToEuler();

    tf->SetRotation({
        DirectX::XMConvertToDegrees(nextEuler.x),
        DirectX::XMConvertToDegrees(nextEuler.y),
        DirectX::XMConvertToDegrees(nextEuler.z)
        });
}

#include "Transform.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/Graphics/Resource/GlTFModel.h"
#include "ModelAttachmentComponent.h"

DirectX::XMFLOAT3 Transform::GetWorldRotation()
{
    using namespace DirectX;

    //セットされていれば
    if (isDirty)
    {
        UpdateWorldMatrix();
    }

    XMVECTOR s, r, t;
    XMMatrixDecompose(&s, &r, &t, worldMatrix);

    // クォータニオンをオイラー角に変換
    DirectX::XMFLOAT4 q;
    XMStoreFloat4(&q, r);

    //YXZ順序
    float xsqr = q.x * q.x;

    // Pitch (Y軸周り) - 最初の回転
    float t0 = 2.0f * (q.w * q.y + q.x * q.z);
    float t1 = 1.0f - 2.0f * (q.y * q.y + xsqr);
    float pitch = atan2f(t0, t1);

    // Roll (X軸周り) - 2番目の回転
    float t2 = 2.0f * (q.w * q.x - q.y * q.z);
    t2 = t2 > 1.0f ? 1.0f : (t2 < -1.0f ? -1.0f : t2);
    float roll = asinf(t2);

    // Yaw (Z軸周り) - 最後の回転
    float t3 = 2.0f * (q.w * q.z + q.x * q.y);
    float t4 = 1.0f - 2.0f * (q.z * q.z + xsqr);
    float yaw = atan2f(t3, t4);


    // ラジアン→度数法に変換（必要なら）
    DirectX::XMFLOAT3 euler = {
        DirectX::XMConvertToDegrees(roll),
        DirectX::XMConvertToDegrees(pitch),
        DirectX::XMConvertToDegrees(yaw)
    };
    return euler;
}

void Transform::ApplyWorldMatrix(const DirectX::XMMATRIX& newWorldMat)
{
    using namespace DirectX;

    XMMATRIX finalLocalMat;

    // 1. 親がいる場合は、(親のワールド逆行列 * 新しいワールド行列) でローカル行列を求める
    if (auto p = parent.lock()) {
        XMVECTOR det;
        XMMATRIX invParent = XMMatrixInverse(&det, p->GetWorldMatrix());
        finalLocalMat = newWorldMat * invParent;
    }
    else {
        finalLocalMat = newWorldMat;
    }

    // 2. 行列を分解
    XMVECTOR s, r, t;
    if (XMMatrixDecompose(&s, &r, &t, finalLocalMat)) {
        // Position
        XMStoreFloat3(&localPosition, t);

        // Scale
        XMStoreFloat3(&localScale, s);

        // Rotation (クォータニオンをオイラー角へ)
        // 既存の GetWorldRotation のロジックを流用して localRotation を更新
        XMFLOAT4 q;
        XMStoreFloat4(&q, r);

        // YXZ順序での計算（Transform内のロジックと合わせる）
        float xsqr = q.x * q.x;
        float t0 = 2.0f * (q.w * q.y + q.x * q.z);
        float t1 = 1.0f - 2.0f * (q.y * q.y + xsqr);
        float pitch = atan2f(t0, t1);

        float t2 = 2.0f * (q.w * q.x - q.y * q.z);
        t2 = t2 > 1.0f ? 1.0f : (t2 < -1.0f ? -1.0f : t2);
        float roll = asinf(t2);

        float t3 = 2.0f * (q.w * q.z + q.x * q.y);
        float t4 = 1.0f - 2.0f * (q.z * q.z + xsqr);
        float yaw = atan2f(t3, t4);

        localRotation = {
            XMConvertToDegrees(roll),
            XMConvertToDegrees(pitch),
            XMConvertToDegrees(yaw)
        };

        MarkDirty();
    }
}

void Transform::SetParentOverride(bool enable, DirectX::XMMATRIX matrix)
{
    m_IsOverriding = enable;
    m_OverrideParentMatrix = matrix;
    MarkDirty();
}

Vector3 Transform::GetFront()
{
    //isDirtyフラグが立っていたらワールド行列を更新する
    if (isDirty)
    {
        UpdateWorldMatrix();
        isDirty = false;
    }

    Matrix mat = worldMatrix;
    return mat.Backward();
}



Vector3 Transform::GetRight()
{
    //isDirtyフラグが立っていたらワールド行列を更新する
    if (isDirty)
    {
        UpdateWorldMatrix();
        isDirty = false;
    }

    Matrix mat = worldMatrix;
    return mat.Right();
}

Vector3 Transform::GetUp()
{
    //isDirtyフラグが立っていたらワールド行列を更新する
    if (isDirty)
    {
        UpdateWorldMatrix();
        isDirty = false;
    }

    Matrix mat = worldMatrix;
    return mat.Up();
}

void Transform::SetParent(std::shared_ptr<Transform> p, std::string nodeName)
{
    // 1. 古い親がいれば、その子供リストから自分を削除
    if (auto currentParent = parent.lock()) {
        auto& c = currentParent->children;
        c.erase(std::remove(c.begin(), c.end(), shared_from_this()), c.end());
    }

    // 2. 新しい親をセット
    parent = p;

    // 3. 新しい親の子供リストに自分を追加
    if (p) {
        p->children.push_back(shared_from_this());
    }

    //アタッチメントを設置する親のグローバル行列が入っているノード名を入れる
    m_ParentNodeName = nodeName;

    MarkDirty();
}

void Transform::UpdateWorldMatrix()
{
    // 位置の更新
    localPosition.x += moveOffset.x;
    localPosition.y += moveOffset.y;
    localPosition.z += moveOffset.z;

    // 回転の更新
    localRotation.x += rotateOffset.x;
    localRotation.y += rotateOffset.y;
    localRotation.z += rotateOffset.z;

    // --- 角度の正規化 (0 ~ 360度に収める) ---
    auto NormalizeAngle = [](float& angle) {
        angle = fmodf(angle, 360.0f);
        if (angle < 0.0f) angle += 360.0f;
        };
    NormalizeAngle(localRotation.x);
    NormalizeAngle(localRotation.y);
    NormalizeAngle(localRotation.z);

    // オフセットを使い切ったのでリセット
    moveOffset = { 0, 0, 0 };
    rotateOffset = { 0, 0, 0 };


    // 変化がない場合は、以降の重い行列計算をスキップ
    if (!isDirty) return;

    // 2. クォータニオンと行列の作成
    DirectX::XMVECTOR q = DirectX::XMQuaternionRotationRollPitchYaw(
        DirectX::XMConvertToRadians(localRotation.x),
        DirectX::XMConvertToRadians(localRotation.y),
        DirectX::XMConvertToRadians(localRotation.z)
    );

    DirectX::XMMATRIX localMat =
        DirectX::XMMatrixScaling(localScale.x, localScale.y, localScale.z) *
        DirectX::XMMatrixRotationQuaternion(q) *
        DirectX::XMMatrixTranslation(localPosition.x, localPosition.y, localPosition.z);

    // 2. 親行列の適用
    // もし親が設定されており、かつ親のノードが指定されていれば、その行列を親とする
    if (m_IsOverriding) {
        worldMatrix = localMat * m_OverrideParentMatrix;
    }
    else if (auto p = parent.lock()) {
        worldMatrix = localMat * p->GetWorldMatrix();
    }
    else {
        worldMatrix = localMat;
    }

calculated:
    isDirty = false;

    // 4. 子に親の変化を伝播させて再計算させる
    for (auto& c : children)
    {
        c->isDirty = true; // 親が変わったので強制的に再計算させる
        c->UpdateWorldMatrix();
    }
}
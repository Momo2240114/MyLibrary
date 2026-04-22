#include "Transform.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"



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

    worldMatrix = localMat;

    // フラグを戻す
    isDirty = false;

    // 4. 子に親の変化を伝播させて再計算させる
    for (auto& c : children)
    {
        c->isDirty = true; // 親が変わったので強制的に再計算させる
        c->UpdateWorldMatrix();
    }
}
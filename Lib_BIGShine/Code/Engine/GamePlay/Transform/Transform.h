#pragma once


#include <memory>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <d3d11.h>
#include <vector>

#include <ImGui.h>
#include <string>
#include <map>

#include"Vector.h"
#include <nlohmann/json.hpp>

class GameObject; // 前方宣言

// =========================================
// Transform
// =========================================
class Transform : public std::enable_shared_from_this<Transform>
{
public:
    // --- ローカル情報 ---
    DirectX::XMFLOAT3 localPosition{ 0, 0, 0 };   // ローカル位置
    DirectX::XMFLOAT3 localRotation{ 0, 0, 0 };   // ローカル回転（オイラー角）
    //Quaternion localRotation = Quaternion::Identity;//クォータニオン
    DirectX::XMFLOAT3 localScale{ 1, 1, 1 };      // スケール

    //アタッチメントを設定する親のノード名
    std::string m_ParentNodeName;

    // --- 階層関係 ---
    std::weak_ptr<GameObject> gameObject; // ポインタなので前方宣言だけでOK
    std::string parentNameDuringLoad;    // ロード時に親の名前を一時保存

    std::weak_ptr<Transform> parent;
    std::vector<std::shared_ptr<Transform>> children;

    // --- ワールドキャッシュ ---
    DirectX::XMMATRIX worldMatrix = DirectX::XMMatrixIdentity();
    bool isDirty = true;

    //グローバル行列
    Matrix m_GlobalMatrix = DirectX::XMMatrixIdentity();

    // --- 追加 --- 移動と回転のオフセット
    DirectX::XMFLOAT3 moveOffset{ 0, 0, 0 };      // 移動のオフセット
    DirectX::XMFLOAT3 rotateOffset{ 0, 0, 0 };    // 回転のオフセット（オイラー角）
    //Quaternion rotationOffset = Quaternion::Identity;

public:
    void Gui()
    {
        // 値が変わったらバージョンを上げる
        if (ImGui::DragFloat3("Position", &localPosition.x, 0.1f)) { MarkDirty(); }
        if (ImGui::DragFloat3("Rotation", &localRotation.x, 0.5f)) { MarkDirty(); }
        if (ImGui::DragFloat3("Scale", &localScale.x, 0.01f)) { MarkDirty(); }
    }

    //前方向ベクトルを取得する
    Vector3 GetFront();

    //横方向ベクトルを取得する
    Vector3 GetRight();

    //上方向ベクトルを取得する
    Vector3 GetUp();

    // ======================
    // 親子設定
    // ======================
    void SetParent(std::shared_ptr<Transform> p, std::string nodeName = "");

    // ======================
    // Dirtyフラグ
    // ======================
    void MarkDirty()
    {
        isDirty = true;
        for (auto& c : children)
            c->MarkDirty();
    }

    // ======================
    // 行列更新
    // ======================
    void UpdateWorldMatrix();


    // ======================
    // 移動値の追加
    // ======================
    void AddMoveOffset(const DirectX::XMFLOAT3& offset)
    {
        moveOffset.x += offset.x;
        moveOffset.y += offset.y;
        moveOffset.z += offset.z;
        MarkDirty();
    }

    // ======================
    // 回転値の追加
    // ======================
    void AddRotateOffset(const DirectX::XMFLOAT3& offset)
    {
        rotateOffset.x += offset.x;
        rotateOffset.y += offset.y;
        rotateOffset.z += offset.z;
        MarkDirty();
    }

    void SetRotationOffset(const DirectX::XMFLOAT3 offset)
    {
        rotateOffset.x = offset.x;
        rotateOffset.y = offset.y;
        rotateOffset.z = offset.z;
        MarkDirty();
    }

    void SetRotation(Vector3 angle)
    {
        localRotation = angle;
        MarkDirty();
    }

    // ======================
    // ワールド行列取得
    // ======================
    DirectX::XMMATRIX GetWorldMatrix()
    {
        //セットされていれば
        if (isDirty)
        {
            UpdateWorldMatrix();
        }

        return worldMatrix;
    }
    DirectX::XMFLOAT4X4 GetWorld()
    {
        //セットされていれば
        if (isDirty)
        {
            UpdateWorldMatrix();
        }

        DirectX::XMFLOAT4X4 m;
        DirectX::XMStoreFloat4x4(&m, worldMatrix);
        return m;
    }

    // ======================
    // ワールド座標取得
    // ======================
    DirectX::XMFLOAT3 GetWorldPosition()
    {
        using namespace DirectX;

        //セットされていれば
        if (isDirty)
        {
            UpdateWorldMatrix();
        }

        XMFLOAT3 pos;
        XMStoreFloat3(&pos, worldMatrix.r[3]);
        return pos;
    }

    // ======================
    // ワールドスケール取得
    // ======================
    DirectX::XMFLOAT3 GetWorldScale()
    {
        using namespace DirectX;

        //セットされていれば
        if (isDirty)
        {
            UpdateWorldMatrix();
        }


        XMVECTOR s, r, t;
        XMMatrixDecompose(&s, &r, &t, worldMatrix);


        DirectX::XMFLOAT3 scale;
        XMStoreFloat3(&scale, s);
        return scale;
    }

    // ======================
    // ワールド回転取得（オイラー角）
    // ======================
    DirectX::XMFLOAT3 GetWorldRotation()
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

    void SetLocalPosition(Vector3& localPosition)
    {
        this->localPosition = localPosition;
        MarkDirty();
    }

    void SetLocalScale(Vector3& localScale)
    {
        this->localScale = localScale;
        MarkDirty();
    }

    void SetLocalRotation(Vector3& angle)
    {
        this->localRotation = angle;
        MarkDirty();
    }

    // Transform.h 内に追加
    void ApplyWorldMatrix(const DirectX::XMMATRIX& newWorldMat)
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

};


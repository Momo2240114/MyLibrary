#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include <unordered_set>

#include "Collision.h"

class ColliderComponent : public Component
{
public:
    enum class ColliderType { Box, Sphere, Capsule };
public:

    //現在ヒットしているコリジョン
    std::unordered_set<std::shared_ptr<GameObject>> currentTriggers;
    std::vector<CollisionManifold> m_CollisionMainFolds;


    // 共通プロパティ
    DirectX::XMFLOAT3 offset = { 0.0f, 0.0f, 0.0f }; // モデル中心からのズレ
    bool isTrigger = false;                          // すり抜けるか
    bool isStatic = false;                           // 地面や壁など動かないものか
    bool enabled = true;                               // 有効フラグ
    bool isGrounded = false;
    bool shapeRenderFlg = true;
    float m_Mass = 0.0f;

    Vector4 m_DebugPrimitiveColor = { 0,0,0,1 };
    bool useGravity = false;            // 重力フラグ
    float gravityScale = 1.0f;          // 重力の強さ倍率
    DirectX::XMFLOAT3 velocity = { 0, 0, 0 }; // 現在の移動速度（落下速度用）

    // ColliderComponent.h 内
    void Serialize(nlohmann::json& j) const override {
        j["Offset"] = { offset.x, offset.y, offset.z };
        j["IsTrigger"] = isTrigger;
        j["IsStatic"] = isStatic;
        j["ShapeRenderFlg"] = shapeRenderFlg;
        j["UseGravity"] = useGravity;
        j["GravityScale"] = gravityScale;
        j["DebugPrimitiveColor"] = { m_DebugPrimitiveColor.x,m_DebugPrimitiveColor.y,m_DebugPrimitiveColor.z,m_DebugPrimitiveColor.w };
    }

    void Deserialize(const nlohmann::json& j) override {
        if (j.contains("Offset")) {
            offset.x = j["Offset"][0];
            offset.y = j["Offset"][1];
            offset.z = j["Offset"][2];
        }
        isTrigger = j.value("IsTrigger", false);
        isStatic = j.value("IsStatic", false);
        shapeRenderFlg = j.value("ShapeRenderFlg", true);
        useGravity = j.value("UseGravity", false);
        gravityScale = j.value("GravityScale", 1.0f);
        if (j.contains("DebugPrimitiveColor")) {
            m_DebugPrimitiveColor.x = j["DebugPrimitiveColor"][0];
            m_DebugPrimitiveColor.y = j["DebugPrimitiveColor"][1];
            m_DebugPrimitiveColor.z = j["DebugPrimitiveColor"][2];
            m_DebugPrimitiveColor.w = j["DebugPrimitiveColor"][3];
        }

    }

    // 重力計算用メソッド
    void ApplyGravity(float dt) {
    }

    // GUIの共通部分
    virtual void Gui();

    virtual void OnTriggerEnter(std::shared_ptr<GameObject> other) {}
    virtual void OnTriggerStay(std::shared_ptr<GameObject> other) {}
    virtual void OnTriggerExit(std::shared_ptr<GameObject> other) {}
    virtual void OnCollisionEnter(std::shared_ptr<GameObject> other) {}

    virtual CollisionManifold CheckCollision(std::shared_ptr<ColliderComponent> other) { return{}; };
    virtual ColliderType GetColliderType() const = 0;

    virtual ~ColliderComponent() = default;

    virtual void SetHitFlag(bool flag)
    {
        m_IsHitFlag = flag;
    }

    virtual bool GetHitFlag()
    {
        return m_IsHitFlag;
    }

    //追加処理
    virtual void AddCollisionMainFolds(const CollisionManifold& manifold)
    {
        m_CollisionMainFolds.push_back(manifold);
    }

    void Start()override {};
private:
    bool m_IsHitFlag = false;
};




#pragma once

#include "ComponentBase.h"
#include "EasingComponent.h"
#include "IActionProvider.h"

struct TransformWayPoint {
    Vector3 pos;
    Vector3 angle;
    Vector3 scale;
    float duration;

    EasingType easing;

    //加算用
    bool m_AddFlag = false;

    ////イージングを開始する直後のワールド行列を保存する
    //Matrix m_WorldMatrix;
};

class TransformDirector : public Component , public IActionProvider {
public:
    const std::string& GetName() const override { static std::string n = "TransformDirector"; return n; }

    void Start() override;

    void Update(float et) override;

    void PlayMove(const TransformWayPoint& wp, EasingType type);

    void Gui()override;
    DirectX::XMFLOAT3 QuaternionToEuler(DirectX::XMVECTOR q);

    void Serialize(nlohmann::json& j) const;
    void Deserialize(const nlohmann::json& j);

    void CaptureCurrentState();
    int GetCount() const { return (int)EditorWaypoints.size(); }
    void PlayMoveByIndex(int idx);

    void RegisterActions();

    bool GetIsMoving()
    {
        return isMoving;
    }
private:
    int easingIndex = -1;
    bool isMoving = false;
    Vector3 startPos, targetPos, startAngle, targetAngle,startScale,targetScale;
    std::vector<TransformWayPoint> EditorWaypoints;
    Vector3 LerpVector(Vector3 a, Vector3 b, float t) { return { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t }; }

    //加算用
    bool m_AddFlag = false;

    //イージングを開始する直後のワールド行列を保存する
    Matrix m_WorldMatrix;
};
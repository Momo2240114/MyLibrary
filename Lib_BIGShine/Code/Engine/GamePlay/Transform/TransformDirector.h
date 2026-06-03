#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/Math/EasingComponent.h"
#include "Engine/GamePlay/EventSystem/IActionProvider.h"

struct TransformWayPoint {
    Vector3 pos;
    Vector3 angle;
    Vector3 scale;
    float duration;

    EasingType easing;

    //加算用
    bool m_AddFlag = false;

    bool m_autoReturn = false;  // この移動のあとに戻るか
    float m_waitTime = 0.0f;    // 戻るまでの待機時間
};

struct TransformPath {
    std::string name;
    std::vector<TransformWayPoint> points; // 経由地のリスト
    bool m_autoReturn = false;             // 全行程終了後に戻るか
    float m_waitTime = 0.0f;               // 戻る前の待機
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

    void PlayReturn(float duration = 1.0f, EasingType ease = EasingType::OutExpo);

private:
    // 追加する関数
    void StartNextWaypoint();
    void PlayPath(int pathIdx);
    void CaptureToPath(int pathIdx);
private:
    // 戻るための地点（直前の座標）を保持
    bool shouldReturn = false;    // 戻るかどうかのフラグ
    float waitTime = 0.0f;        // 待機時間（設定値）
    float waitTimer = 0.0f;       // 待機カウント用タイマー
    bool isWaiting = false;       // 待機中かどうかの状態
    TransformWayPoint prevWP;     // 戻り先（移動開始時の座標）

    int easingIndex = -1;
    bool isMoving = false;
    Vector3 startPos, targetPos, startAngle, targetAngle,startScale,targetScale;
    std::vector<TransformWayPoint> EditorWaypoints;
    std::vector<TransformPath> m_paths;    // 保存されるパスのリスト
    int m_currentPathIdx = -1;
    int m_currentPointIdx = -1;            // 現在何番目の経由地に移動中か
    Vector3 LerpVector(Vector3 a, Vector3 b, float t) { return { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t }; }

    //加算用
    bool m_AddFlag = false;

    //イージングを開始する直後のワールド行列を保存する
    Matrix m_WorldMatrix;
};
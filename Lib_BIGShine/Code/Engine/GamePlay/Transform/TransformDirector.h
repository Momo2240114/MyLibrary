#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/Math/EasingComponent.h"
#include "Engine/GamePlay/EventSystem/IActionProvider.h"

struct TransformWayPoint {
    // 移動先のローカル Transform。m_AddFlag が true の時は開始地点への加算値として扱う。
    Vector3 pos;
    Vector3 angle;
    Vector3 scale;

    // この WayPoint へ到達するまでの時間。
    float duration;

    EasingType easing;

    //加算用
    bool m_AddFlag = false;

    bool m_autoReturn = false;  // この移動のあとに戻るか
    float m_waitTime = 0.0f;    // 戻るまでの待機時間
};

struct TransformPath {
    // Action 名や GUI 表示で使うパス名。
    std::string name;

    // 複数 WayPoint を順番に再生するための経路。
    std::vector<TransformWayPoint> points; // 経由地のリスト
    bool m_autoReturn = false;             // 全行程終了後に戻るか
    float m_waitTime = 0.0f;               // 戻る前の待機
};

class TransformDirector : public Component , public IActionProvider {
public:
    const std::string& GetName() const override { static std::string n = "TransformDirector"; return n; }

    void Start() override;

    void Update(float et) override;

    // 単体 WayPoint を再生する。type で保存値とは別の easing を指定できる。
    void PlayMove(const TransformWayPoint& wp, EasingType type);

    void Gui()override;

    // DirectX の Quaternion を、GUI で扱いやすい Euler 角へ変換する。
    DirectX::XMFLOAT3 QuaternionToEuler(DirectX::XMVECTOR q);

    void Serialize(nlohmann::json& j) const;
    void Deserialize(const nlohmann::json& j);

    // 現在の Transform を WayPoint として EditorWaypoints へ追加する。
    void CaptureCurrentState();
    int GetCount() const { return (int)EditorWaypoints.size(); }

    // ActionSequence などから番号指定で単体 WayPoint を再生する。
    void PlayMoveByIndex(int idx);

    void RegisterActions();

    bool GetIsMoving()
    {
        return isMoving;
    }

    // 現在位置から prevWP に保存した開始時 Transform へ戻る。
    void PlayReturn(float duration = 1.0f, EasingType ease = EasingType::OutExpo);

private:
    // Path 再生中に、次の WayPoint へ進める。
    void StartNextWaypoint();

    // 登録済み Path を先頭から再生する。
    void PlayPath(int pathIdx);

    // 現在の Transform を指定 Path の末尾 WayPoint として取り込む。
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

    // 現在再生中の補間開始値と目標値。
    Vector3 startPos, targetPos, startAngle, targetAngle,startScale,targetScale;

    // 単体 Action として呼べる WayPoint 一覧。
    std::vector<TransformWayPoint> EditorWaypoints;

    // 複数 WayPoint を連続再生する Path 一覧。
    std::vector<TransformPath> m_paths;    // 保存されるパスのリスト
    int m_currentPathIdx = -1;
    int m_currentPointIdx = -1;            // 現在何番目の経由地に移動中か

    // Transform の各成分を同じ ratio で補間するための小さな補助。
    Vector3 LerpVector(Vector3 a, Vector3 b, float t) { return { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t }; }

    //加算用
    bool m_AddFlag = false;

    //イージングを開始する直後のワールド行列を保存する
    // 加算移動時に、開始時の姿勢基準を保ったまま目標値を組み立てるために使う。
    Matrix m_WorldMatrix;
};

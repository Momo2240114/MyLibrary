#pragma once
#include <string>
#include <memory>
#include <vector>

#include "imgui_node_editor.h"
#include "imgui_node_editor_internal.h"
namespace ed = ax::NodeEditor;


struct RootMotionSettings {
    bool enable = false;        // この処理を起動するか否か
    std::string sourceNodeName; // 空の場合はモデル側のRootMotionNameを使用
    bool applyPositionX = true; // X軸の移動を許すか
    bool applyPositionY = true; // Y軸の移動を許すか
    bool applyPositionZ = true; // Z軸の移動を許すか
    bool applyRotationX = true; // X軸の傾きを許すか
    bool applyRotationY = true; // Y軸の振り向く力を許すか
    bool applyRotationZ = true; // Z軸の傾げを許すか
};

struct AnimEvent 
{
    std::string eventLabel; // 開始時に呼ぶアクション名
    std::string startAction; // 開始時に呼ぶアクション名 (例: "EnableHitBox")
    std::string updateAction; // 開始から終了の間、毎フレーム実行
    std::string endAction;   // 終了時に呼ぶアクション名 (例: "DisableHitBox")
    float startTime = 0.0f;
    float endTime = 0.0f;

    int frameStart = 0;
    int frameEnd = 0;
    bool isFired = false;    // そのループ内で実行済みか

};

// アニメーションの状態種別
enum class AnimStateType 
{
    Single,   // 通常の単一再生
    Blend1D ,  // 2つのアニメーションをパラメータでブレンド
    Custom
};

// ノードエディタ上の各「状態」を表現するベース
struct AnimState 
{
    AnimStateType type = AnimStateType::Single; // デフォルトは単一再生
    RootMotionSettings rootMotion;

    std::string sequencerDataName; // これがシーケンサーのファイルパス

    // --- Single 用データ ---
    int animationIndex = -1;

    // --- Blend1D 用データ ---
    int animIndexA = -1;
    int animIndexB = -1;
    std::string blendParamName = "BlendAlpha";

    // --- Custom (ポーズライブラリ) 用データ 【追加】 ---
    int customAnimIndex = -1; // PoseLibraryResource内の customAnims のインデックス

    //animationごとにRootMotionの移動値が違う可能性があるのでそれ用に設定できるように
    float rootMotionScale = 1.0f;
    std::string rootMotionScaleCurveName = "";


    std::string name;
    float playbackSpeed = 1.0f;
    bool isLoop = true;
    // 使用するカーブの名前（空なら従来の playbackSpeed を使用）
    std::string speedCurveName = "";
    // 将来的にはここに「ノードの座標」などのエディタ用データが入る
    // イベントリスト
    std::vector<AnimEvent> events;
};

enum class ConditionType 
{
    Variable,      // 通常の変数比較 (Speed > 1.0 など)
    AnimationEnd,  // アニメーションが最後まで再生されたか
    Trigger        // 1回だけ反応するフラグ
};
// 判定演算子
enum class ConditionOp { Equal, NotEqual, Greater, Less };

enum class TransitionLogic { And, Or };

enum class AnimGraphNodeType
{
    State,
    Start,
    Any,
    SubGraph,
    Entry,
    Exit
};

// 遷移条件
struct AnimCondition {
    ConditionType type = ConditionType::Variable;
    std::string parameterName; // Blackboardのキー
    ConditionOp op;
    float threshold;           // Vector2等の場合は長さや特定の成分と比較するロジックが必要
};

// 遷移（リンク）
struct AnimTransition 
{
    std::string fromStateName; // 始まりのステート名
    std::string toStateName;   // 行き先のステート名
    float duration = 0.2f;
    TransitionLogic logic = TransitionLogic::And;
    std::vector<AnimCondition> conditions;
    ed::LinkId id;
    std::string scopeId = "Root";
};

// ノード（ステート）
struct AnimGraphNode {
    ed::NodeId id;
    std::string stateName;     // AnimationControllerComponent::states内と一致
    ed::PinId inputPin;
    ed::PinId outputPin;
    ImVec2 pos;
    AnimGraphNodeType type = AnimGraphNodeType::State;
    std::string scopeId = "Root";
    std::string childScopeId;
};

// 宣言だけを記述
void to_json(nlohmann::json& j, const AnimEvent& e);
void from_json(const nlohmann::json& j, AnimEvent& e);
void to_json(nlohmann::json& j, const AnimState& s);
void from_json(const nlohmann::json& j, AnimState& s);

//RootMotionSettings に加えて、個々のステートのシリアライズに統合する
inline void to_json(nlohmann::json& j, const RootMotionSettings& r) {
    j = nlohmann::json{
        {"enable", r.enable},
        {"sourceNodeName", r.sourceNodeName},
        {"applyPositionX", r.applyPositionX},
        {"applyPositionY", r.applyPositionY},
        {"applyPositionZ", r.applyPositionZ},
        {"applyRotationX", r.applyRotationX},
        {"applyRotationY", r.applyRotationY},
        {"applyRotationZ", r.applyRotationZ}
    };
}

inline void from_json(const nlohmann::json& j, RootMotionSettings& r) {
    r.enable = j.value("enable", false);
    r.sourceNodeName = j.value("sourceNodeName", "");
    r.applyPositionX = j.value("applyPositionX", true);
    r.applyPositionY = j.value("applyPositionY", true);
    r.applyPositionZ = j.value("applyPositionZ", true);
    r.applyRotationX = j.value("applyRotationX", true);
    r.applyRotationY = j.value("applyRotationY", true);
    r.applyRotationZ = j.value("applyRotationZ", true);
}



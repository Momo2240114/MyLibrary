#pragma once
#include <string>
#include <memory>
#include <vector>

#include "imgui_node_editor.h"
#include "imgui_node_editor_internal.h"
namespace ed = ax::NodeEditor;




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
    Blend1D   // 2つのアニメーションをパラメータでブレンド
};

// ノードエディタ上の各「状態」を表現するベース
struct AnimState 
{
    AnimStateType type = AnimStateType::Single; // デフォルトは単一再生

    // --- Single 用データ ---
    int animationIndex = -1;

    // --- Blend1D 用データ ---
    int animIndexA = -1;
    int animIndexB = -1;
    std::string blendParamName = "BlendAlpha";

    std::string name;
    float playbackSpeed = 1.0f;
    bool isLoop = true;
    // 使用するカーブの名前（空なら従来の playbackSpeed を使用）
    std::string speedCurveName = "";
    // 将来的にはここに「ノードの座標」などのエディタ用データが入る
    // イベントリスト
    std::vector<AnimEvent> events;
};

enum class ConditionType {
    Variable,      // 通常の変数比較 (Speed > 1.0 など)
    AnimationEnd,  // アニメーションが最後まで再生されたか
    Trigger        // 1回だけ反応するフラグ
};
// 判定演算子
enum class ConditionOp { Equal, NotEqual, Greater, Less };

// 遷移条件
struct AnimCondition {
    ConditionType type = ConditionType::Variable;
    std::string parameterName; // Blackboardのキー
    ConditionOp op;
    float threshold;           // Vector2等の場合は長さや特定の成分と比較するロジックが必要
};

// 遷移（リンク）
struct AnimTransition {
    ed::LinkId id;
    ed::PinId startPin;
    ed::PinId endPin;
    float duration = 0.2f;     // クロスフェード時間
    std::vector<AnimCondition> conditions;
};

// ノード（ステート）
struct AnimGraphNode {
    ed::NodeId id;
    std::string stateName;     // AnimationControllerComponent::states内と一致
    ed::PinId inputPin;
    ed::PinId outputPin;
    ImVec2 pos;
};
// 宣言だけを記述
void to_json(nlohmann::json& j, const AnimEvent& e);
void from_json(const nlohmann::json& j, AnimEvent& e);
void to_json(nlohmann::json& j, const AnimState& s);
void from_json(const nlohmann::json& j, AnimState& s);
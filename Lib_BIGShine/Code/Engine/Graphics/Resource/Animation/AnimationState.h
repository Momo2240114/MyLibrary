#pragma once
#include <string>
#include <memory>
#include <vector>

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

// ノードエディタ上の各「状態」を表現するベース
struct AnimState 
{
    std::string name;
    int animationIndex = -1;
    float playbackSpeed = 1.0f;
    bool isLoop = true;

    // 将来的にはここに「ノードの座標」などのエディタ用データが入る
    // イベントリスト
    std::vector<AnimEvent> events;
};

// 遷移条件の抽象
struct Transition 
{
    std::string targetStateName;
    float duration = 0.2f;
    // 条件式（例: "Speed > 0.1"）を保持する関数ポインタや文字列
    // std::function<bool()> condition; 
};

// 宣言だけを記述
void to_json(nlohmann::json& j, const AnimEvent& e);
void from_json(const nlohmann::json& j, AnimEvent& e);
void to_json(nlohmann::json& j, const AnimState& s);
void from_json(const nlohmann::json& j, AnimState& s);
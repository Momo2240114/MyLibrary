#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

// Editorでの分類用情報。カテゴリによる専用の実行分岐は作らない。
enum class SkillCategory
{
    ATTACK, RUSH, LAUNCHER, PROJECTILE, HEAL, BUFF, COUNTER, UTILITY
};

// 分類名をJSONとEditorの共通表記へ変換する。
const char* GetSkillCategoryName(SkillCategory category_);

// プレイヤーと敵が共通で使用するPhase終了条件。
enum class SkillPhaseEnd
{
    ANIMATION_END,
    DURATION,
    ACTION_END,
    GROUND_CONTACT
};

// 既存Animation EventとActionSequenceを組み合わせるSkillの一区間。
struct SkillPhase
{
    std::string animationState;
    std::string sequence;
    // 空なら補正なし。敵の実行担当が段ごとの既存MotionWarpプリセットを選ぶ。
    std::string motionWarpPreset;
    bool continueSequence = false; // 前段と同じSequenceを継続し、コンボ全段の予約を維持する。
    bool playAnimationFromSequence = false; // 時刻0のSequence Actionで再生し、Phase側の直接再生を省く。
    bool waitForAnimationEnd = false; // ACTION_END通知が早くても、反応クリップの終端まで制御を保持する。
    std::string enterAction;
    std::string exitAction;
    SkillPhaseEnd endCondition = SkillPhaseEnd::ANIMATION_END;
    float duration = 0.0f;
    float maximumDuration = 10.0f;
    float blendDuration = 0.1f;
    float animationStartTime = 0.0f; // ブレンド先クリップの開始位置。シークで補間を解除せず再生を始める。
    bool canDodgeCancel = false;
    bool canActionBranch = false;
    bool canBufferInput = false;
    bool enableGuard = false; // PlayerのGuard受付をこのPhaseだけ有効にする。
    bool preventFlinch = false; // ダメージは受けるが、通常のひるみと吹き飛びを抑制する。
    // 既存CharacterMovementへ渡す移動設定。重力停止中はVerticalSpeedで高さを制御する。
    float forwardSpeed = 0.0f;
    std::string forwardSpeedCurveName; // Phase進行率を使って前進速度を補正する既存Curveの名前。
    float warpHeight = 0.0f; // Phase開始時に現在のY座標へ加えるワープ高さ。
    float verticalImpulse = 0.0f;
    float verticalSpeed = 0.0f;
    std::string verticalSpeedCurveName; // Phase経過秒で評価する縦速度の倍率Curve。
    bool suspendGravity = false;
};

// 入力やAI判断を含まず、Skillの実行内容だけを保持する共通データ。
struct SkillDefinition
{
    std::string name;
    SkillCategory category = SkillCategory::UTILITY;
    bool canUseOnGround = true;
    bool canUseInAir = false;
    bool enabled = true;
    float cooldown = 0.0f; // Playerの発動成功時に開始する秒数。AI側の既存Cooldown管理は維持する。
    std::string cleanupAction;
    std::vector<SkillPhase> phases;
};

// 共通項目を読み込み、Player Actionなど呼び出し側固有の項目には依存しない。
bool DeserializeSkillDefinition(const nlohmann::json& json_, SkillDefinition& definition_, std::string& error_);
// 元のJSONへ共通項目だけを書き込み、入力割り当てや未対応項目を保持する。
void SerializeSkillDefinition(const SkillDefinition& definition_, nlohmann::json& json_);
// 共通ライブラリを検証して読み込み、成功時だけ現在の定義を差し替える。
bool LoadSkillDefinitions(const std::string& path_, std::vector<SkillDefinition>& definitions_,
    nlohmann::json& document_, std::string& error_);
// 一時ファイルへの書き込み成功後に置換し、失敗時に既存ファイルを失わない。
bool SaveSkillDefinitions(const std::string& path_, const nlohmann::json& document_, std::string& error_);

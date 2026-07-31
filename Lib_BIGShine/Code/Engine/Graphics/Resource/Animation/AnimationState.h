#pragma once
#include <string>
#include <cstdint>
#include <memory>
#include <vector>
#include <DirectXMath.h>

#include "imgui_node_editor.h"
#include "imgui_node_editor_internal.h"
namespace ed = ax::NodeEditor;

enum class ANIM_BLACKBOARD_VALUE_TYPE
{
    FLOAT,
    INT,
    BOOL,
    STRING
};

enum class ANIMATION_EVENT_TEMPLATE_TYPE
{
    NONE,
    ATTACK_TEMPLATE,
    TRAIL_TEMPLATE,
    VFX_TEMPLATE,
    SOUND_TEMPLATE
};

struct AnimBlackboardWrite
{
    std::string category = "Attack";
    std::string key = "Damage";
    ANIM_BLACKBOARD_VALUE_TYPE type = ANIM_BLACKBOARD_VALUE_TYPE::FLOAT;
    float floatValue = 0.0f;
    int intValue = 0;
    bool boolValue = false;
    std::string stringValue;
};


struct RootMotionSettings {
    bool enable = false;        // この処理を起動するか否か
    std::string sourceNodeName; // 空の場合はモデル側のRootMotionNameを使用
    bool applyPositionX = true; // X軸の移動を許すか
    bool applyPositionY = true; // Y軸の移動を許すか
    bool applyPositionZ = true; // Z軸の移動を許すか
    bool cutVisualPositionX = true;
    bool cutVisualPositionY = true;
    bool cutVisualPositionZ = true;
    bool applyRotationX = false; // X軸の傾きを許すか
    bool applyRotationY = false; // Y軸の振り向く力を許すか
    bool applyRotationZ = false; // Z軸の傾げを許すか
};

struct AnimEvent 
{
    std::string eventLabel; // 開始時に呼ぶアクション名
    std::string startAction; // 開始時に呼ぶアクション名 (例: "EnableHitBox")
    std::string updateAction; // 開始から終了の間、毎フレーム実行
    std::string endAction;   // 終了時に呼ぶアクション名 (例: "DisableHitBox")
    float startTime = 0.0f;
    float endTime = 0.0f;
    bool overridePlaybackSpeed = false;
    float playbackSpeed = 1.0f;

    int frameStart = 0;
    int frameEnd = 0;
    bool isFired = false;    // そのループ内で実行済みか
    bool isEndFired = false; // End action fired in this loop.
    bool callEndOnStateExit = false;
    std::vector<AnimBlackboardWrite> blackboardWrites;
    ANIMATION_EVENT_TEMPLATE_TYPE eventTemplateType = ANIMATION_EVENT_TEMPLATE_TYPE::NONE;
    std::string attackTemplateId;
    std::string ownerTarget = "Self";
    bool attackTemplateRuntimeActive = false;
    uint32_t attackTemplateInstanceId = 0u;
    std::vector<bool> spawnedAttackEffects;
    std::string meshVFXAssetPath;
    std::string attachNodeName;
    DirectX::XMFLOAT3 localPosition = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 localRotation = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 localScale = { 1.0f, 1.0f, 1.0f };
    float meshVFXLifeTime = 1.0f;
    float meshVFXPlaybackSpeed = 1.0f;
    bool attachToOwner = false;
    bool mirrorMeshVFX = false;
};

// アニメーションの状態種別
enum class AnimStateType 
{
    Single,   // 通常の単一再生
    Blend1D,  // 2つのアニメーションをパラメータでブレンド
    Custom,
    BLEND_2D   // Vector2入力から前後左右のアニメーションをブレンド
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
    float blendDefaultWeight = 0.5f;
    float blendParameterMin = 0.0f;
    float blendParameterMax = 1.0f;
    float blendPhaseOffsetA = 0.0f;
    float blendPhaseOffsetB = 0.0f;
    bool blendSyncNormalizedTime = true;
    bool blendUseMask = false;
    std::vector<std::string> blendMaskRootNodes;

    // --- Blend2D 用データ ---
    int blend2DForwardAnimationIndex = -1;
    int blend2DBackwardAnimationIndex = -1;
    int blend2DLeftAnimationIndex = -1;
    int blend2DRightAnimationIndex = -1;
    int blend2DForwardLeftAnimationIndex = -1;
    int blend2DForwardRightAnimationIndex = -1;
    int blend2DBackwardLeftAnimationIndex = -1;
    int blend2DBackwardRightAnimationIndex = -1;
    int blend2DForwardLeft90AnimationIndex = -1;
    int blend2DForwardRight90AnimationIndex = -1;
    int blend2DBackwardLeft90AnimationIndex = -1;
    int blend2DBackwardRight90AnimationIndex = -1;
    int blend2DIdleAnimationIndex = -1;
    std::string blend2DParamName = "MoveInput";
    DirectX::XMFLOAT2 blend2DDefaultInput = { 0.0f, 1.0f };
    bool blend2DUseIdleStop = false;
    bool blend2DUseEightDirections = false;
    bool blend2DUseForwardBackwardLateral = false;
    float blend2DIdleThreshold = 0.05f;
    bool blend2DSyncNormalizedTime = true;
    float blend2DForwardPhaseOffset = 0.0f;
    float blend2DBackwardPhaseOffset = 0.0f;
    float blend2DLeftPhaseOffset = 0.0f;
    float blend2DRightPhaseOffset = 0.0f;
    float blend2DForwardLeftPhaseOffset = 0.0f;
    float blend2DForwardRightPhaseOffset = 0.0f;
    float blend2DBackwardLeftPhaseOffset = 0.0f;
    float blend2DBackwardRightPhaseOffset = 0.0f;
    float blend2DForwardLeft90PhaseOffset = 0.0f;
    float blend2DForwardRight90PhaseOffset = 0.0f;
    float blend2DBackwardLeft90PhaseOffset = 0.0f;
    float blend2DBackwardRight90PhaseOffset = 0.0f;
    float blend2DIdlePhaseOffset = 0.0f;

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

enum class ANIM_GRAPH_CONNECTION_MODE
{
    LEGACY_TWO_PIN,
    UNITY_STYLE
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
void to_json(nlohmann::json& json_, const AnimBlackboardWrite& write_);
void from_json(const nlohmann::json& json_, AnimBlackboardWrite& write_);
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
        {"cutVisualPositionX", r.cutVisualPositionX},
        {"cutVisualPositionY", r.cutVisualPositionY},
        {"cutVisualPositionZ", r.cutVisualPositionZ},
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
    r.cutVisualPositionX = j.value("cutVisualPositionX", true);
    r.cutVisualPositionY = j.value("cutVisualPositionY", true);
    r.cutVisualPositionZ = j.value("cutVisualPositionZ", true);
    r.applyRotationX = j.value("applyRotationX", false);
    r.applyRotationY = j.value("applyRotationY", false);
    r.applyRotationZ = j.value("applyRotationZ", false);
}



#pragma once

// AIとアニメーショングラフの要求・結果を共有するキー。実行中の要求は保存値から復元しない。
namespace AnimationGraphBlackboardKeys
{
    inline constexpr const char* ANIMATION_CATEGORY = "Anim";
    inline constexpr const char* CURRENT_ACTION = "CurrentAction";
    inline constexpr const char* ACTION_TRIGGER = "ActionTrigger";
    inline constexpr const char* ACTION_ENDED = "ActionEnded";
    inline constexpr const char* RETURN_TO_IDLE = "ReturnToIdle";
    inline constexpr const char* RETURN_TO_AIR_IDLE = "ReturnToAirIdle";
    inline constexpr const char* IS_ACTING = "IsActing";
    inline constexpr const char* DESIRED_STATE = "CurrentAnimationState";
    inline constexpr const char* MOVE_INPUT = "MoveInput";
    inline constexpr const char* GUARD_MOVE_INPUT = "GuardMoveInput";
    inline constexpr const char* DODGE_INPUT = "DodgeInput";
    inline constexpr const char* REQUEST = "AnimationGraphRequest";
    inline constexpr const char* FEEDBACK = "AnimationGraphFeedback";
    inline constexpr const char* CATEGORY = "Category";
    inline constexpr const char* INPUT_CATEGORY_PENDING = "InputCategoryPending";
    inline constexpr const char* INPUT_CATEGORY = "InputCategory";
    inline constexpr const char* STATE_PENDING = "StatePending";
    inline constexpr const char* STATE = "State";
    inline constexpr const char* BLEND = "Blend";
    inline constexpr const char* RESTART = "Restart";
    inline constexpr const char* START_TIME = "StartTime";
    inline constexpr const char* ALLOW_SKILL = "AllowSkill";
    inline constexpr const char* ACCEPTED = "Accepted";
    inline constexpr const char* EVALUATE_PENDING = "EvaluatePending";
    inline constexpr const char* EVALUATE_IMMEDIATE = "EvaluateImmediate";
    inline constexpr const char* TELEGRAPH_CANCEL_PENDING = "TelegraphCancelPending";
    inline constexpr const char* YAW_PENDING = "YawPending";
    inline constexpr const char* YAW_ENABLED = "YawEnabled";
    inline constexpr const char* YAW_LIMIT = "YawLimit";
    inline constexpr const char* ASSET_PENDING = "AssetPending";
    inline constexpr const char* ASSET_STATE = "AssetState";
    inline constexpr const char* ASSET_PATH = "AssetPath";
    inline constexpr const char* ASSET_LOADED = "AssetLoaded";
    inline constexpr const char* ROOT_PENDING = "RootPending";
    inline constexpr const char* ROOT_STATE = "RootState";
    inline constexpr const char* ROOT_START = "RootStart";
    inline constexpr const char* ROOT_END = "RootEnd";
    inline constexpr const char* ROOT_SUCCESS = "RootSuccess";
    inline constexpr const char* ROOT_DELTA = "RootDelta";
    inline constexpr const char* ROOT_DURATION = "RootDuration";
    inline constexpr const char* CURRENT_STATE = "CurrentState";
    inline constexpr const char* PLAYBACK_TIME = "PlaybackTime";
    inline constexpr const char* PLAYBACK_DURATION = "PlaybackDuration";
    inline constexpr const char* SKILL_OWNED = "SkillOwned";
    inline constexpr const char* TRANSITION_PREFIX = "GraphTo_";
}

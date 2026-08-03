#pragma once

// AIController、BehaviorGraph、Animation間で共有するBlackboardキーを一元管理する。
// 文字列の打ち間違いを防ぎ、各システムが同じカテゴリとキーを参照できるようにする。
namespace AIBlackboardKeys
{
    // AIの判断・行動状態を保存するカテゴリ名。
    constexpr const char* CATEGORY_AI = "AI";

    // アニメーション遷移用パラメータを保存するカテゴリ名。
    constexpr const char* CATEGORY_ANIM = "Anim";

    // 有効なTargetを取得できているかを示す。
    constexpr const char* HAS_TARGET = "HasTarget";

    // OwnerからTargetまでの距離。
    constexpr const char* TARGET_DISTANCE = "TargetDistance";

    // Ownerから初期位置までの距離。
    constexpr const char* SPAWN_DISTANCE = "SpawnDistance";

    // Targetが索敵範囲内にいるかを示す。
    constexpr const char* IS_TARGET_IN_DETECT_RANGE = "IsTargetInDetectRange";

    // Targetが攻撃可能範囲内にいるかを示す。
    constexpr const char* IS_TARGET_IN_ATTACK_RANGE = "IsTargetInAttackRange";

    // Targetを見失った、またはLostRange外にいるかを示す。
    constexpr const char* IS_TARGET_LOST = "IsTargetLost";

    // Ownerが初期位置の停止範囲内にいるかを示す。
    constexpr const char* IS_AT_SPAWN = "IsAtSpawn";

    // Ownerの現在World座標。
    constexpr const char* OWNER_POSITION = "OwnerPosition";

    // Targetの現在World座標。
    constexpr const char* TARGET_POSITION = "TargetPosition";

    // AIが帰還先として使用する初期World座標。
    constexpr const char* SPAWN_POSITION = "SpawnPosition";

    // OwnerからTargetへ向かう正規化方向。
    constexpr const char* TO_TARGET_DIRECTION = "ToTargetDirection";

    // Ownerから初期位置へ向かう正規化方向。
    constexpr const char* TO_SPAWN_DIRECTION = "ToSpawnDirection";

    // 現在実行中のAIアクション名。
    constexpr const char* CURRENT_ACTION = "CurrentAction";

    // 現在のアクションが終了したかを示す。
    constexpr const char* ACTION_ENDED = "ActionEnded";

    // AIが終了待ちを伴うアクション中かを示す。
    constexpr const char* IS_ACTING = "IsActing";

    // アニメーション開始要求に使用するTrigger名。
    constexpr const char* ANIMATION_ACTION_TRIGGER = "ActionTrigger";

    // アニメーション終了要求に使用するTrigger名。
    constexpr const char* ANIMATION_ACTION_END_TRIGGER = "ActionEndTrigger";
}

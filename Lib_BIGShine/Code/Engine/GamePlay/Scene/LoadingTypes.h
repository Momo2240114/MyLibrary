#pragma once
#include <string>

// SceneLoadingが所有するロード要求の状態。ゲーム内の戦闘進行とは独立している。
enum class LOADING_STATE { IDLE, PREPARING, LOADING, FINALIZING, COMPLETED, FAILED };
enum class LOADING_SILHOUETTE_MODE { WHITE, BLACK };

// UIへ値で返す進捗情報。Sceneやワーカーの内部データを参照させない。
struct LoadingStatus
{
    LOADING_STATE state = LOADING_STATE::IDLE;
    float progress = 0.0f;
    std::string operation;
    std::string error;
    bool IsCompleted() const { return state == LOADING_STATE::COMPLETED; }
    bool HasFailed() const { return state == LOADING_STATE::FAILED; }
};

#pragma once

#include "AttackTelegraphSettings.h"
#include <nlohmann/json.hpp>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class GameObject;
class GameObjectManager;
class GltfModelCommponennt;
struct AnimState;

// Animatorの再生区間ごとに予兆を一度だけ予約し、最新のモデル姿勢からParticleを出す。
class AttackTelegraphRuntime
{
public:
    // State変更・再開始・シーク時に、旧区間の未発生予兆を破棄する。
    void Reset();
    // Stateが変わらない中断でも予約を破棄し、明示的な再開始まで再予約しない。
    void Cancel(const AnimState* state_);
    // 攻撃判定と同じアニメーション時間を使い、今回通過した予兆だけを予約する。
    void Advance(const std::shared_ptr<GameObject>& owner_, const AnimState& state_, float previousTime_, float currentTime_);
    // ブレンドとRootMotionを反映した後に、予約済みの予兆を描画側へ渡す。
    void Flush(const std::shared_ptr<GameObject>& owner_);
    // Editorのプレビューも通常再生と同じ発生位置・Particle経路を使用する。
    static void Preview(const std::shared_ptr<GameObject>& owner_, const AttackTelegraphSettings& settings_);
    // Templateと従来のAnimation Eventで共通の保存形式を使用する。
    static nlohmann::json SerializeSettings(const AttackTelegraphSettings& settings_);
    static AttackTelegraphSettings DeserializeSettings(const nlohmann::json& json_);
    // 既存Editorへ設定と選択中モデルでのプレビューを追加する。
    static void DrawSettings(AttackTelegraphSettings& settings_, GameObjectManager* manager_, const std::shared_ptr<GameObject>& owner_ = nullptr);

private:
    // 同じ開始時刻の複数HitBoxをまとめるため、判定時刻も保持する。
    struct Cue
    {
        float hitTime = 0.0f;
        float warningTime = 0.0f;
        AttackTelegraphSettings settings;
        bool consumed = false;
    };

    // 再生開始時だけイベントを調べ、毎フレームのTemplate検索を避ける。
    void Build(const std::shared_ptr<GameObject>& owner_, const AnimState& state_, float startTime_);
    // ノードの行列とローカルオフセットから発生位置を求め、既存EffectManagerへ渡す。
    void Spawn(const std::shared_ptr<GameObject>& owner_, const AttackTelegraphSettings& settings_);
    // 同じ設定不備を毎フレーム・毎ループ出力しない。
    void WarnOnce(const std::string& message_);

    const AnimState* activeState = nullptr;
    float lastTime = -1.0f;
    bool canceled = false;
    std::vector<Cue> cues;
    std::vector<AttackTelegraphSettings> pending;
    std::weak_ptr<GltfModelCommponennt> cachedModel;
    const void* cachedNodes = nullptr;
    std::size_t cachedNodeCount = 0;
    std::unordered_map<std::string, int> nodeIndices;
    std::unordered_set<std::string> warnings;
};

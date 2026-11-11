#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "AnimationState.h"
#include <unordered_map>

class AnimationControllerComponent;
class BlackboardComponent;

// AIはBlackboardへ要求を渡し、グラフ側だけが再生制御を行うための窓口。
class AnimationGraphBridgeComponent : public Component
{
public:
    const std::string& GetName() const override;
    // 必要なOwnerだけに窓口を作成し、Controllerの寿命はweak参照で扱う。
    static std::shared_ptr<AnimationGraphBridgeComponent> Acquire(const std::shared_ptr<GameObject>& owner_);
    // グラフ遷移条件へ要求を渡す。Skill自身からのPhase開始だけ制御権を明示する。
    bool RequestStateTransition(const std::string& state_, float blend_ = 0.2f, bool restart_ = false,
        float startTime_ = 0.0f, bool allowSkill_ = false, const std::string& category_ = "Anim");
    // 既存To_条件などをBlackboardへ書き、グラフの評価を要求する。
    void RequestTrigger(const std::string& trigger_, const std::string& category_ = "Anim");
    void ClearTrigger(const std::string& trigger_, const std::string& category_ = "Anim");
    void RequestEvaluation(const std::string& category_ = "Anim");
    // 固定カテゴリを使用するAIの既存入力接続を、Blackboard要求として維持する。
    void RequestInputCategory(const std::string& category_);
    // RootMotion回転上限もBlackboardの要求として扱う。
    void RequestYawLimit(float degrees_);
    void ClearYawLimit();
    // 遷移先がない攻撃中断も、グラフへの要求として未発生予兆を破棄する。
    void RequestCancelAttackTelegraphs();
    // 未登録Assetと予定RootMotionの問い合わせをエンジン側へ渡す。
    bool LoadMissingStateAsset(const std::string& state_, const std::string& path_);
    bool EstimateRootMotion(const std::string& state_, float start_, float end_,
        DirectX::XMFLOAT3& delta_, float& duration_);
    // 定義は読取専用で公開し、実行状態と時間はBlackboardの結果から取得する。
    const std::unordered_map<std::string, std::unique_ptr<AnimState>>& GetStateCatalog();
    const AnimState* GetActiveState();
    float GetPlaybackTime();
    float GetPlaybackDuration();
    bool IsSkillOwned();
private:
    // Actorごとに接続を保持し、再取得時に古い実行要求を引き継がない。
    std::shared_ptr<AnimationControllerComponent> ResolveController();
    std::shared_ptr<BlackboardComponent> ResolveBoard();
    void ProcessRequests(bool transitions_ = false);
    void RefreshFeedback();
    std::weak_ptr<AnimationControllerComponent> controller;
    std::weak_ptr<BlackboardComponent> board;
};

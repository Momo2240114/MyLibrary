#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"

#include <memory>

class HitBox;

// HitBoxの再有効化と攻撃値調整をEditor上で確認するためのデバッグComponent。
// 一度当たった対象への再ヒット履歴を一定間隔でリセットし、当たり判定の確認をしやすくする。
class DebugHitHistoryResetComponent : public Component
{
public:
    const std::string& GetName() const override
    {
        static std::string name_ = "DebugHitHistoryResetComponent";
        return name_;
    }

    void Start() override;
    void Update(float elapsedTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

private:
    // Owner上のHitBoxを取得し、存在しない場合は追加する。
    void ResolveHitBox();
    // Editorで設定した攻撃パラメータをHitBoxへ反映する。
    void ApplySettings();
    // HitBoxを一度停止してから再起動し、ヒット履歴をリセットする。
    void Rearm();

    // 操作対象のHitBox。Ownerが消えた場合に所有権を持たないようweak参照にする。
    std::weak_ptr<HitBox> hitBox;
    // HitBoxへ反映するダメージ量。
    float damage = 5.0f;
    // Hit時の衝撃値。
    float impactPower = 9.0f;
    // HitStopの継続時間。
    float hitStopDuration = 0.05f;
    // HitStop中に適用する時間スケール。
    float hitStopScale = 0.0f;
    // Hit対象を押し戻す強さ。
    float knockBackPower = 5.0f;
    // 自動でRearmする間隔。
    float rearmInterval = 0.8f;
    // 次のRearmまでの経過時間。
    float rearmTimer = 0.0f;
    // trueなら一定間隔でHitBoxを自動的に再有効化する。
    bool autoActivate = true;
};

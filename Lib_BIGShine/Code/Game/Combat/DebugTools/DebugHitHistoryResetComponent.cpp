#include "PrecompiledHeader.h"

#include "DebugHitHistoryResetComponent.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/Physics/HitBox.h"

#include <algorithm>
#include <imgui.h>

// 開始時にHitBoxを準備し、初期設定を反映して必要なら即座に有効化する。
void DebugHitHistoryResetComponent::Start()
{
    ResolveHitBox();
    ApplySettings();
    if (autoActivate) Rearm();
}

// HitBox設定を更新し、自動有効化が有効なら一定間隔でヒット履歴をリセットする。
void DebugHitHistoryResetComponent::Update(float elapsedTime_)
{
    ResolveHitBox();
    const std::shared_ptr<HitBox> hitBox_ = hitBox.lock();
    if (!hitBox_) return;

    ApplySettings();
    if (!autoActivate)
    {
        // 手動確認中はHitBoxを停止し、再開時に余分な経過時間が残らないようにする。
        hitBox_->Deactivate();
        rearmTimer = 0.0f;
        return;
    }

    rearmTimer += (std::max)(elapsedTime_, 0.0f);
    if (rearmTimer >= (std::max)(rearmInterval, 0.01f)) Rearm();
}

void DebugHitHistoryResetComponent::ResolveHitBox()
{
    // 既に有効な参照がある場合は、毎フレーム検索し直さない。
    if (!hitBox.expired()) return;
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (owner_) hitBox = owner_->GetOrAddComponent<HitBox>();
}

// GUIまたはJSONから設定された値を安全な範囲へ補正し、HitBoxへ直接反映する。
void DebugHitHistoryResetComponent::ApplySettings()
{
    const std::shared_ptr<HitBox> hitBox_ = hitBox.lock();
    if (!hitBox_) return;

    hitBox_->damage = (std::max)(damage, 0.0f);
    hitBox_->impactPower = (std::max)(impactPower, 0.0f);
    hitBox_->hitStopDuration = (std::max)(hitStopDuration, 0.0f);
    hitBox_->hitStopScale = (std::clamp)(hitStopScale, 0.0f, 1.0f);
    hitBox_->knockBackPower = (std::max)(knockBackPower, 0.0f);
    hitBox_->allowMultiHit = false;
    hitBox_->hitOncePerActivation = true;
    hitBox_->canHitOwner = false;
    hitBox_->attackTag = "DamageBox";
}

// HitBoxを再起動し、同一対象へ再度ヒットできる確認状態を作る。
void DebugHitHistoryResetComponent::Rearm()
{
    const std::shared_ptr<HitBox> hitBox_ = hitBox.lock();
    if (!hitBox_) return;
    hitBox_->Deactivate();
    hitBox_->Activate();
    rearmTimer = 0.0f;
}

// デバッグ用HitBoxの攻撃値と再有効化間隔を編集するImGuiを描画する。
void DebugHitHistoryResetComponent::Gui()
{
    ImGui::Checkbox("Auto Activate", &autoActivate);
    ImGui::DragFloat("Damage", &damage, 0.1f, 0.0f, 99999.0f);
    ImGui::DragFloat("Impact Power", &impactPower, 0.1f, 0.0f, 99999.0f);
    ImGui::DragFloat("Hit Stop Duration", &hitStopDuration, 0.001f, 0.0f, 10.0f);
    ImGui::DragFloat("Hit Stop Scale", &hitStopScale, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("Knock Back Power", &knockBackPower, 0.1f, 0.0f, 99999.0f);
    ImGui::DragFloat("Rearm Interval", &rearmInterval, 0.01f, 0.01f, 30.0f);
    if (ImGui::Button("Rearm Now")) Rearm();
}

// デバッグ確認に必要なHitBox設定をJSONへ保存する。
void DebugHitHistoryResetComponent::Serialize(nlohmann::json& json_) const
{
    json_["Damage"] = damage;
    json_["ImpactPower"] = impactPower;
    json_["HitStopDuration"] = hitStopDuration;
    json_["HitStopScale"] = hitStopScale;
    json_["KnockBackPower"] = knockBackPower;
    json_["RearmInterval"] = rearmInterval;
    json_["AutoActivate"] = autoActivate;
}

// JSONからHitBox設定を復元し、数値項目を有効範囲へ丸める。
void DebugHitHistoryResetComponent::Deserialize(const nlohmann::json& json_)
{
    damage = (std::max)(json_.value("Damage", 5.0f), 0.0f);
    impactPower = (std::max)(json_.value("ImpactPower", 9.0f), 0.0f);
    hitStopDuration = (std::max)(json_.value("HitStopDuration", 0.05f), 0.0f);
    hitStopScale = (std::clamp)(json_.value("HitStopScale", 0.0f), 0.0f, 1.0f);
    knockBackPower = (std::max)(json_.value("KnockBackPower", 5.0f), 0.0f);
    rearmInterval = (std::max)(json_.value("RearmInterval", 0.8f), 0.01f);
    autoActivate = json_.value("AutoActivate", true);
}

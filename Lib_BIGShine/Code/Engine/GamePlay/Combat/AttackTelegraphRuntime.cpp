#include "PrecompiledHeader.h"
#include "AttackTelegraphRuntime.h"
#include "AttackTemplateManager.h"
#include "Engine/Common/DebugLog.h"
#include "Engine/Debug/TracyProfiler.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"
#include "Engine/GamePlay/System/EffectManager.h"
#include "Engine/Graphics/Particle/ParticleComponent.h"
#include "Engine/Graphics/Resource/GlTFModel.h"
#include "Engine/Graphics/Resource/Animation/AnimationState.h"
#include <algorithm>
#include <cmath>
#include <utility>
#include <imgui.h>
#include <imgui_stdlib.h>

namespace
{
    constexpr float REFERENCE_FPS = 60.0f;
    constexpr float SAME_HIT_TIME_EPSILON = 0.0001f;
}

void AttackTelegraphRuntime::Reset()
{
    canceled = false;
    activeState = nullptr;
    lastTime = -1.0f;
    cues.clear();
    pending.clear();
    cachedModel.reset();
    cachedNodes = nullptr;
    cachedNodeCount = 0;
    nodeIndices.clear();
}

void AttackTelegraphRuntime::Cancel(const AnimState* state_)
{
    BS_PROFILE_SCOPE("AttackTelegraphRuntime::Cancel");
    activeState = state_;
    canceled = true;
    cues.clear();
    pending.clear();
}

void AttackTelegraphRuntime::WarnOnce(const std::string& message_)
{
    if (warnings.insert(message_).second)
    {
        BS_LOG_WARNING(LOG_CATEGORY::GAME, "[AttackTelegraph] " + message_);
    }
}

void AttackTelegraphRuntime::Build(const std::shared_ptr<GameObject>& owner_, const AnimState& state_, float startTime_)
{
    BS_PROFILE_SCOPE("AttackTelegraphRuntime::Build");
    cues.clear();
    activeState = &state_;
    int berserkStage_ = -1;
    if (const auto blackboard_ = owner_->GetComponent<BlackboardComponent>())
    {
        blackboard_->GetInt("HumanBossAI", "BerserkStage", berserkStage_);
    }
    const auto addCue_ = [&](float hitTime_, const AttackTelegraphSettings& settings_)
    {
        if (!settings_.enabled)
        {
            return;
        }
        const float frames_ = berserkStage_ >= 0 ? settings_.berserkLeadFrames : settings_.leadFrames;
        const float warningTime_ = hitTime_ - frames_ / REFERENCE_FPS;
        if (!std::isfinite(warningTime_) || frames_ <= 0.0f || warningTime_ < startTime_)
        {
            WarnOnce(state_.name + ": insufficient startup for telegraph at " + std::to_string(hitTime_));
            return;
        }
        // 同時刻に始まる武器・手足などの判定は、最初の有効な予兆設定にまとめる。
        if (std::any_of(cues.begin(), cues.end(), [hitTime_](const Cue& cue_)
            {
            return std::abs(cue_.hitTime - hitTime_) <= SAME_HIT_TIME_EPSILON;
        }))
        {
            return;
        }
        cues.push_back({ hitTime_, warningTime_, settings_, false });
    };
    auto& templates_ = AttackTemplateManager::Instance();
    for (const auto& event_ : state_.events)
    {
        if (event_.eventTemplateType == ANIMATION_EVENT_TEMPLATE_TYPE::ATTACK_TEMPLATE)
        {
            if (templates_.GetTemplates().empty())
            {
                templates_.LoadAll();
            }
            const auto attack_ = templates_.FindTemplate(event_.attackTemplateId);
            if (!attack_)
            {
                continue;
            }
            if (attack_->weaponHitBox.enabled)
            {
                // Runtime::Updateが有効化する時刻と同じ、イベント開始＋判定内オフセットを使う。
                const float hitTime_ = event_.startTime + attack_->weaponHitBox.activeStartTime;
                if (hitTime_ <= event_.endTime)
                {
                    addCue_(hitTime_, attack_->telegraph);
                }
            }
            // 生成直後に有効な判定は、生成を決めるAnimation Eventの時刻へ正確に結び付けられる。
            // 生成後の遅延はEffect側の時計なので、アニメーション速度から近似しない。
            for (const auto& effect_ : attack_->attackEffects)
            {
                if (!effect_.enabled)
                {
                    continue;
                }
                for (const auto& shape_ : effect_.hitShapes)
                {
                    if (!shape_.enabled || shape_.shape == ATTACK_HIT_SHAPE::NONE)
                    {
                        continue;
                    }
                    if (shape_.activeStartTime <= 0.0f && shape_.activeEndTime >= 0.0f)
                    {
                        const float hitTime_ = event_.startTime + (std::max)(effect_.spawnTimeOffset, 0.0f);
                        if (hitTime_ <= event_.endTime)
                        {
                            addCue_(hitTime_, attack_->telegraph);
                        }
                    }
                    else if (attack_->telegraph.enabled)
                    {
                        WarnOnce(event_.attackTemplateId + ": delayed effect hitbox uses an independent clock; telegraph skipped");
                    }
                }
            }
            for (const auto& slash_ : attack_->slashEffects)
            {
                if (!slash_.enabled || slash_.vfxPath.empty() || !slash_.hitBox.enabled)
                {
                    continue;
                }
                if (slash_.spawnTimeOffset <= 0.0f && slash_.hitBox.activeStartTime <= 0.0f && slash_.hitBox.activeEndTime >= 0.0f)
                {
                    addCue_(event_.startTime, attack_->telegraph);
                }
                else if (attack_->telegraph.enabled)
                {
                    WarnOnce(event_.attackTemplateId + ": delayed slash hitbox uses an independent clock; telegraph skipped");
                }
            }
        }
        else if (event_.telegraphHitStart || (event_.startAction.find("Enable") != std::string::npos &&
            event_.startAction.find("HitBox") != std::string::npos))
        {
            // Dragon等の従来Actionも、Animation Eventが直接有効化する判定だけを対象にする。
            addCue_(event_.startTime, event_.telegraph);
        }
    }
}

void AttackTelegraphRuntime::Advance(const std::shared_ptr<GameObject>& owner_, const AnimState& state_, float previousTime_, float currentTime_)
{
    if (!owner_ || !owner_->manager || owner_->IsPendingDestroy() || owner_->GetLayer() != LayerType::Enemy)
    {
        return;
    }
    BS_PROFILE_SCOPE("AttackTelegraphRuntime::Advance");
    // 中断直後に同じStateを描画し続けても、古い攻撃の予約を復元しない。
    if (canceled && activeState == &state_)
    {
        return;
    }
    // 同じStateのループも新しい区間として扱い、前周の消費状態を持ち越さない。
    if (activeState != &state_ || (state_.isLoop && previousTime_ < lastTime))
    {
        Reset();
        Build(owner_, state_, previousTime_);
    }
    lastTime = currentTime_;
    for (auto& cue_ : cues)
    {
        if (cue_.consumed || currentTime_ < cue_.warningTime)
        {
            continue;
        }
        cue_.consumed = true;
        // 大きな時間飛びで判定時刻まで通過した場合は、遅れた警告を強制表示しない。
        if (previousTime_ > cue_.warningTime || currentTime_ >= cue_.hitTime)
        {
            continue;
        }
        pending.push_back(cue_.settings);
    }
}

void AttackTelegraphRuntime::Flush(const std::shared_ptr<GameObject>& owner_)
{
    if (pending.empty())
    {
        return;
    }
    BS_PROFILE_SCOPE("AttackTelegraphRuntime::Flush");
    if (owner_ && owner_->manager && !owner_->IsPendingDestroy())
    {
        for (const auto& settings_ : pending)
        {
            Spawn(owner_, settings_);
        }
    }
    pending.clear();
}

void AttackTelegraphRuntime::Spawn(const std::shared_ptr<GameObject>& owner_, const AttackTelegraphSettings& settings_)
{
    BS_PROFILE_SCOPE("AttackTelegraphRuntime::Spawn");
    if (!owner_ || !owner_->manager || !owner_->transform || owner_->IsPendingDestroy())
    {
        return;
    }
    DirectX::XMMATRIX base_ = owner_->transform->GetWorldMatrix();
    DirectX::XMFLOAT3 offset_ = settings_.fallbackOffset;
    const auto model_ = owner_->GetComponent<GltfModelCommponennt>();
    if (model_ && model_->IsModelReady() && !settings_.attachNodeName.empty())
    {
        model_->UpdateTransformIfDirty();
        const auto& nodes_ = model_->GetNodes();
        // モデル差替え・再読込後はポインター、要素数、保存済みノード名でキャッシュを検証する。
        if (cachedModel.lock() != model_ || cachedNodes != nodes_.data() || cachedNodeCount != nodes_.size())
        {
            cachedModel = model_;
            cachedNodes = nodes_.data();
            cachedNodeCount = nodes_.size();
            nodeIndices.clear();
        }
        auto found_ = nodeIndices.find(settings_.attachNodeName);
        if (found_ != nodeIndices.end() && found_->second >= 0 &&
            (found_->second >= static_cast<int>(nodes_.size()) || nodes_[found_->second].name != settings_.attachNodeName))
        {
            nodeIndices.erase(found_);
            found_ = nodeIndices.end();
        }
        if (found_ == nodeIndices.end())
        {
            const auto node_ = std::find_if(nodes_.begin(), nodes_.end(), [&](const auto& value_)
                {
                return value_.name == settings_.attachNodeName;
            });
            const int index_ = node_ == nodes_.end() ? -1 : static_cast<int>(node_ - nodes_.begin());
            found_ = nodeIndices.emplace(settings_.attachNodeName, index_).first;
        }
        if (found_->second >= 0)
        {
            base_ = DirectX::XMLoadFloat4x4(&nodes_[found_->second].worldTransform);
            offset_ = settings_.localOffset;
        }
        else
        {
            WarnOnce("Node not found, using fallback: " + settings_.attachNodeName);
        }
    }
    ParticleSpawnRequest request_;
    request_.assetPath = settings_.particleName;
    request_.lifeTime = settings_.lifeTime;
    // 座標として変換することで、オフセットへ頭部の回転・スケール・平行移動を反映する。
    DirectX::XMStoreFloat3(&request_.position, DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&offset_), base_));
    const auto effect_ = owner_->GetManager().GetEffectManager().SpawnParticle(request_);
    if (!effect_)
    {
        WarnOnce("Particle preset unavailable: " + settings_.particleName);
        return;
    }
    if (const auto particle_ = effect_->GetComponent<ParticleComponent>())
    {
        particle_->SetRuntimeVisualScale(settings_.particleScale);
    }
}

void AttackTelegraphRuntime::Preview(const std::shared_ptr<GameObject>& owner_, const AttackTelegraphSettings& settings_)
{
    AttackTelegraphRuntime preview_;
    preview_.Spawn(owner_, settings_);
}

nlohmann::json AttackTelegraphRuntime::SerializeSettings(const AttackTelegraphSettings& settings_)
{
    return { { "enabled", settings_.enabled }, { "leadFrames", settings_.leadFrames },
        { "berserkLeadFrames", settings_.berserkLeadFrames }, { "particleName", settings_.particleName },
        { "attachNodeName", settings_.attachNodeName }, { "localOffset", { settings_.localOffset.x, settings_.localOffset.y, settings_.localOffset.z } },
        { "fallbackOffset", { settings_.fallbackOffset.x, settings_.fallbackOffset.y, settings_.fallbackOffset.z } },
        { "particleScale", settings_.particleScale }, { "lifeTime", settings_.lifeTime } };
}

AttackTelegraphSettings AttackTelegraphRuntime::DeserializeSettings(const nlohmann::json& json_)
{
    AttackTelegraphSettings settings_;
    if (!json_.is_object())
    {
        return settings_;
    }
    const auto readFloat_ = [&](const char* key_, float fallback_, float minimum_, float maximum_)
    {
        const auto entry_ = json_.find(key_);
        if (entry_ == json_.end() || !entry_->is_number())
        {
            return fallback_;
        }
        const float value_ = entry_->get<float>();
        return std::isfinite(value_) ? std::clamp(value_, minimum_, maximum_) : fallback_;
    };
    // 古い設定や型の異なる手編集データは、予兆だけを既定値へ戻して読み込む。
    const auto enabled_ = json_.find("enabled");
    if (enabled_ != json_.end() && enabled_->is_boolean())
    {
        settings_.enabled = enabled_->get<bool>();
    }
    settings_.leadFrames = readFloat_("leadFrames", settings_.leadFrames, 1.0f, 120.0f);
    settings_.berserkLeadFrames = readFloat_("berserkLeadFrames", settings_.berserkLeadFrames, 1.0f, 120.0f);
    const auto particleName_ = json_.find("particleName");
    if (particleName_ != json_.end() && particleName_->is_string())
    {
        settings_.particleName = particleName_->get<std::string>();
    }
    const auto attachNodeName_ = json_.find("attachNodeName");
    if (attachNodeName_ != json_.end() && attachNodeName_->is_string())
    {
        settings_.attachNodeName = attachNodeName_->get<std::string>();
    }
    settings_.particleScale = readFloat_("particleScale", settings_.particleScale, 0.01f, 20.0f);
    settings_.lifeTime = readFloat_("lifeTime", settings_.lifeTime, 0.01f, 5.0f);
    for (const auto& entry_ : { std::make_pair("localOffset", &settings_.localOffset), std::make_pair("fallbackOffset", &settings_.fallbackOffset) })
    {
        const auto value_ = json_.find(entry_.first);
        if (value_ == json_.end() || !value_->is_array() || value_->size() != 3)
        {
            continue;
        }
        bool valid_ = true;
        for (const auto& coordinate_ : *value_)
        {
            valid_ = valid_ && coordinate_.is_number() && std::isfinite(coordinate_.get<float>());
        }
        if (valid_)
        {
            *entry_.second = { (*value_)[0].get<float>(), (*value_)[1].get<float>(), (*value_)[2].get<float>() };
        }
    }
    return settings_;
}

void AttackTelegraphRuntime::DrawSettings(AttackTelegraphSettings& settings_, GameObjectManager* manager_, const std::shared_ptr<GameObject>& owner_)
{
    if (!ImGui::CollapsingHeader("Attack Telegraph"))
    {
        return;
    }
    ImGui::Checkbox("Enable Telegraph", &settings_.enabled);
    ImGui::TextDisabled("ANIMATION_TIME / 60 FPS reference");
    ImGui::TextWrapped("Warnings are skipped when the attack starts before the configured lead time. Independent effect timers are not predicted.");
    ImGui::InputText("Particle Preset", &settings_.particleName);
    if (manager_ && ImGui::BeginCombo("Preset List", settings_.particleName.c_str()))
    {
        for (const auto& name_ : manager_->GetEffectManager().GetParticleAssetKeys())
        {
            if (ImGui::Selectable(name_.c_str(), settings_.particleName == name_))
            {
                settings_.particleName = name_;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::DragFloat("Lead Frames", &settings_.leadFrames, 0.5f, 1.0f, 120.0f);
    ImGui::DragFloat("Berserk Lead Frames", &settings_.berserkLeadFrames, 0.5f, 1.0f, 120.0f);
    ImGui::InputText("Attach Node", &settings_.attachNodeName);
    const auto actor_ = owner_ ? owner_ : (manager_ ? manager_->GetSelectedObject() : nullptr);
    const auto model_ = actor_ ? actor_->GetComponent<GltfModelCommponennt>() : nullptr;
    if (model_ && ImGui::BeginCombo("Node List", settings_.attachNodeName.empty() ? "(Fallback)" : settings_.attachNodeName.c_str()))
    {
        if (ImGui::Selectable("(Fallback)", settings_.attachNodeName.empty()))
        {
            settings_.attachNodeName.clear();
        }
        for (const auto& node_ : model_->GetNodes())
        {
            if (ImGui::Selectable(node_.name.c_str(), settings_.attachNodeName == node_.name))
            {
                settings_.attachNodeName = node_.name;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::DragFloat3("Local Offset", &settings_.localOffset.x, 0.01f);
    ImGui::DragFloat3("Fallback Offset", &settings_.fallbackOffset.x, 0.01f);
    ImGui::DragFloat("Particle Scale", &settings_.particleScale, 0.01f, 0.01f, 20.0f);
    ImGui::DragFloat("Telegraph Life Time", &settings_.lifeTime, 0.01f, 0.01f, 5.0f);
    // 手入力による範囲外の値も、保存時と同じ制約へ戻す。
    settings_ = DeserializeSettings(SerializeSettings(settings_));
    ImGui::BeginDisabled(!actor_);
    if (ImGui::Button("Preview Telegraph"))
    {
        Preview(actor_, settings_);
    }
    ImGui::EndDisabled();
}

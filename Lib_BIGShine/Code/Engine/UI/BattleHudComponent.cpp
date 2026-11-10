#include "PrecompiledHeader.h"
#include "BattleHudComponent.h"
#include "UIWidgetComponent.h"
#include "UIProgressBarComponent.h"
#include "UITextComponent.h"
#include "UISpriteSheetComponent.h"
#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"
#include "WidgetBoardComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Combat/HealthComponent.h"
#include "Engine/GamePlay/Input/KeyConfig.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Graphics/Resource/Sprite.h"
#include "Engine/Physics/HurtBox.h"
#include "Game/Charcter/Player/PlayerControllerComponent.h"
#include "Game/GameMode/GameModeManager.h"
#include "Game/AI/Human/HumanBossAIComponent.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>

namespace
{
    // 装備の分類に対応した素材を選び、登録されていないスキルは空欄へ戻す。
    const char* SkillIcon(const PlayerSkillDefinition* definition_)
    {
        if (!definition_) return "Skills/Skill_Empty";
        switch (definition_->category)
        {
        case SkillCategory::RUSH: return "Skills/Skill_ChargeRound";
        case SkillCategory::LAUNCHER: return "Skills/Skill_LauncherRound";
        case SkillCategory::HEAL: return "Skills/Skill_HealRound";
        case SkillCategory::BUFF: return "Skills/Skill_BuffRound";
        case SkillCategory::PROJECTILE: return "Skills/Skill_MagicRound";
        default: return "Skills/Skill_ComboRound";
        }
    }

    // UI用の補間だけを進め、操作やCooldownの経過には介入しない。
    float Approach(float value_, float target_, float deltaTime_, float duration_)
    {
        const float step_ = (std::max)(0.0f, deltaTime_) / (std::max)(0.001f, duration_);
        return value_ < target_ ? (std::min)(target_, value_ + step_) : (std::max)(target_, value_ - step_);
    }

    // NaNや最大HPが0のデータを描画へ伝搬させず、HP割合を0から1へ収める。
    float HealthRate(const std::shared_ptr<HealthComponent>& health_)
    {
        if (!health_ || !std::isfinite(health_->GetMaxHealth()) || health_->GetMaxHealth() <= 0.0f ||
            !std::isfinite(health_->GetCurrentHealth())) return 0.0f;
        return std::clamp(health_->GetCurrentHealth() / health_->GetMaxHealth(), 0.0f, 1.0f);
    }
}

void BattleHudComponent::Start()
{
    BS_PROFILE_SCOPE("BattleHudComponent::Start");
    inputAtlas.Initialize();
    ResolveElements();
    hudAlpha = 0.0f;
    paletteAlpha = 0.0f;
    const auto owner_ = GetGameObject();
    if (!owner_) return;
    // Object側のAction寿命管理へ登録し、Scene破棄後にthisを参照する通知を残さない。
    const std::weak_ptr<GameObject> weakOwner_ = owner_;
    owner_->RegisterGlobalAction("UI", "Hud_Show", [weakOwner_]()
    {
        const auto owner_ = weakOwner_.lock();
        if (const auto hud_ = owner_ ? owner_->GetComponent<BattleHudComponent>() : nullptr) hud_->requestedVisible = true;
    });
    owner_->RegisterGlobalAction("UI", "Hud_Hide", [weakOwner_]()
    {
        const auto owner_ = weakOwner_.lock();
        if (const auto hud_ = owner_ ? owner_->GetComponent<BattleHudComponent>() : nullptr) hud_->requestedVisible = false;
    });
    for (auto& element_ : elements) ApplyElement(element_, 0.0f);
}

void BattleHudComponent::ResolveElements()
{
    BS_PROFILE_SCOPE("BattleHudComponent::ResolveElements");
    elements.clear();
    elementIndices.clear();
    guideDisplayCache = {};
    actionBindings.clear();
    bindingVisuals.clear();
    guideDeviceInitialized = false;
    guideDeviceAlpha = 0.0f;
    healthSources = {};
    healthBars = {};
    healthTextInitialized = {};
    elements.reserve(80);
    elementIndices.reserve(80);
    for (const auto& object_ : GetManager().GetObjects())
    {
        if (!object_ || object_->IsPendingDestroy()) continue;
        const auto widget_ = object_->GetComponent<UIWidgetComponent>();
        if (!widget_) continue;
        const auto& name_ = object_->GetName();
        const int group_ = name_.find("BossHealth") == 0 || name_ == "BossNamePlate" ? 1 :
            name_.find("PlayerHp") == 0 || name_.find("PlayerFace") == 0 || name_ == "PlayerStatusRoot" || name_.find("HudStatus") == 0 ? 2 :
            name_.find("HudGuide") == 0 ? 3 : 0;
        if (!group_) continue;
        HudElement element_;
        element_.object = object_;
        element_.widget = widget_;
        element_.sprite = object_->GetComponent<SpriteRenderer>();
        element_.text = object_->GetComponent<UITextComponent>();
        if (const auto sprite_ = element_.sprite.lock()) element_.color = sprite_->GetConfiguredColor();
        element_.position = widget_->GetRectTransform().anchoredPosition;
        element_.size = widget_->GetRectTransform().sizeDelta;
        // Scene保存済みの描画用Rectへ倍率を再適用しないよう、保存時の基準配置を優先する。
        if (savedLayouts.contains(name_))
        {
            const auto& layout_ = savedLayouts[name_];
            if (layout_.is_array() && layout_.size() == 4)
            {
                element_.position = { layout_[0].get<float>(),layout_[1].get<float>() };
                element_.size = { layout_[2].get<float>(),layout_[3].get<float>() };
            }
        }
        element_.group = group_;
        elementIndices.emplace(name_,elements.size());
        elements.push_back(element_);
    }
    // HPゲージのComponentは表示要素の解決時に保持し、毎フレームの型検索を避ける。
    const std::array<const char*, 2> barNames_ = { "BossHealthBar","PlayerHpBar" };
    for (std::size_t index_ = 0; index_ < barNames_.size(); ++index_)
        if (const auto element_ = FindElement(barNames_[index_]))
            if (const auto object_ = element_->object.lock()) healthBars[index_] = object_->GetComponent<UIProgressBarComponent>();
}

BattleHudComponent::HudElement* BattleHudComponent::FindElement(const std::string& name_)
{
    const auto iterator_ = elementIndices.find(name_);
    if (iterator_ == elementIndices.end()) return nullptr;
    auto& element_ = elements[iterator_->second];
    if (const auto object_ = element_.object.lock(); object_ && !object_->IsPendingDestroy() && object_->GetName() == name_) return &element_;
    return nullptr;
}

void BattleHudComponent::ApplyElement(HudElement& element_, float alpha_, float brightness_)
{
    const auto object_ = element_.object.lock();
    const auto widget_ = element_.widget.lock();
    if (!object_ || object_->IsPendingDestroy() || !widget_) return;
    const auto& name_ = object_->GetName();
    const float scale_ = element_.group == 1 ? bossScale : element_.group == 2 ? playerScale : guideScale;
    auto& rect_ = widget_->GetRectTransform();
    const auto previousPosition_ = rect_.anchoredPosition;
    const auto previousSize_ = rect_.sizeDelta;
    const auto previousScale_ = rect_.localScale;
    const auto position_ = element_.runtimeLayout ? element_.runtimePosition : element_.position;
    const auto size_ = element_.runtimeLayout ? element_.runtimeSize : element_.size;
    rect_.anchoredPosition = { position_.x * scale_, position_.y * scale_ };
    rect_.sizeDelta = { size_.x * scale_, size_.y * scale_ };
    if (name_ == "BossHealthBar")
    {
        rect_.anchoredPosition = bossPosition;
        rect_.sizeDelta = { bossWidth * scale_, bossHeight * scale_ };
    }
    else if (name_ == "PlayerStatusRoot") rect_.anchoredPosition = playerPosition;
    else if (name_ == "HudGuideRoot") rect_.anchoredPosition = guideDevice == InputDisplayDevice::KEYBOARD_MOUSE ?
        DirectX::XMFLOAT2{guidePosition.x + 80.0f,guidePosition.y - 60.0f} : guidePosition;
    else if (name_ == "BossHealthFrame" || name_ == "BossHealthAwakened")
        rect_.sizeDelta = { (bossWidth + 40.0f) * scale_, (bossHeight + 16.0f) * scale_ };
    if (name_.find("SkillIcon") != std::string::npos || name_.find("CooldownMask") != std::string::npos || name_.find("ReadyFrame") != std::string::npos)
        rect_.localScale = { iconScale,iconScale };
    else if (name_.find("CooldownText") != std::string::npos) rect_.localScale = { cooldownTextScale,cooldownTextScale };
    else if (element_.group == 3 && !element_.text.expired()) rect_.localScale = { labelScale,labelScale };
    // 色のフェードだけで配置を無効化せず、座標や倍率が変わった場合だけ更新を通知する。
    if (previousPosition_.x != rect_.anchoredPosition.x || previousPosition_.y != rect_.anchoredPosition.y ||
        previousSize_.x != rect_.sizeDelta.x || previousSize_.y != rect_.sizeDelta.y ||
        previousScale_.x != rect_.localScale.x || previousScale_.y != rect_.localScale.y) widget_->MarkLayoutDirty();
    if (const auto sprite_ = element_.sprite.lock())
    {
        auto color_ = element_.color;
        // Flashと端部Highlightは既存ProgressBarの演出色を保ち、透明度だけを後段で掛ける。
        if (name_.find("Flash") != std::string::npos || name_.find("EdgeHighlight") != std::string::npos) color_ = sprite_->GetColor();
        color_.x *= brightness_;
        color_.y *= brightness_;
        color_.z *= brightness_;
        color_.w *= std::clamp(alpha_, 0.0f, 1.0f);
        // 実行中のフェード値をSceneの初期色へ混入させない。
        sprite_->SetRuntimeColor(color_);
        // 複合入力の未使用セルを描画せず、非表示の要素によるMapとDrawを増やさない。
        const bool playerHpFill_ = name_ == "PlayerHpCurrent" || name_ == "PlayerHpDamageDelay" || name_ == "PlayerHpHealDelay";
        // 幅が0のゲージをHUDの透明度更新で再表示すると、最小1pxの塗りが残る。
        sprite_->SetRuntimeVisible(color_.w > 0.001f && (!playerHpFill_ || rect_.anchorMax.x > rect_.anchorMin.x));
    }
}

void BattleHudComponent::SetText(const std::string& name_, const std::string& text_)
{
    if (const auto element_ = FindElement(name_))
        if (const auto textComponent_ = element_->text.lock()) textComponent_->SetText(text_);
}

void BattleHudComponent::SetAlpha(const std::string& name_, float alpha_, float brightness_)
{
    if (auto element_ = FindElement(name_)) ApplyElement(*element_, alpha_, brightness_);
}

void BattleHudComponent::UpdateHealth(float deltaTime_)
{
    BS_PROFILE_SCOPE("BattleHudComponent::UpdateHealth");
    auto& manager_ = GetManager();
    const auto mode_ = manager_.GetGameModeManager();
    const bool human_ = mode_ && mode_->GetCurrentBattlePhase() >= static_cast<int>(BossBattlePhase::PHASE_2);
    // ボスの所有権はSceneに置く。名前を解決し直すことで形態移行時の古い参照を使用しない。
    auto boss_ = manager_.Find(human_ ? "HumanBoss" : "TestTarget");
    if (boss_ && (boss_->IsPendingDestroy() || !boss_->IsCombatReady())) boss_.reset();
    const auto bossAI_ = boss_ ? boss_->GetComponent<HumanBossAIComponent>() : nullptr;
    const bool invulnerable_ = bossAI_ && bossAI_->IsBerserkInvulnerable();
    const bool invulnerabilityChanged_ = bossInvulnerable != invulnerable_;
    bossInvulnerable = invulnerable_;
    const auto player_ = manager_.Find("PlayerModel");
    const std::array<std::shared_ptr<GameObject>, 2> actors_ = { boss_, player_ && !player_->IsPendingDestroy() ? player_ : nullptr };
    for (std::size_t index_ = 0; index_ < actors_.size(); ++index_)
    {
        auto& previous_ = index_ == 0 ? previousBoss : previousPlayer;
        const bool actorChanged_ = previous_.lock() != actors_[index_];
        // Objectの切替やComponentの差替え時だけ再取得し、Scene側の所有権は保持しない。
        if (actorChanged_ || healthSources[index_].expired())
            healthSources[index_] = actors_[index_] ? actors_[index_]->GetComponent<HealthComponent>() : nullptr;
        const auto health_ = actors_[index_] ? healthSources[index_].lock() : nullptr;
        const float rate_ = HealthRate(health_);
        if (const auto bar_ = healthBars[index_].lock())
        {
            if (actorChanged_) bar_->ResetDisplayedRate(rate_);
            else bar_->SetTargetRate(rate_);
            // HPの読取と同じフレームに既存の補間処理を進め、時間経過は間引かない。
            if (bar_->IsEnabled() && !bar_->ShouldUpdate()) bar_->Update(deltaTime_);
        }
        previous_ = actors_[index_];
        const float healthValue_ = health_ && std::isfinite(health_->GetCurrentHealth()) ? std::round((std::max)(0.0f,health_->GetCurrentHealth())) : -1.0f;
        const float maxValue_ = health_ && std::isfinite(health_->GetMaxHealth()) ? std::round((std::max)(0.0f,health_->GetMaxHealth())) : -1.0f;
        // 整数の表示値が同じ間は文字列化も行わず、UITextへの更新通知を発生させない。
        if (!healthTextInitialized[index_] || displayedHealth[index_] != healthValue_ || displayedMaxHealth[index_] != maxValue_ || (index_ == 0 && invulnerabilityChanged_))
        {
            char buffer_[96]{};
            if (healthValue_ >= 0.0f && maxValue_ >= 0.0f) std::snprintf(buffer_,sizeof(buffer_),"%.0f / %.0f",healthValue_,maxValue_);
            // 無敵をHP量から推定せず、攻撃受付と同じ実行時状態で明示する。
            SetText(index_ == 0 ? "BossHealthValue" : "PlayerHpValue",
                index_ == 0 && invulnerable_ ? std::string("無敵 | ") + buffer_ : std::string(buffer_));
            displayedHealth[index_] = healthValue_;
            displayedMaxHealth[index_] = maxValue_;
            healthTextInitialized[index_] = true;
        }
        for (auto& element_ : elements)
            if (element_.group == static_cast<int>(index_) + 1) ApplyElement(element_, health_ ? hudAlpha : 0.0f);
    }
    const int style_ = human_ ? 1 : 0;
    if (bossStyle != style_)
    {
        // Texture読込は形態が切り替わった時だけ行い、毎フレームの生成を避ける。
        const std::array<std::pair<const char*, const char*>, 1> paths_ = { {
            { "BossHealthFrame", human_ ? "Boss/BossBar_HumanFrame" : "Boss/BossBar_Frame" }
        } };
        for (const auto& path_ : paths_)
            if (auto element_ = FindElement(path_.first))
                if (const auto sprite_ = element_->sprite.lock())
                {
                    sprite_->LoadTexture(GraphicsDeviceManager::Instance().GetDevice(), std::string("Data/Sprite/UI/Hud/") + path_.second + ".png");
                    const auto size_ = sprite_->GetTextureSize();
                    sprite_->SetUVRect(0,0,size_.x,size_.y);
                }
        SetText("BossNamePlate", human_ ? "黒泥の鏡像" : "断崖の龍");
        bossStyle = style_;
    }
    // 人型AIがBlackboardへ公開したPhaseだけを参照し、HPから暴走条件を推定しない。
    int phase_ = 0;
    if (const auto blackboard_ = boss_ ? boss_->GetComponent<BlackboardComponent>() : nullptr) blackboard_->GetInt("HumanBossAI", "Phase", phase_);
    // 名前欄へ状態説明を連結すると既定幅で折り返すため、無敵はHP値と色で表示する。
    SetText("BossNamePlate", human_ ? "黒泥の鏡像" : "断崖の龍");
    // 両形態の顔素材はモデルと一致しないため表示しない。バー本体とフレームは維持する。
    SetAlpha("BossHealthDecoration",0.0f);
    SetAlpha("BossHealthAwakened", invulnerable_ ? hudAlpha * (0.8f + 0.15f*std::sin(pulseTime*6.0f)) :
        human_ && phase_ >= 3 && boss_ ? hudAlpha * (0.55f + 0.12f*std::sin(pulseTime*2.0f)) : 0.0f);
    if (invulnerable_)
    {
        // Frameも着色するため、HP5％の短い残量でも無敵状態を読み取れる。
        for (const auto name_ : {"BossHealthCurrent","BossHealthFrame","BossHealthValue"})
            if (const auto element_ = FindElement(name_))
                if (const auto sprite_ = element_->sprite.lock())
                    sprite_->SetRuntimeColor({0.72f,0.55f,1.0f,hudAlpha});
    }
}

// 元のScene配置を書き換えず、実行中のデバイス用配置だけを更新する。
void BattleHudComponent::SetGuideLayout(const std::string& name_, DirectX::XMFLOAT2 position_, DirectX::XMFLOAT2 size_)
{
    if (auto element_ = FindElement(name_))
    {
        element_->runtimeLayout = true;
        element_->runtimePosition = position_;
        element_->runtimeSize = size_;
    }
}

const InputCombo& BattleHudComponent::GetGuideBinding(const std::string& action_)
{
    const auto iterator_ = actionBindings.find(action_);
    if (iterator_ != actionBindings.end()) return iterator_->second;
    return actionBindings.emplace(action_,InputIconAtlas::FindBinding(action_,guideDevice)).first->second;
}

const std::string& BattleHudComponent::PresentBinding(const std::string& prefix_, const InputCombo& combo_, float alpha_, DirectX::XMFLOAT2 position_, float size_)
{
    auto& visual_ = bindingVisuals[prefix_];
    const bool changed_ = !visual_.initialized || visual_.combo.size() != combo_.size() ||
        !std::equal(combo_.begin(),combo_.end(),visual_.combo.begin(),[](const InputUnit& left_, const InputUnit& right_)
        { return left_.device == right_.device && left_.code == right_.code; });
    if (changed_)
    {
        visual_.combo = combo_;
        visual_.icons = {};
        visual_.fallback.clear();
        bool missing_ = combo_.empty() || combo_.size() > visual_.icons.size();
        for (std::size_t index_ = 0; index_ < combo_.size() && index_ < visual_.icons.size(); ++index_)
        {
            const std::string name_ = prefix_ + (index_ == 0 ? "" : std::to_string(index_));
            if (const auto element_ = FindElement(name_))
                if (const auto sprite_ = element_->sprite.lock()) visual_.icons[index_] = inputAtlas.Apply(*sprite_,guideDevice,combo_[index_].code);
            missing_ |= !visual_.icons[index_];
        }
        // 複合入力は画像に加えてAND関係を文字でも示す。未収録キーだけの場合も操作名を失わない。
        if (missing_ || combo_.size() > 1)
        {
            for (const auto& unit_ : combo_)
            {
                if (!visual_.fallback.empty()) visual_.fallback += " + ";
                visual_.fallback += InputIconAtlas::GetKeyLabel(guideDevice,unit_.code);
            }
            if (combo_.empty()) visual_.fallback = "未割当";
            visual_.fallback = "[" + visual_.fallback + "] ";
        }
        visual_.initialized = true;
    }
    for (std::size_t index_ = 0; index_ < visual_.icons.size(); ++index_)
    {
        const std::string name_ = prefix_ + (index_ == 0 ? "" : std::to_string(index_));
        SetGuideLayout(name_,{position_.x + static_cast<float>(index_) * (size_ + 4.0f),position_.y},{size_,size_});
        SetAlpha(name_,visual_.icons[index_] ? alpha_ : 0.0f);
    }
    return visual_.fallback;
}

void BattleHudComponent::UpdateGuide(const std::shared_ptr<PlayerControllerComponent>& player_, float deltaTime_)
{
    BS_PROFILE_SCOPE("BattleHudComponent::UpdateGuide");
    const auto& keys_ = KeyConfig::Instance();
    auto desiredDevice_ = keys_.GetActiveDevice();
    if (desiredDevice_ != InputDisplayDevice::KEYBOARD_MOUSE && controllerIconType != 0)
        desiredDevice_ = controllerIconType == 1 ? InputDisplayDevice::XBOX_PAD : InputDisplayDevice::PLAYSTATION_PAD;
    if (previewDevice > 0) desiredDevice_ = static_cast<InputDisplayDevice>(previewDevice - 1);
    if (!guideDeviceInitialized) { guideDevice = desiredDevice_; guideDeviceInitialized = true; }
    // 切替先が変わっても進行中のフェードを巻き戻さず、完全に消えた時点で配置とUVを交換する。
    guideDeviceAlpha = Approach(guideDeviceAlpha,desiredDevice_ == guideDevice ? 1.0f : 0.0f,deltaTime_,0.075f);
    const bool deviceChanged_ = desiredDevice_ != guideDevice && guideDeviceAlpha <= 0.0f;
    if (deviceChanged_) guideDevice = desiredDevice_;
    if (deviceChanged_ || observedBindingsRevision != keys_.GetBindingsRevision())
    {
        actionBindings.clear();
        bindingVisuals.clear();
        observedBindingsRevision = keys_.GetBindingsRevision();
        for (auto& cache_ : guideDisplayCache)
        {
            cache_.normalInitialized = false;
            cache_.skillInitialized = false;
            cache_.hintInitialized = false;
        }
    }
    const bool keyboard_ = guideDevice == InputDisplayDevice::KEYBOARD_MOUSE;
    bool skillHeld_ = previewDevice > 0 && previewSkillPalette;
    bool hasKeyboardModifier_ = false;
    if (player_)
        for (const auto& binding_ : player_->GetSkillInputBindings())
        {
            const auto& modifier_ = GetGuideBinding(binding_.modifierAction);
            hasKeyboardModifier_ |= !modifier_.empty();
            if (!modifier_.empty()) skillHeld_ |= keys_.ActionPressed(binding_.modifierAction);
        }
    // キーボード修飾キーが未設定なら既存の直接スキル入力を併記し、ゲームの設定を補完しない。
    const bool directSkills_ = keyboard_ && !hasKeyboardModifier_;
    paletteAlpha = Approach(paletteAlpha,skillHeld_ ? 1.0f : 0.0f,deltaTime_,paletteFadeDuration);
    const float alpha_ = hudAlpha * guideOpacity * guideDeviceAlpha;
    SetAlpha("HudGuideRoot",alpha_);
    const auto mode_ = GetManager().GetGameModeManager();
    const bool controlLocked_ = !player_ || !player_->IsEnabled() || (mode_ && (mode_->IsPlayerControlLocked() || mode_->IsGameplayLocked()));
    const std::array<const char*, 4> actions_ = { "Player.StrongAttack", "Player.WeakAttack", "Player.JustGuard", "Player.Jump" };
    const std::array<const char*, 4> labels_ = { "強攻撃", "弱攻撃", "パリィ", "ジャンプ" };
    const std::array<PLAYER_ACTION, 4> states_ = { PLAYER_ACTION::STRONG_ATTACK,PLAYER_ACTION::WEAK_ATTACK,PLAYER_ACTION::JUST_GUARD,PLAYER_ACTION::JUMP };
    const auto currentAction_ = player_ ? player_->GetCurrentAction() : PLAYER_ACTION::NONE;
    const auto owner_ = player_ ? player_->GetGameObject() : nullptr;
    const auto health_ = owner_ ? owner_->GetComponent<HealthComponent>() : nullptr;
    const bool dead_ = !health_ || health_->IsDead();
    const auto blackboard_ = owner_ ? owner_->GetComponent<BlackboardComponent>() : nullptr;
    bool comboOpen_ = false;
    if (blackboard_) blackboard_->GetBool(PlayerBlackboardCategory::PLAYER,PlayerBlackboardKey::CAN_COMBO,comboOpen_);
    // 操作名ではなく、現在の割り当てに含まれる物理フェイスボタンから十字位置を決める。
    const auto facePosition_ = [](const InputCombo& combo_, std::size_t index_) -> DirectX::XMFLOAT2
    {
        for (const auto& unit_ : combo_)
            switch (static_cast<PadCode>(unit_.code))
            {
            case PadCode::Y: return {-100,-85}; case PadCode::X: return {-185,0};
            case PadCode::B: return {-15,0}; case PadCode::A: return {-100,85};
            default: break;
            }
        // フェイス以外への再割当は十字へ偽装せず、左側の独立した行へ表示する。
        return {-390,-100 + static_cast<float>(index_) * 70.0f};
    };
    for (std::size_t index_ = 0; index_ < actions_.size(); ++index_)
    {
        const std::string prefix_ = "HudGuide" + std::to_string(index_);
        auto& cache_ = guideDisplayCache[index_];
        const auto& normalBinding_ = GetGuideBinding(actions_[index_]);
        auto position_ = keyboard_ ? DirectX::XMFLOAT2{-160,-220 + static_cast<float>(index_) * 40.0f} : facePosition_(normalBinding_,index_);
        // 同じフェイスへの重複割当は独立行へ逃がし、複数の操作名を重ねて描かない。
        if (!keyboard_)
            for (std::size_t previousIndex_ = 0; previousIndex_ < index_; ++previousIndex_)
            {
                const auto previousPosition_ = facePosition_(GetGuideBinding(actions_[previousIndex_]),previousIndex_);
                if (previousPosition_.x == position_.x && previousPosition_.y == position_.y)
                {
                    position_ = {-390,-100 + static_cast<float>(index_) * 70.0f};
                    break;
                }
            }
        const float normalAlpha_ = alpha_ * (directSkills_ ? 1.0f : 1.0f - paletteAlpha);
        const bool ready_ = !controlLocked_ && !dead_ && player_->CanDisplayActionReady(states_[index_]);
        const bool active_ = !controlLocked_ && !dead_ && (currentAction_ == states_[index_] ||
            (index_ == 0 && (currentAction_ == PLAYER_ACTION::AIR_STRONG_ATTACK || currentAction_ == PLAYER_ACTION::STRONG_CHARGE_ATTACK || currentAction_ == PLAYER_ACTION::AIR_STRONG_CHARGE_ATTACK)) ||
            (index_ == 1 && currentAction_ == PLAYER_ACTION::AIR_WEAK_ATTACK));
        const auto& fallback_ = PresentBinding(prefix_ + "Button",normalBinding_,normalAlpha_ * (ready_ || active_ ? 1.0f : 0.4f),position_,keyboard_ ? 32.0f : 42.0f);
        if (!cache_.normalInitialized || cache_.comboOpen != comboOpen_)
        {
            SetText(prefix_ + "Label",fallback_ + (index_ == 1 && comboOpen_ ? "連撃受付" : labels_[index_]));
            cache_.normalInitialized = true; cache_.comboOpen = comboOpen_;
        }
        SetGuideLayout(prefix_ + "Label",keyboard_ ? DirectX::XMFLOAT2{80,position_.y} : DirectX::XMFLOAT2{position_.x,position_.y + 43},keyboard_ ? DirectX::XMFLOAT2{200,36} : DirectX::XMFLOAT2{160,38});
        SetGuideLayout(prefix_ + "Highlight",position_,{52,52});
        SetAlpha(prefix_ + "Label",normalAlpha_,ready_ || active_ ? 1.0f : 0.85f);
        SetAlpha(prefix_ + "Highlight",normalAlpha_ * (active_ || (ready_ && keys_.ActionPressed(actions_[index_])) ? 1.0f : 0.0f));

        // 4枠目も直接入力と複合入力で共通表示し、回復の待ち時間を確認できるようにする。
        const float skillAlpha_ = alpha_ * (directSkills_ ? 1.0f : paletteAlpha);
        if (skillAlpha_ <= 0.0f)
        {
            for (const char* suffix_ : {"SkillIcon","SkillLabel","CooldownMask","CooldownText","ReadyFrame","SkillButton","SkillButton1","SkillButton2","SkillButton3"}) SetAlpha(prefix_ + suffix_,0.0f);
            continue;
        }
        const PlayerSkillInputBinding* selectedBinding_ = nullptr;
        if (player_ && !directSkills_)
            for (const auto& binding_ : player_->GetSkillInputBindings())
            {
                if (binding_.triggerAction != actions_[index_] || binding_.slotIndex < 0 || binding_.slotIndex >= static_cast<int>(PlayerSkillLoadout::SLOT_COUNT) || GetGuideBinding(binding_.modifierAction).empty()) continue;
                const bool held_ = keys_.ActionPressed(binding_.modifierAction);
                const bool previousHeld_ = selectedBinding_ && keys_.ActionPressed(selectedBinding_->modifierAction);
                if (!selectedBinding_ || (held_ && !previousHeld_) || (held_ == previousHeld_ && binding_.priority > selectedBinding_->priority)) selectedBinding_ = &binding_;
            }
        if (!directSkills_ && !selectedBinding_)
        {
            for (const char* suffix_ : {"SkillIcon","SkillLabel","CooldownMask","CooldownText","ReadyFrame","SkillButton","SkillButton1","SkillButton2","SkillButton3"}) SetAlpha(prefix_ + suffix_,0.0f);
            continue;
        }
        const int slot_ = directSkills_ ? static_cast<int>(index_) : selectedBinding_ ? selectedBinding_->slotIndex : -1;
        const auto skillAction_ = slot_ >= 0 ? PlayerSkillLoadout::GetSlotAction(static_cast<std::size_t>(slot_)) : PLAYER_ACTION::NONE;
        const auto definition_ = player_ && slot_ >= 0 ? player_->GetSkillDefinition(skillAction_) : nullptr;
        // 入力定義は設定変更時に解決済み。ModifierとTriggerの両方を共通のアイコン表示へ渡す。
        const std::string bindingKey_ = directSkills_ ? std::string("Player.Skill") + static_cast<char>('A' + index_) : selectedBinding_ ? selectedBinding_->modifierAction + "+" + selectedBinding_->triggerAction : "";
        if (!cache_.skillInitialized || cache_.skillBinding != bindingKey_)
        {
            InputCombo skillCombo_;
            if (directSkills_) skillCombo_ = GetGuideBinding(bindingKey_);
            else if (selectedBinding_)
            {
                skillCombo_ = GetGuideBinding(selectedBinding_->modifierAction);
                const auto& trigger_ = GetGuideBinding(selectedBinding_->triggerAction);
                if (trigger_.empty()) skillCombo_.clear();
                else skillCombo_.insert(skillCombo_.end(),trigger_.begin(),trigger_.end());
            }
            actionBindings[prefix_ + "SkillCombo"] = std::move(skillCombo_);
            cache_.skillBinding = bindingKey_;
            cache_.skillInitialized = false;
        }
        const auto skillPosition_ = directSkills_ ? DirectX::XMFLOAT2{-160,100 + static_cast<float>(index_) * 46.0f} : keyboard_ ? DirectX::XMFLOAT2{-160,position_.y} : position_;
        const auto& skillFallback_ = PresentBinding(prefix_ + "SkillButton",actionBindings[prefix_ + "SkillCombo"],skillAlpha_,
            directSkills_ || keyboard_ ? DirectX::XMFLOAT2{-112,skillPosition_.y} : DirectX::XMFLOAT2{skillPosition_.x - 28,skillPosition_.y + 42},24.0f);
        const std::string skillName_ = definition_ ? definition_->name : "未設定";
        if (!cache_.skillInitialized || cache_.skillName != skillName_)
        {
            SetText(prefix_ + "SkillLabel",skillFallback_ + skillName_);
            cache_.skillName = skillName_; cache_.skillInitialized = true;
        }
        const float remaining_ = definition_ ? (std::max)(0.0f,player_->GetRemainingSkillCooldown(definition_->name)) : 0.0f;
        const float duration_ = definition_ ? definition_->cooldown : 0.0f;
        const float rate_ = duration_ > 0.0f && std::isfinite(remaining_) && std::isfinite(duration_) ? std::clamp(remaining_ / duration_,0.0f,1.0f) : 0.0f;
        const bool usable_ = definition_ && !actionBindings[prefix_ + "SkillCombo"].empty() && !controlLocked_ && !dead_ && player_->CanDisplayActionReady(skillAction_);
        const bool activeSkill_ = definition_ && currentAction_ == skillAction_;
        const int state_ = !definition_ ? 0 : remaining_ > 0 ? 1 : usable_ ? 2 : 3;
        const float tenths_ = remaining_ > 0.0f && std::isfinite(remaining_) ? std::ceil(remaining_ * 10.0f) : 0.0f;
        if (cache_.cooldownTenths != tenths_ || cache_.skillState != state_)
        {
            char time_[24]{};
            if (tenths_ > 0.0f) std::snprintf(time_,sizeof(time_),"%.1f",tenths_ / 10.0f);
            SetText(prefix_ + "CooldownText",tenths_ > 0.0f ? time_ : state_ == 2 ? "READY" : state_ == 3 ? "UNAVAILABLE" : "");
            cache_.cooldownTenths = tenths_; cache_.skillState = state_;
        }
        const int iconCategory_ = definition_ ? static_cast<int>(definition_->category) : -1;
        if (cache_.iconCategory != iconCategory_)
        {
            if (const auto element_ = FindElement(prefix_ + "SkillIcon"))
                if (const auto sprite_ = element_->sprite.lock())
                {
                    sprite_->LoadTexture(GraphicsDeviceManager::Instance().GetDevice(),std::string("Data/Sprite/UI/Hud/") + SkillIcon(definition_) + ".png");
                    const auto size_ = sprite_->GetTextureSize(); sprite_->SetUVRect(0,0,size_.x,size_.y);
                }
            cache_.iconCategory = iconCategory_;
        }
        for (const char* suffix_ : {"SkillIcon","CooldownMask","ReadyFrame"})
            SetGuideLayout(prefix_ + suffix_,skillPosition_,keyboard_ ? DirectX::XMFLOAT2{38,38} : DirectX::XMFLOAT2{64,64});
        SetGuideLayout(prefix_ + "SkillLabel",keyboard_ ? DirectX::XMFLOAT2{100,skillPosition_.y} : DirectX::XMFLOAT2{skillPosition_.x,skillPosition_.y < -40 ? skillPosition_.y - 66 : skillPosition_.y + 72},keyboard_ ? DirectX::XMFLOAT2{180,42} : DirectX::XMFLOAT2{180,42});
        SetGuideLayout(prefix_ + "CooldownText",skillPosition_,keyboard_ ? DirectX::XMFLOAT2{60,26} : DirectX::XMFLOAT2{80,28});
        if (const auto element_ = FindElement(prefix_ + "CooldownMask"))
            if (const auto sprite_ = element_->sprite.lock()) sprite_->SetUVGridIndex(std::clamp(static_cast<int>(std::ceil(rate_ * 64.0f)),0,64),9,{128,128});
        SetAlpha(prefix_ + "SkillIcon",skillAlpha_,remaining_ > 0.0f ? 0.45f : usable_ || activeSkill_ ? 1.0f : 0.55f);
        SetAlpha(prefix_ + "SkillLabel",skillAlpha_);
        SetAlpha(prefix_ + "CooldownMask",remaining_ > 0.0f ? skillAlpha_ : 0.0f);
        SetAlpha(prefix_ + "CooldownText",skillAlpha_);
        SetAlpha(prefix_ + "ReadyFrame",usable_ || activeSkill_ ? skillAlpha_ * (0.7f + 0.15f * std::sin(pulseTime * 2.5f)) : 0.0f);
    }
    const std::array<const char*,4> hintActions_ = {"Player.Dodge","Player.Guard","Player.SkillModifier","Player.LockOn"};
    const std::array<const char*,4> hintLabels_ = {"回避","ガード","スキル（長押し）","ロックオン切替"};
    std::size_t visibleHintIndex_ = 0;
    for (std::size_t index_ = 0; index_ < hintActions_.size(); ++index_)
    {
        const std::string prefix_ = "HudGuideHint" + std::to_string(index_);
        const auto& combo_ = GetGuideBinding(hintActions_[index_]);
        // 現在のデバイスに割り当てがない案内は行全体を隠し、表示する行だけを詰めて配置する。
        const float hintAlpha_ = combo_.empty() ? 0.0f : alpha_;
        const auto position_ = keyboard_ ? DirectX::XMFLOAT2{-160,-60 + static_cast<float>(visibleHintIndex_) * 40.0f} : DirectX::XMFLOAT2{50,-90 + static_cast<float>(visibleHintIndex_) * 60.0f};
        if (!combo_.empty()) ++visibleHintIndex_;
        const auto& fallback_ = PresentBinding(prefix_ + "Button",combo_,hintAlpha_,keyboard_ ? position_ : DirectX::XMFLOAT2{position_.x,position_.y - 10},keyboard_ ? 32.0f : 24.0f);
        auto& cache_ = guideDisplayCache[index_];
        if (!cache_.hintInitialized)
        {
            SetText(prefix_ + "Text",combo_.empty() ? std::string{} : fallback_ + hintLabels_[index_]); cache_.hintInitialized = true;
        }
        SetGuideLayout(prefix_ + "Text",{keyboard_ ? 80.0f : 150.0f,position_.y + (keyboard_ ? 0.0f : 15.0f)},{200,keyboard_ ? 42.0f : 28.0f});
        SetGuideLayout(prefix_ + "Background",{keyboard_ ? 0.0f : 150.0f,position_.y},{keyboard_ ? 380.0f : 240.0f,44});
        SetAlpha(prefix_ + "Text",hintAlpha_);
        SetAlpha(prefix_ + "Background",hintAlpha_ * 0.6f);
    }
    bool guard_ = false;
    bool justGuard_ = false;
    if (blackboard_)
    {
        blackboard_->GetBool(PlayerBlackboardCategory::PLAYER,PlayerBlackboardKey::IS_GUARD,guard_);
        blackboard_->GetBool(PlayerBlackboardCategory::PLAYER,PlayerBlackboardKey::JUST_GUARD_COUNTER_TAG,justGuard_);
    }
    const auto hurtbox_ = owner_ ? owner_->GetComponent<HurtBox>() : nullptr;
    SetAlpha("HudStatusGuard",hudAlpha * (guard_ ? 1.0f : 0.0f));
    SetAlpha("HudStatusJustGuard",hudAlpha * (justGuard_ ? 1.0f : 0.0f));
    SetAlpha("HudStatusInvincible",hudAlpha * (hurtbox_ && hurtbox_->IsInvincible() ? 1.0f : 0.0f));
}

// 表情表示は戦闘状態を読むだけに留め、TriggerやCounterTagを消費しない。
void BattleHudComponent::UpdatePortrait(const std::shared_ptr<PlayerControllerComponent>& player_, float deltaTime_)
{
    BS_PROFILE_SCOPE("BattleHudComponent::UpdatePortrait");
    const auto element_ = FindElement("PlayerFaceIcon");
    const auto face_ = element_ ? element_->object.lock() : nullptr;
    const auto sheet_ = face_ ? face_->GetComponent<UISpriteSheetComponent>() : nullptr;
    if (!sheet_) return;
    const auto owner_ = player_ ? player_->GetGameObject() : nullptr;
    const auto health_ = owner_ ? owner_->GetComponent<HealthComponent>() : nullptr;
    if (!owner_ || !health_ || !std::isfinite(health_->GetCurrentHealth()))
    {
        sheet_->ClearRuntimeCell();
        portraitPlayer.reset(); portraitHealth.reset();
        portraitPreviousHealth = -1.0f;
        portraitHoldTime = 0.0f;
        return;
    }
    const bool changed_ = portraitPlayer.lock() != owner_ || portraitHealth.lock() != health_;
    const float healthValue_ = health_->GetCurrentHealth();
    if (changed_)
    {
        portraitPlayer = owner_; portraitHealth = health_;
        portraitPreviousHealth = healthValue_;
        portraitHoldTime = 0.0f;
        portraitJustGuard = portraitParried = false;
        portraitAttackInstance = portraitGuardInstance = portraitGuardHitGroup = 0;
    }
    portraitHoldTime = (std::max)(0.0f, portraitHoldTime - (std::max)(0.0f, deltaTime_));
    bool justGuard_ = false, counter_ = false, parried_ = false;
    int attackInstance_ = 0, guardInstance_ = 0, guardHitGroup_ = 0;
    bool guardSucceeded_ = false;
    const auto blackboard_ = owner_->GetComponent<BlackboardComponent>();
    if (blackboard_)
    {
        blackboard_->GetBool(PlayerBlackboardCategory::PLAYER, PlayerBlackboardKey::JUST_GUARD_COUNTER_TAG, justGuard_);
        blackboard_->GetBool(PlayerBlackboardCategory::PLAYER, PlayerBlackboardKey::COUNTER_TAG, counter_);
        blackboard_->GetBool("Guard", "JustGuardSucceeded", guardSucceeded_);
        blackboard_->GetInt("Guard", "AttackInstanceId", guardInstance_);
        blackboard_->GetInt("Guard", "HitGroupId", guardHitGroup_);
        blackboard_->GetBool("GuardReaction", "AttackJustGuarded", parried_);
        blackboard_->GetInt("GuardReaction", "AttackInstanceId", attackInstance_);
    }
    const bool guardEvent_ = guardSucceeded_ && (guardInstance_ != portraitGuardInstance || guardHitGroup_ != portraitGuardHitGroup);
    // ダメージを最優先の一時表情とし、同じ成功フラグを毎フレーム延長しない。
    if (healthValue_ < portraitPreviousHealth) { portraitTransient = 1; portraitHoldTime = portraitHoldDuration; }
    else if (parried_ && (!portraitParried || attackInstance_ != portraitAttackInstance))
        { portraitTransient = 2; portraitHoldTime = portraitHoldDuration; }
    else if (healthValue_ > portraitPreviousHealth || guardEvent_ || (justGuard_ && !portraitJustGuard))
        { portraitTransient = 3; portraitHoldTime = portraitHoldDuration; }
    portraitPreviousHealth = healthValue_;
    portraitJustGuard = justGuard_; portraitParried = parried_; portraitAttackInstance = attackInstance_;
    portraitGuardInstance = guardInstance_; portraitGuardHitGroup = guardHitGroup_;
    const auto action_ = player_->GetCurrentAction();
    const bool skill_ = action_ == PLAYER_ACTION::SKILL_A || action_ == PLAYER_ACTION::SKILL_B ||
        action_ == PLAYER_ACTION::SKILL_C || action_ == PLAYER_ACTION::SKILL_D;
    const auto definition_ = skill_ ? player_->GetSkillDefinition(action_) : nullptr;
    // 死亡・瀕死を基準に、一時反応、回復スキル、攻撃スキルと反撃待機の順で選ぶ。
    int expression_ = 0;
    if (health_->IsDead()) expression_ = 2;
    else if (portraitHoldTime > 0.0f) expression_ = portraitTransient;
    else if (HealthRate(health_) <= portraitLowHealthRate) expression_ = 2;
    else if (definition_ && definition_->category == SkillCategory::HEAL) expression_ = 3;
    else if (skill_ || counter_ || action_ == PLAYER_ACTION::COUNTER_ATTACK) expression_ = 4;
    sheet_->SetRuntimeCell(portraitColumns[expression_]);
}

bool BattleHudComponent::UpdatesPlayerHealthBar(const UIProgressBarComponent* bar_) const
{
    const auto owner_ = gameObject.lock();
    return IsEnabled() && IsStarted() && ShouldLateUpdate() && !freezeHudInformation &&
        owner_ && !owner_->IsPendingDestroy() && healthBars[1].lock().get() == bar_;
}

void BattleHudComponent::LateUpdate(float deltaTime_)
{
    BS_PROFILE_SCOPE("BattleHudComponent::LateUpdate");
    BS_PROFILE_PLOT("HUD.InformationUpdatesEnabled",freezeHudInformation ? 0 : 1);
    // 情報転送の負荷を切り分けるため、既存のSpriteとTextを残して更新だけ停止する。
    if (freezeHudInformation) return;
    pulseTime += (std::max)(0.0f, deltaTime_);
    const auto mode_ = GetManager().GetGameModeManager();
    const auto sourceObject_ = GetManager().Find("UI");
    const auto sourceBoard_ = sourceObject_ ? sourceObject_->GetComponent<WidgetBoardComponent>() : nullptr;
    // 既存UI_Show/HideによるBoard抑制を受け取り、別Board上のHUDをフェードさせる。
    const bool sourceVisible_ = !sourceBoard_ || (sourceBoard_->IsEnabled() && sourceBoard_->IsVisible());
    const bool battleFinished_ = mode_ && mode_->GetCurrentBattlePhase() == static_cast<int>(BossBattlePhase::DEFEATED);
    const bool visible_ = requestedVisible && sourceVisible_ && !battleFinished_ &&
        !(hideDuringPresentation && mode_ && mode_->IsPresentationPlaying());
    hudAlpha = Approach(hudAlpha,visible_ ? 1.0f : 0.0f,deltaTime_,fadeDuration);
    UpdateHealth(deltaTime_);
    const auto playerObject_ = GetManager().Find("PlayerModel");
    const auto player_ = playerObject_ && !playerObject_->IsPendingDestroy() ? playerObject_->GetComponent<PlayerControllerComponent>() : nullptr;
    UpdateGuide(player_,deltaTime_);
    UpdatePortrait(player_,deltaTime_);
}

void BattleHudComponent::Gui()
{
    BS_PROFILE_SCOPE("BattleHudComponent::Gui");
    ImGui::Text("Active Device: %s",InputIconAtlas::GetDeviceName(KeyConfig::Instance().GetActiveDevice()));
    ImGui::Text("Controller: %s",ControllerInput::Instance().IsConnected() ? ControllerInput::Instance().GetName().c_str() : "Disconnected");
    ImGui::Combo("Controller Icon Type",&controllerIconType,"AUTO\0XBOX\0PLAYSTATION\0");
    ImGui::Combo("Guide Preview",&previewDevice,"Live input\0Keyboard & Mouse\0Xbox\0PlayStation\0");
    ImGui::Checkbox("Preview Skill Palette",&previewSkillPalette);
    ImGui::Text("Displayed Device: %s",InputIconAtlas::GetDeviceName(guideDevice));
    inputAtlas.Initialize();
    inputAtlas.Gui(previewDevice > 0 ? static_cast<InputDisplayDevice>(previewDevice - 1) : guideDevice);
    ImGui::Checkbox("HUD Visible",&requestedVisible);
    ImGui::SliderFloat("Portrait Low HP Rate", &portraitLowHealthRate, 0.01f, 0.9f);
    ImGui::SliderFloat("Portrait Reaction Hold", &portraitHoldDuration, 0.1f, 2.0f);
    const char* PORTRAIT_LABELS[] = { "Normal Cell", "Damage Cell", "Critical / Parried Cell", "Heal / Just Guard Cell", "Skill / Counter Cell" };
    for (std::size_t index_ = 0; index_ < portraitColumns.size(); ++index_)
        if (ImGui::InputInt(PORTRAIT_LABELS[index_], &portraitColumns[index_])) portraitColumns[index_] = std::clamp(portraitColumns[index_], 0, 255);
    ImGui::Checkbox("Freeze HUD Information (keep drawing)",&freezeHudInformation);
    ImGui::Checkbox("Hide During Presentation",&hideDuringPresentation);
    ImGui::DragFloat2("Boss Position",&bossPosition.x,1.0f);
    ImGui::DragFloat("Boss Scale",&bossScale,0.01f,0.25f,2.0f);
    ImGui::DragFloat("Boss Width",&bossWidth,1.0f,100.0f,1200.0f);
    ImGui::DragFloat("Boss Height",&bossHeight,0.5f,4.0f,24.0f);
    ImGui::DragFloat2("Player Position",&playerPosition.x,1.0f);
    ImGui::DragFloat("Player Scale",&playerScale,0.01f,0.25f,2.0f);
    ImGui::DragFloat2("Guide Position",&guidePosition.x,1.0f);
    ImGui::DragFloat("Guide Scale",&guideScale,0.01f,0.25f,1.5f);
    ImGui::SliderFloat("Guide Opacity",&guideOpacity,0.0f,1.0f);
    ImGui::DragFloat("Skill Icon Scale",&iconScale,0.01f,0.5f,1.5f);
    ImGui::DragFloat("Cooldown Text Scale",&cooldownTextScale,0.01f,0.5f,1.5f);
    ImGui::DragFloat("Guide Label Scale",&labelScale,0.01f,0.5f,1.5f);
    ImGui::DragFloat("HUD Fade Duration",&fadeDuration,0.01f,0.01f,2.0f);
    ImGui::DragFloat("Palette Fade Duration",&paletteFadeDuration,0.01f,0.01f,0.5f);
}

void BattleHudComponent::Serialize(nlohmann::json& json_) const
{
    BS_PROFILE_SCOPE("BattleHudComponent::Serialize");
    json_ = { {"BossPosition",{bossPosition.x,bossPosition.y}}, {"PlayerPosition",{playerPosition.x,playerPosition.y}},
        {"GuidePosition",{guidePosition.x,guidePosition.y}}, {"BossWidth",bossWidth}, {"BossHeight",bossHeight},
        {"BossScale",bossScale}, {"PlayerScale",playerScale}, {"GuideScale",guideScale}, {"GuideOpacity",guideOpacity},
        {"IconScale",iconScale}, {"CooldownTextScale",cooldownTextScale}, {"LabelScale",labelScale},
        {"ControllerIconType",controllerIconType}, {"FadeDuration",fadeDuration}, {"PaletteFadeDuration",paletteFadeDuration}, {"HideDuringPresentation",hideDuringPresentation} };
    json_["PortraitColumns"] = portraitColumns;
    json_["PortraitLowHealthRate"] = portraitLowHealthRate;
    json_["PortraitHoldDuration"] = portraitHoldDuration;
    json_["ElementLayouts"] = nlohmann::json::object();
    for (const auto& element_ : elements)
        if (const auto object_ = element_.object.lock(); object_ && !object_->IsPendingDestroy())
            json_["ElementLayouts"][object_->GetName()] = { element_.position.x,element_.position.y,element_.size.x,element_.size.y };
}

void BattleHudComponent::Deserialize(const nlohmann::json& json_)
{
    BS_PROFILE_SCOPE("BattleHudComponent::Deserialize");
    // 既存データに項目がない場合は宣言時の既定値を保つ。
    const auto readPosition_ = [&json_](const char* key_, DirectX::XMFLOAT2& position_)
    {
        if (json_.contains(key_) && json_[key_].is_array() && json_[key_].size() == 2)
            position_ = { json_[key_][0].get<float>(),json_[key_][1].get<float>() };
    };
    readPosition_("BossPosition",bossPosition);
    readPosition_("PlayerPosition",playerPosition);
    readPosition_("GuidePosition",guidePosition);
    bossWidth = std::clamp(json_.value("BossWidth",bossWidth),100.0f,1200.0f);
    bossHeight = std::clamp(json_.value("BossHeight",bossHeight),4.0f,24.0f);
    bossScale = std::clamp(json_.value("BossScale",bossScale),0.25f,2.0f);
    playerScale = std::clamp(json_.value("PlayerScale",playerScale),0.25f,2.0f);
    guideScale = std::clamp(json_.value("GuideScale",guideScale),0.25f,1.5f);
    guideOpacity = std::clamp(json_.value("GuideOpacity",guideOpacity),0.0f,1.0f);
    iconScale = std::clamp(json_.value("IconScale",iconScale),0.5f,1.5f);
    cooldownTextScale = std::clamp(json_.value("CooldownTextScale",cooldownTextScale),0.5f,1.5f);
    labelScale = std::clamp(json_.value("LabelScale",labelScale),0.5f,1.5f);
    fadeDuration = (std::max)(0.01f,json_.value("FadeDuration",fadeDuration));
    paletteFadeDuration = (std::max)(0.01f,json_.value("PaletteFadeDuration",paletteFadeDuration));
    controllerIconType = std::clamp(json_.value("ControllerIconType",0),0,2);
    hideDuringPresentation = json_.value("HideDuringPresentation",hideDuringPresentation);
    portraitLowHealthRate = std::clamp(json_.value("PortraitLowHealthRate", 0.25f), 0.01f, 0.9f);
    portraitHoldDuration = std::clamp(json_.value("PortraitHoldDuration", 0.8f), 0.1f, 2.0f);
    if (json_.contains("PortraitColumns") && json_["PortraitColumns"].is_array() && json_["PortraitColumns"].size() == portraitColumns.size())
        for (std::size_t index_ = 0; index_ < portraitColumns.size(); ++index_)
            if (json_["PortraitColumns"][index_].is_number_integer()) portraitColumns[index_] = std::clamp(json_["PortraitColumns"][index_].get<int>(), 0, 255);
    savedLayouts = json_.value("ElementLayouts",nlohmann::json::object());
    if (!savedLayouts.is_object()) savedLayouts = nlohmann::json::object();
}

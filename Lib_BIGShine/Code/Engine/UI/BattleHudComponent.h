#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include <array>
#include <vector>
#include <unordered_map>
#include "InputIconAtlas.h"

class UIWidgetComponent;
class SpriteRenderer;
class UITextComponent;
class PlayerControllerComponent;
class HealthComponent;
class UIProgressBarComponent;

// 既存Widgetで構成した戦闘HUDへ、確定済みの戦闘状態と入力設定を表示する。
class BattleHudComponent : public Component
{
public:
    const std::string& GetName() const override
    {
        static const std::string NAME = "BattleHudComponent";
        return NAME;
    }
    // Sceneに保存されたWidgetを参照し、表示専用Actionを登録する。
    void Start() override;
    // 全ComponentのUpdate後にHP・入力・演出状態を反映する。
    void LateUpdate(float deltaTime_) override;
    bool UpdatesPlayerHealthBar(const UIProgressBarComponent* bar_) const;
    // 位置・大きさ・透明度を既存Inspectorから調整する。
    void Gui() override;
    // 配置設定だけを保存し、Object参照や演出タイマーは保存しない。
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

private:
    // 描画対象の寿命を延ばさず、各Widgetの初期色と配置を保持する。
    struct HudElement
    {
        std::weak_ptr<GameObject> object;
        std::weak_ptr<UIWidgetComponent> widget;
        std::weak_ptr<SpriteRenderer> sprite;
        std::weak_ptr<UITextComponent> text;
        DirectX::XMFLOAT4 color{ 1,1,1,1 };
        DirectX::XMFLOAT2 position{ 0,0 };
        DirectX::XMFLOAT2 size{ 0,0 };
        // デバイス用の一時配置は保存対象の基準配置から分離する。
        DirectX::XMFLOAT2 runtimePosition{ 0,0 };
        DirectX::XMFLOAT2 runtimeSize{ 0,0 };
        bool runtimeLayout = false;
        int group = 0;
    };
    // 文字列化やTexture更新を、表示内容が変わった場合だけに限定するための実行時キャッシュ。
    struct GuideDisplayCache
    {
        std::string binding;
        std::string skillName;
        std::string skillModifier;
        std::string skillBinding;
        std::string hintBinding;
        float cooldownTenths = -1.0f;
        int skillState = -1;
        int iconCategory = -2;
        bool comboOpen = false;
        bool normalInitialized = false;
        bool skillInitialized = false;
        bool hintInitialized = false;
    };
    // Scene内の表示専用Objectを一度解決する。
    void ResolveElements();
    // 名前が一致する表示要素を探す。
    HudElement* FindElement(const std::string& name_);
    // 所属グループの表示倍率を、既存Anchorに対するOffsetとSizeへ反映する。
    void ApplyElement(HudElement& element_, float alpha_, float brightness_ = 1.0f);
    // 装備と実入力バインドを読み、通常表示からスキル表示へ補間する。
    void UpdateGuide(const std::shared_ptr<PlayerControllerComponent>& player_, float deltaTime_);
    // 確定済みのHP・戦闘状態から、優先順位と保持時間を考慮して表情を選ぶ。
    void UpdatePortrait(const std::shared_ptr<PlayerControllerComponent>& player_, float deltaTime_);
    // HPを安全に取得し、形態が変わった時は遅延ゲージも初期化する。
    void UpdateHealth(float deltaTime_);
    // キャッシュ済みのTextを、内容が変化した時だけ更新する。
    void SetText(const std::string& name_, const std::string& text_);
    // 表示要素の透明度を指定し、完全に透明な要素は描画対象から外す。
    void SetAlpha(const std::string& name_, float alpha_, float brightness_ = 1.0f);
    // 配置の基準値を保存したまま、操作ガイドだけを一時的に並べ替える。
    void SetGuideLayout(const std::string& name_, DirectX::XMFLOAT2 position_, DirectX::XMFLOAT2 size_);
    const InputCombo& GetGuideBinding(const std::string& action_);
    // 割り当てが変わった時だけUVを設定し、毎フレームは配置と透明度のみ更新する。
    const std::string& PresentBinding(const std::string& prefix_, const InputCombo& combo_, float alpha_, DirectX::XMFLOAT2 position_, float size_);

    struct BindingVisual
    {
        InputCombo combo;
        std::array<bool, 4> icons{};
        std::string fallback;
        bool initialized = false;
    };
    InputIconAtlas inputAtlas;
    std::unordered_map<std::string, InputCombo> actionBindings;
    std::unordered_map<std::string, BindingVisual> bindingVisuals;
    std::uint64_t observedBindingsRevision = 0;
    InputDisplayDevice guideDevice = InputDisplayDevice::KEYBOARD_MOUSE;
    // 手動指定と未接続プレビューは実際の入力元へ書き戻さない。
    int controllerIconType = 0;
    int previewDevice = 0;
    bool previewSkillPalette = false;
    bool guideDeviceInitialized = false;
    float guideDeviceAlpha = 0.0f;

    // 通常、被弾、瀕死・被ジャストガード、回復・成功、スキル・反撃の順にセルを指定する。
    std::array<int, 5> portraitColumns{ 0,1,2,3,4 };
    float portraitLowHealthRate = 0.25f;
    float portraitHoldDuration = 0.8f;
    float portraitHoldTime = 0.0f;
    float portraitPreviousHealth = -1.0f;
    int portraitTransient = 0;
    bool portraitJustGuard = false;
    bool portraitParried = false;
    int portraitAttackInstance = 0;
    int portraitGuardInstance = 0;
    int portraitGuardHitGroup = 0;
    std::weak_ptr<GameObject> portraitPlayer;
    std::weak_ptr<HealthComponent> portraitHealth;
    std::vector<HudElement> elements;
    // 名前から直接表示要素へ到達し、毎フレームの全要素走査とweak_ptrのlockを避ける。
    std::unordered_map<std::string, std::size_t> elementIndices;
    std::array<GuideDisplayCache, 4> guideDisplayCache;
    std::array<std::weak_ptr<HealthComponent>, 2> healthSources;
    std::array<std::weak_ptr<UIProgressBarComponent>, 2> healthBars;
    std::array<float, 2> displayedHealth{ -1.0f,-1.0f };
    std::array<float, 2> displayedMaxHealth{ -1.0f,-1.0f };
    std::array<bool, 2> healthTextInitialized{ false,false };
    // 診断専用。描画を維持したまま情報取得を止める。Sceneには保存しない。
    bool freezeHudInformation = false;
    DirectX::XMFLOAT2 bossPosition{ 0,55 };
    DirectX::XMFLOAT2 playerPosition{ 60,-120 };
    DirectX::XMFLOAT2 guidePosition{ -340,-250 };
    float bossWidth = 850.0f;
    float bossHeight = 12.0f;
    float bossScale = 1.0f;
    float playerScale = 1.0f;
    float guideScale = 1.0f;
    float guideOpacity = 0.94f;
    float iconScale = 1.0f;
    float cooldownTextScale = 1.0f;
    float labelScale = 1.0f;
    float fadeDuration = 0.18f;
    float paletteFadeDuration = 0.15f;
    float hudAlpha = 0.0f;
    float paletteAlpha = 0.0f;
    float pulseTime = 0.0f;
    bool requestedVisible = true;
    bool hideDuringPresentation = true;
    std::weak_ptr<GameObject> previousBoss;
    std::weak_ptr<GameObject> previousPlayer;
    int bossStyle = -1;
    // HP値が不変でも無敵の開始・終了で表示を更新する。
    bool bossInvulnerable = false;
    std::array<std::string, 4> iconPaths;
    std::array<std::string, 4> buttonPaths;
    std::array<std::string, 4> hintPaths;
    nlohmann::json savedLayouts = nlohmann::json::object();
};

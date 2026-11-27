#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>
#include <DirectXMath.h>
class GameObject;
class GameObjectManager;
class SpriteRenderSystem;
struct ImVec2;

// ポーズと死亡画面で、入力受付と二択メニューの演出を共通管理する。
class MenuPresentation
{
public:
    // Sceneが所有するManagerを借用し、表示専用Objectを通常更新・保存から分離する。
    void Initialize(GameObjectManager& manager_);
    // 押しっぱなしの決定を受け付けず、先頭項目から表示を始める。
    void Reset();
    // 戦闘の時計に依存せず、選択移動と決定演出を進める。
    void Update(float deltaTime_);
    // UIWidget・UIText・UIVisualEffectで構成したメニューをSprite描画へ提出する。
    void Draw(SpriteRenderSystem& sprites_, const char* title_, const char* firstLabel_, bool death_, const char* secondLabel_ = "タイトルに戻る");
    // 死亡演出の暗転も通常UIと同じ描画経路へ提出する。
    void DrawBackdrop(SpriteRenderSystem& sprites_, float alpha_);
    // 決定演出と入力の解放が完了した項目を一度だけ取得する。
    int TakeResult(const std::string& releaseInputAction_ = {});
    // ESCなどのキャンセル操作も、再開ボタンと同じ決定演出へ渡す。
    void Confirm(int index_);
    // 既存のCurveアセット形式から、登場と選択の補間値を取得する。
    static float Reveal(float ratio_);
    static float Focus(float ratio_);
    // 次回のメニュー表示から、Curve Editorで保存した調整を反映する。
    static void ReloadCurves();
    // タイトルSceneの既存決定フィードバックを描画する。
    static void DrawFeedback(const ImVec2& minimum_, const ImVec2& maximum_, float progress_, float alpha_, bool death_);

    // 同名の表示要素を再利用し、文字が変わった時だけTextureを再生成する。
    void SubmitVisual(SpriteRenderSystem& sprites_, const char* name_, float x_, float y_, float width_, float height_,
        const DirectX::XMFLOAT4& color_, const char* text_ = nullptr, float fontSize_ = 26.0f, const char* preset_ = nullptr, float strength_ = 1.0f);

private:
    // 選択変更時だけ効果音を鳴らし、拡縮の時計を戻す。
    void Select(int index_);

    // 表示専用Objectの寿命をメニューに限定し、シーン保存へ混入させない。
    struct MenuVisual
    {
        std::string name;
        std::string text;
        std::shared_ptr<GameObject> object;
    };

    GameObjectManager* manager = nullptr;
    std::vector<MenuVisual> visuals;
    DirectX::XMFLOAT2 previousMouse{ 0,0 };
    float elapsed = 0.0f;
    float selectionTime = 0.0f;
    float confirmTime = 0.0f;
    float repeatTime = 0.0f;
    std::array<float, 2> selection{};
    int selectedIndex = 0;
    int heldDirection = 0;
    int confirmedIndex = -1;
    bool armed = false;
    bool delivered = false;
};

#include "PrecompiledHeader.h"
#include "MenuPresentation.h"
#include "UIWidgetComponent.h"
#include "UITextComponent.h"
#include "UIVisualEffectComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Graphics/RenderSystem/SpriteRenderSystem.h"
#include "Engine/UI/ScreenTransitionManager.h"
#include "Engine/Math/Curve/CurveComponent.h"
#include "Engine/Debug/TracyProfiler.h"
#include "Engine/Audio/AudioManager.h"
#include "Engine/GamePlay/Input/KeyConfig.h"
#include "Engine/GamePlay/Input/KeyInput.h"
#include "Engine/GamePlay/Input/MouseInput.h"
#include "Engine/GamePlay/Input/ControllerInput.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iterator>
#include <utility>

namespace
{
    // 所有GameObjectを必要としないCurve読込・評価APIを、メニュー間で共有する。
    CurveComponent& GetMenuCurves()
    {
        static CurveComponent curves_;
        return curves_;
    }

    // 配布データに音源がある場合だけ、既存のタイトル操作音を再利用する。
    void PlayMenuSound(bool confirm_)
    {
        const char* path_ = confirm_ ? "Data/Audio/SE/Sword-Fight-1.wav" : "Data/Audio/SE/Sword-Practice-2.wav";
        std::error_code error_;
        if (std::filesystem::is_regular_file(path_, error_)) AudioManager::Instance().PlaySEByPath(path_);
    }


}

float MenuPresentation::Reveal(float ratio_)
{
    const float time_ = std::clamp(ratio_, 0.0f, 1.0f);
    auto& curves_ = GetMenuCurves();
    return std::clamp(curves_.HasTrack("UI.MenuReveal") ? curves_.GetValue("UI.MenuReveal", time_) :
        1.0f - std::pow(1.0f - time_, 3.0f), 0.0f, 1.0f);
}

float MenuPresentation::Focus(float ratio_)
{
    const float time_ = std::clamp(ratio_, 0.0f, 1.0f);
    auto& curves_ = GetMenuCurves();
    return curves_.HasTrack("UI.MenuFocus") ? curves_.GetValue("UI.MenuFocus", time_) : Reveal(time_);
}

void MenuPresentation::ReloadCurves()
{
    BS_PROFILE_SCOPE("MenuPresentation::ReloadCurves");
    auto& curves_ = GetMenuCurves();
    curves_.LoadTrackAsset("Data/Curve/UI.MenuReveal.json");
    curves_.LoadTrackAsset("Data/Curve/UI.MenuFocus.json");
}

// 表示ObjectはSceneへ登録せず、所有元の破棄とともに解放する。
void MenuPresentation::Initialize(GameObjectManager& manager_)
{
    manager = &manager_;
}

void MenuPresentation::Reset()
{
    ReloadCurves();
    elapsed = selectionTime = confirmTime = repeatTime = 0.0f;
    selectedIndex = heldDirection = 0;
    confirmedIndex = -1;
    armed = delivered = false;
    selection = {};
    previousMouse = MouseInput::Instance().GetScreenPos();
    for (auto& visual_ : visuals)
        if (const auto effect_ = visual_.object->GetComponent<UIVisualEffectComponent>()) effect_->Play();
}

void MenuPresentation::Select(int index_)
{
    if (selectedIndex == index_ || confirmedIndex >= 0) return;
    selectedIndex = index_;
    selectionTime = 0.0f;
    PlayMenuSound(false);
}

void MenuPresentation::Confirm(int index_)
{
    if (ScreenTransitionManager::Instance().IsInputBlocked()) return;
    if (index_ < 0 || index_ > 1 || confirmedIndex >= 0 || !armed || elapsed < 0.2f) return;
    selectedIndex = confirmedIndex = index_;
    confirmTime = 0.0f;
    PlayMenuSound(true);
}

void MenuPresentation::Update(float deltaTime_)
{
    BS_PROFILE_SCOPE("MenuPresentation::Update");
    const float step_ = std::isfinite(deltaTime_) ? (std::max)(0.0f, deltaTime_) : 0.0f;
    elapsed += step_;
    selectionTime += step_;
    if (confirmedIndex >= 0) confirmTime += step_;
    for (int index_ = 0; index_ < 2; ++index_)
        selection[index_] += ((index_ == selectedIndex ? 1.0f : 0.0f) - selection[index_]) * (1.0f - std::exp(-18.0f * step_));
    for (auto& visual_ : visuals)
        if (const auto effect_ = visual_.object->GetComponent<UIVisualEffectComponent>()) effect_->UpdateUnscaled(step_);
    if (GetForegroundWindow() != GraphicsDeviceManager::Instance().GetWindowHandle()) return;
    const auto& keys_ = KeyConfig::Instance();
    const bool confirm_ = keys_.ActionPressed("UI.MenuConfirm");
    if (!confirm_ && !MouseInput::Instance().LeftDown()) armed = true;
    if (confirmedIndex >= 0 || !armed || elapsed < 0.2f) return;
    const bool up_ = keys_.ActionPressed("UI.MenuPrevious");
    const bool down_ = keys_.ActionPressed("UI.MenuNext");
    const int direction_ = up_ == down_ ? 0 : up_ ? -1 : 1;
    if (!direction_) { heldDirection = 0; repeatTime = 0.0f; }
    else if (direction_ != heldDirection)
    {
        Select(1 - selectedIndex); heldDirection = direction_; repeatTime = 0.35f;
    }
    else if ((repeatTime -= step_) <= 0.0f) { Select(1 - selectedIndex); repeatTime = 0.14f; }
    if (keys_.ActionPressedIn("UI.MenuConfirm") && !KeyInput::KeyPressed(VK_MENU)) Confirm(selectedIndex);
}

int MenuPresentation::TakeResult()
{
    if (GetForegroundWindow() != GraphicsDeviceManager::Instance().GetWindowHandle()) return -1;
    // 決定ボタンの保持をゲームへ持ち越し、再開直後に攻撃することを防ぐ。
    if (delivered || confirmedIndex < 0 || confirmTime < 0.28f ||
        KeyConfig::Instance().ActionPressed("UI.MenuConfirm") || MouseInput::Instance().LeftDown() ||
        KeyInput::KeyPressed(VK_ESCAPE) || ControllerInput::Instance().ButtonPressed(PadCode::BACK)) return -1;
    delivered = true;
    return confirmedIndex;
}

void MenuPresentation::DrawFeedback(const ImVec2& minimum_, const ImVec2& maximum_, float progress_, float alpha_, bool death_)
{
    if (progress_ < 0.0f || progress_ >= 1.0f) return;
    BS_PROFILE_SCOPE("MenuPresentation::DrawFeedback");
    const auto drawList_ = ImGui::GetForegroundDrawList();
    const float height_ = maximum_.y - minimum_.y;
    const float y_ = (minimum_.y + maximum_.y) * 0.5f;
    const float x_ = minimum_.x + (maximum_.x - minimum_.x) * Reveal(progress_);
    const auto tint_ = ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, death_ ? 0.32f : 0.85f,
        death_ ? 0.25f : 0.58f, std::clamp(alpha_ * (1.0f - progress_), 0.0f, 1.0f)));
    // MeshVFXの流れる発光と消散を、停止中にも進む画面上の帯と散光で表現する。
    drawList_->AddRectFilledMultiColor(ImVec2((std::max)(minimum_.x, x_ - height_), minimum_.y),
        ImVec2(x_, maximum_.y), IM_COL32(255, 240, 200, 0), tint_, tint_, IM_COL32(255, 240, 200, 0));
    for (int index_ = 0; index_ < 8; ++index_)
    {
        const float angle_ = static_cast<float>(index_) * 0.785398f;
        const float direction_ = index_ < 4 ? 1.0f : -1.0f;
        const float edge_ = direction_ > 0.0f ? maximum_.x : minimum_.x;
        const float distance_ = height_ * (0.15f + progress_ * 0.9f);
        const ImVec2 start_(edge_ + direction_ * distance_ * 0.6f, y_ + std::sin(angle_) * distance_);
        const ImVec2 end_(start_.x + direction_ * height_ * 0.12f * (1.0f - progress_),
            start_.y + std::sin(angle_) * height_ * 0.08f);
        drawList_->AddLine(start_, end_, tint_, (std::max)(1.0f, height_ * 0.025f));
    }
}

// ポーズ中も呼び出される描画側で配置を更新し、通常Updateの停止に影響されない。
void MenuPresentation::SubmitVisual(SpriteRenderSystem& sprites_, const char* name_, float x_, float y_, float width_, float height_,
    const DirectX::XMFLOAT4& color_, const char* text_, float fontSize_, const char* preset_, float strength_)
{
    BS_PROFILE_SCOPE("MenuPresentation::SubmitVisual");
    if (!manager) return;
    auto found_ = std::find_if(visuals.begin(), visuals.end(), [name_](const MenuVisual& visual_) { return visual_.name == name_; });
    if (found_ == visuals.end())
    {
        MenuVisual visual_;
        visual_.name = name_;
        visual_.object = std::make_shared<GameObject>(name_);
        visual_.object->SetManager(manager);
        visual_.object->SetNoSaveObject();
        visual_.object->transform->gameObject = visual_.object;
        const auto sprite_ = visual_.object->AddComponent<SpriteRenderer>();
        sprite_->Deserialize({{"SdrTexture",true},{"LinearSampling",true},{"CatSize",{1,1}}});
        const auto widget_ = visual_.object->AddComponent<UIWidgetComponent>();
        widget_->Deserialize({{"InputEnabled",false},{"RaycastTarget",false}});
        if (text_)
        {
            const auto textComponent_ = visual_.object->AddComponent<UITextComponent>();
            textComponent_->Deserialize({{"FontSize",fontSize_},{"Padding",6.0f},{"AutoSizeWidget",false},{"Color",{1,1,1,1}}});
        }
        if (preset_)
        {
            // 自動Startを通さず表示専用に初期化し、Actionをグローバル登録しない。
            const auto effect_ = visual_.object->AddComponent<UIVisualEffectComponent>(false);
            effect_->LoadPreset(preset_);
            effect_->SetStarted(true);
            effect_->Play();
        }
        visuals.push_back(std::move(visual_));
        found_ = std::prev(visuals.end());
    }
    const auto object_ = found_->object;
    if (text_ && found_->text != text_)
    {
        found_->text = text_;
        const auto textComponent_ = object_->GetComponent<UITextComponent>();
        textComponent_->SetText(text_);
        textComponent_->Update(0.0f);
    }
    const auto sprite_ = object_->GetComponent<SpriteRenderer>();
    sprite_->SetRuntimeColor(color_);
    sprite_->SetRuntimeVisible(color_.w > 0.001f);
    if (const auto effect_ = object_->GetComponent<UIVisualEffectComponent>()) effect_->SetRuntimeStrength(strength_);
    const auto widget_ = object_->GetComponent<UIWidgetComponent>();
    auto& rect_ = widget_->GetRectTransform();
    rect_.anchorMin = rect_.anchorMax = rect_.pivot = {0,0};
    rect_.anchoredPosition = {x_,y_}; rect_.sizeDelta = {width_,height_};
    if (text_)
    {
        // 文字画像を枠いっぱいに引き伸ばさず、字形の縦横比と解像度倍率を保って中央へ配置する。
        D3D11_VIEWPORT viewport_{}; UINT count_ = 1;
        GraphicsDeviceManager::Instance().GetContext()->RSGetViewports(&count_, &viewport_);
        const float scale_ = (std::min)(viewport_.Width/1280.0f,viewport_.Height/720.0f);
        const auto size_ = sprite_->GetTextureSize();
        const float fit_ = (std::min)({scale_,width_/(std::max)(1.0f,size_.x),height_/(std::max)(1.0f,size_.y)});
        rect_.sizeDelta = {size_.x*fit_,size_.y*fit_};
        rect_.anchoredPosition = {x_+(width_-rect_.sizeDelta.x)*0.5f,y_+(height_-rect_.sizeDelta.y)*0.5f};
    }
    widget_->MarkLayoutDirty();
    SpriteRenderSystem::SpriteRenderCommand command_;
    command_.sprite = sprite_; command_.widget = widget_.get();
    // Submit順を保ち、暗幕、装飾、文字の順で合成する。
    sprites_.Submit(command_);
}

void MenuPresentation::DrawBackdrop(SpriteRenderSystem& sprites_, float alpha_)
{
    BS_PROFILE_SCOPE("MenuPresentation::DrawBackdrop");
    D3D11_VIEWPORT viewport_{}; UINT count_ = 1;
    auto* context_ = GraphicsDeviceManager::Instance().GetContext();
    context_->RSGetViewports(&count_, &viewport_);
    SubmitVisual(sprites_, "MenuBackdrop", 0, 0, viewport_.Width, viewport_.Height, {0,0,0,std::clamp(alpha_,0.0f,1.0f)});
    sprites_.RenderAll(context_);
}

void MenuPresentation::Draw(SpriteRenderSystem& sprites_, const char* title_, const char* firstLabel_, bool death_, const char* secondLabel_)
{
    BS_PROFILE_SCOPE("MenuPresentation::Draw");
    D3D11_VIEWPORT viewport_{}; UINT count_ = 1;
    auto* context_ = GraphicsDeviceManager::Instance().GetContext();
    context_->RSGetViewports(&count_, &viewport_);
    if (viewport_.Width <= 0 || viewport_.Height <= 0) return;
    const float scale_ = (std::min)(viewport_.Width / 1280.0f, viewport_.Height / 720.0f);
    const float centerX_ = viewport_.Width * 0.5f;
    const float reveal_ = Reveal(elapsed / 0.32f);
    const float transition_ = confirmedIndex >= 0 ? std::clamp(confirmTime / 0.28f,0.0f,1.0f) : 0.0f;
    const float alpha_ = reveal_ * (1.0f - transition_);
    const float top_ = viewport_.Height * 0.24f + (1.0f - reveal_) * 28.0f * scale_;
    const DirectX::XMFLOAT4 accent_ = death_ ? DirectX::XMFLOAT4{1.0f,0.38f,0.3f,alpha_} : DirectX::XMFLOAT4{0.82f,1.0f,0.9f,alpha_};
    const auto draw_ = [&](const char* name_, float x_, float y_, float width_, float height_, DirectX::XMFLOAT4 color_,
        const char* text_ = nullptr, float fontSize_ = 26.0f, const char* preset_ = nullptr, float strength_ = 1.0f)
    {
        SubmitVisual(sprites_,name_,centerX_+x_*scale_,top_+y_*scale_,width_*scale_,height_*scale_,color_,text_,fontSize_,preset_,strength_);
    };
    if (!death_) DrawBackdrop(sprites_,0.74f*reveal_*(confirmedIndex == 0 ? 1.0f-transition_ : 1.0f));
    draw_("MenuPanel",-302,-26,604,418,{0.018f,0.028f,0.045f,alpha_*0.94f});
    draw_("MenuHeading",-290,0,580,100,accent_,title_,death_ ? 76.0f : 62.0f,"MenuHeading");
    draw_("MenuDivider",-246,108,492,2,{accent_.x,accent_.y,accent_.z,alpha_*0.55f},nullptr,26,"MenuAccent");
    const auto mouse_ = MouseInput::Instance().GetScreenPos();
    const bool moved_ = std::abs(mouse_.x-previousMouse.x)+std::abs(mouse_.y-previousMouse.y)>0.5f;
    previousMouse = mouse_;
    const bool focused_ = GetForegroundWindow() == GraphicsDeviceManager::Instance().GetWindowHandle();
    for (int index_ = 0; index_ < 2; ++index_)
    {
        const float focus_ = selection[index_];
        const float kick_ = index_ == selectedIndex ? Focus(selectionTime/0.24f)-1.0f : 0.0f;
        const float half_ = 228.0f+focus_*10.0f+kick_*8.0f;
        const float y_ = 142.0f+index_*78.0f;
        const float left_ = centerX_-half_*scale_, right_ = centerX_+half_*scale_;
        const float upper_ = top_+y_*scale_, lower_ = upper_+60.0f*scale_;
        if (focused_ && armed && confirmedIndex < 0 && elapsed >= 0.2f && (moved_ || MouseInput::Instance().LeftPressed()) &&
            mouse_.x >= left_ && mouse_.x <= right_ && mouse_.y >= upper_ && mouse_.y <= lower_)
        {
            Select(index_);
            if (MouseInput::Instance().LeftPressed()) Confirm(index_);
        }
        const std::string prefix_ = "MenuButton"+std::to_string(index_);
        draw_((prefix_+"Frame").c_str(),-half_-1,y_-1,half_*2+2,62,
            {accent_.x,accent_.y,accent_.z,alpha_*(0.2f+0.55f*focus_)},nullptr,26,"MenuAccent",0.35f+0.65f*focus_);
        draw_((prefix_+"Fill").c_str(),-half_,y_,half_*2,60,{0.035f+focus_*0.025f,0.05f+focus_*0.04f,0.07f+focus_*0.04f,alpha_});
        draw_((prefix_+"Marker").c_str(),-half_+17,y_+18,3,24,{accent_.x,accent_.y,accent_.z,alpha_*focus_},nullptr,26,"MenuAccent");
        // ラベルの字形を保ち、選択時だけ光沢の強度を上げる。
        draw_((prefix_+"Label").c_str(),-206,y_+5,412,50,{1,0.97f,0.9f,alpha_*(0.82f+0.18f*focus_)},
            index_ == 0 ? firstLabel_ : secondLabel_,28,"MenuButton",0.2f+focus_*0.8f);
        const float sweep_ = std::fmod(elapsed*0.28f+index_*0.3f,1.0f);
        draw_((prefix_+"Shine").c_str(),-half_+(half_*2-48)*sweep_,y_+58,48,2,
            {accent_.x,accent_.y,accent_.z,alpha_*focus_*0.65f},nullptr,26,"MenuAccent");
        if (confirmedIndex == index_)
            draw_((prefix_+"Confirm").c_str(),-half_,y_+57,half_*2*Reveal(transition_),3,
                {1,1,0.9f,reveal_*(1.0f-transition_)},nullptr,26,"MenuAccent");
    }
    draw_("MenuGuide",-285,316,570,32,{0.75f,0.85f,0.83f,alpha_*0.95f},
        ControllerInput::Instance().IsPlayStation() ? "方向キー / 左スティック：選択    ×：決定" : "↑ ↓ / 左スティック：選択    Enter / A：決定",18,"HudReadable");
    if (!death_) draw_("MenuCancelGuide",-240,350,480,28,{0.65f,0.78f,0.76f,alpha_},"ESC / Select：再開",17,"HudReadable");
    sprites_.RenderAll(context_);
    if (confirmedIndex == 1) DrawBackdrop(sprites_,transition_);
}

#include "PrecompiledHeader.h"
#include "Engine/Debug/TracyProfiler.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "UIMotionComponent.h"
#include "UIPresentationPresets.h"
#include "TransitionSequence.h"
#include "UIWidgetComponent.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include <imgui.h>
#include <imgui_stdlib.h>
#include <algorithm>
#include <cmath>
namespace { constexpr const char* CATALOG_PATH = "Data/UI/UIMotionPresets.json"; }
const std::string& UIMotionComponent::GetName() const { static const std::string NAME = "UIMotionComponent"; return NAME; }
void UIMotionComponent::Start()
{
    BS_PROFILE_SCOPE("UIMotionComponent::Start");
    catalog = UIPresentationPresets::Load(CATALOG_PATH);
    if (settings.empty()) LoadPreset(preset);
    const auto owner_ = GetGameObject();
    if (!owner_) return;
    widget = owner_->GetComponent<UIWidgetComponent>();
    const std::weak_ptr<UIMotionComponent> weak_ = owner_->GetComponent<UIMotionComponent>();
    const auto register_ = [&](const char* name_,auto function_)
    {
        owner_->RegisterGlobalAction("UIMotion",name_,[weak_,function_]() { if (const auto component_ = weak_.lock(); component_ && component_->IsEnabled()) function_(*component_); });
    };
    register_("MotionPlay",[](UIMotionComponent& component_) { component_.Play(); });
    register_("MotionStop",[](UIMotionComponent& component_) { component_.Stop(); });
    register_("MotionPause",[](UIMotionComponent& component_) { component_.Pause(); });
    register_("MotionResume",[](UIMotionComponent& component_) { component_.Resume(); });
    if (playOnStart && GetManager().GetEditorPlayMode() == EditorPlayMode::PLAY) Play();
}
void UIMotionComponent::OnDestroy() { Stop(); }
bool UIMotionComponent::LoadPreset(const std::string& name_)
{
    if (catalog.empty()) catalog = UIPresentationPresets::Load(CATALOG_PATH);
    const auto found_ = catalog.find(name_);
    if (found_ == catalog.end() || !found_->is_object()) { status = "Preset unavailable; existing settings retained"; return false; }
    preset = name_; settings = *found_; status = "Preset loaded"; return true;
}
void UIMotionComponent::Play(bool preview_)
{
    elapsedTime = 0; playing = true; paused = false; preview = preview_; Evaluate();
}
void UIMotionComponent::Stop(bool restore_)
{
    playing = false; paused = false; preview = false;
    if (restore_) { sample = {}; elapsedTime = 0; ApplyWidget(); }
}
void UIMotionComponent::UpdateUnscaled(float deltaTime_)
{
    BS_PROFILE_SCOPE("UIMotionComponent::UpdateUnscaled");
    if (!playing || paused || (!preview && GetManager().GetEditorPlayMode() == EditorPlayMode::EDITOR)) return;
    const float step_ = std::isfinite(deltaTime_) ? (std::max)(0.0f,deltaTime_) : 0;
    elapsedTime += step_ * UIPresentationPresets::Number(settings,"Speed",1,0.05f,10);
    Evaluate();
}
// 補間と周期運動を加算し、拡縮・位置・回転・Alphaを独立して組み合わせる。
void UIMotionComponent::Evaluate()
{
    const float duration_ = UIPresentationPresets::Number(settings,"Duration",0.6f,0,30);
    const float delay_ = UIPresentationPresets::Number(settings,"Delay",0,0,30);
    const bool loop_ = UIPresentationPresets::Boolean(settings,"Loop",false);
    const bool pingPong_ = UIPresentationPresets::Boolean(settings,"PingPong",false);
    const float time_ = (std::max)(0.0f,elapsedTime - delay_);
    float phase_ = elapsedTime < delay_ ? 0 : duration_ > 0 ? time_ / duration_ : 1;
    if (duration_ <= 0 && elapsedTime >= delay_) { phase_ = pingPong_ ? 2.0f : 1.0f; playing = false; }
    else if (loop_) phase_ = std::fmod(phase_,pingPong_ ? 2.0f : 1.0f);
    else
    {
        if (phase_ >= (pingPong_ ? 2.0f : 1.0f)) playing = false;
        phase_ = (std::min)(phase_,pingPong_ ? 2.0f : 1.0f);
    }
    const float ratio_ = pingPong_ && phase_ > 1 ? 2 - phase_ : phase_;
    const auto easing_ = static_cast<TransitionEasing>(static_cast<int>(UIPresentationPresets::Number(settings,"Easing",3,0,5)));
    const float weight_ = TransitionSequence::Evaluate(ratio_,easing_);
    const auto from_ = UIPresentationPresets::Vector2(settings,"OffsetFrom",{0,0},-4096,4096);
    const auto to_ = UIPresentationPresets::Vector2(settings,"OffsetTo",{0,0},-4096,4096);
    const auto scaleFrom_ = UIPresentationPresets::Vector2(settings,"ScaleFrom",{1,1},0.001f,10);
    const auto scaleTo_ = UIPresentationPresets::Vector2(settings,"ScaleTo",{1,1},0.001f,10);
    const auto offsetAmplitude_ = UIPresentationPresets::Vector2(settings,"OffsetAmplitude",{0,0},-1024,1024);
    const auto scaleAmplitude_ = UIPresentationPresets::Vector2(settings,"ScaleAmplitude",{0,0},-2,2);
    const float cycles_ = UIPresentationPresets::Number(settings,"Cycles",1,0,30);
    const float wave_ = elapsedTime < delay_ ? 0 : std::sin((duration_ > 0 ? time_ / duration_ : 0) * cycles_ * DirectX::XM_2PI);
    sample.offset = {std::lerp(from_.x,to_.x,weight_) + offsetAmplitude_.x * wave_,std::lerp(from_.y,to_.y,weight_) + offsetAmplitude_.y * wave_};
    sample.scale = {(std::max)(0.001f,std::lerp(scaleFrom_.x,scaleTo_.x,weight_) + scaleAmplitude_.x * wave_),
        (std::max)(0.001f,std::lerp(scaleFrom_.y,scaleTo_.y,weight_) + scaleAmplitude_.y * wave_)};
    sample.rotation = std::lerp(UIPresentationPresets::Number(settings,"RotationFrom",0,-3600,3600),UIPresentationPresets::Number(settings,"RotationTo",0,-3600,3600),weight_) + UIPresentationPresets::Number(settings,"RotationAmplitude",0,-360,360) * wave_;
    sample.opacity = std::clamp(std::lerp(UIPresentationPresets::Number(settings,"AlphaFrom",1,0,1),UIPresentationPresets::Number(settings,"AlphaTo",1,0,1),weight_) + UIPresentationPresets::Number(settings,"AlphaAmplitude",0,-1,1) * wave_,0.0f,1.0f);
    ApplyWidget();
}
void UIMotionComponent::ApplyWidget()
{
    if (widget.expired()) if (const auto owner_ = GetGameObject()) widget = owner_->GetComponent<UIWidgetComponent>();
    if (const auto widget_ = widget.lock()) widget_->MarkLayoutDirty();
}
void UIMotionComponent::Gui()
{
    BS_PROFILE_SCOPE("UIMotionComponent::Gui");
    if (catalog.empty()) catalog = UIPresentationPresets::Load(CATALOG_PATH);
    if (UIPresentationPresets::DrawPicker(catalog,preset)) { LoadPreset(preset); Play(true); }
    ImGui::Checkbox("Play On Start",&playOnStart);
    const auto number_ = [&](const char* key_,float fallback_,float minimum_,float maximum_)
    {
        float value_ = UIPresentationPresets::Number(settings,key_,fallback_,minimum_,maximum_);
        if (ImGui::DragFloat(key_,&value_,0.01f,minimum_,maximum_)) { settings[key_] = value_; Evaluate(); }
    };
    number_("Duration",0.6f,0,30); number_("Delay",0,0,30); number_("Speed",1,0.05f,10); number_("Cycles",1,0,30);
    constexpr const char* EASINGS[] = {"LINEAR","EASE_IN_CUBIC","EASE_OUT_CUBIC","EASE_IN_OUT_CUBIC","EASE_OUT_EXPO","EASE_OUT_BACK"};
    int easing_ = static_cast<int>(UIPresentationPresets::Number(settings,"Easing",3,0,5));
    if (ImGui::Combo("Easing",&easing_,EASINGS,6)) settings["Easing"] = easing_;
    for (const auto key_ : {"Loop","PingPong"}) { bool value_ = UIPresentationPresets::Boolean(settings,key_,false); if (ImGui::Checkbox(key_,&value_)) settings[key_] = value_; }
    for (const auto key_ : {"OffsetFrom","OffsetTo","OffsetAmplitude","ScaleFrom","ScaleTo","ScaleAmplitude"})
    {
        const bool scale_ = std::string(key_).find("Scale") == 0;
        const bool amplitude_ = std::string(key_).find("Amplitude") != std::string::npos;
        auto value_ = UIPresentationPresets::Vector2(settings,key_,scale_ && !amplitude_ ? DirectX::XMFLOAT2{1,1} : DirectX::XMFLOAT2{0,0},-4096,4096);
        if (ImGui::DragFloat2(key_,&value_.x,0.05f)) { settings[key_] = {value_.x,value_.y}; Evaluate(); }
    }
    number_("RotationFrom",0,-3600,3600); number_("RotationTo",0,-3600,3600); number_("RotationAmplitude",0,-360,360);
    number_("AlphaFrom",1,0,1); number_("AlphaTo",1,0,1); number_("AlphaAmplitude",0,-1,1);
    if (ImGui::Button("Preview / Restart")) Play(true);
    ImGui::SameLine(); if (ImGui::Button("Pause / Resume")) paused = !paused;
    ImGui::SameLine(); if (ImGui::Button("Stop / Restore")) Stop();
    ImGui::InputText("Save Name",&saveName);
    if (ImGui::Button("Save Motion Preset")) { if (!saveName.empty()) { catalog[saveName] = settings; status = UIPresentationPresets::Save(CATALOG_PATH,catalog) ? "Saved" : "Save failed"; } }
    ImGui::SameLine(); if (ImGui::Button("Reload Motion Presets")) catalog = UIPresentationPresets::Load(CATALOG_PATH);
    ImGui::TextWrapped("%s",status.c_str());
}
void UIMotionComponent::Serialize(nlohmann::json& json_) const { json_ = {{"Preset",preset},{"PlayOnStart",playOnStart},{"Settings",settings}}; }
void UIMotionComponent::Deserialize(const nlohmann::json& json_)
{
    preset = UIPresentationPresets::String(json_,"Preset","PopIn"); playOnStart = UIPresentationPresets::Boolean(json_,"PlayOnStart",true);
    settings = json_.contains("Settings") && json_["Settings"].is_object() ? json_["Settings"] : nlohmann::json::object(); Stop();
}

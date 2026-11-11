#include "PrecompiledHeader.h"
#include "Engine/Debug/TracyProfiler.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "UIVisualEffectComponent.h"
#include "UIPresentationPresets.h"
#include "TransitionSequence.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include <imgui.h>
#include <imgui_stdlib.h>
#include <algorithm>
#include <cmath>
namespace { constexpr const char* CATALOG_PATH = "Data/UI/UIVisualEffectPresets.json"; }
const std::string& UIVisualEffectComponent::GetName() const { static const std::string NAME = "UIVisualEffectComponent"; return NAME; }
void UIVisualEffectComponent::Start()
{
    BS_PROFILE_SCOPE("UIVisualEffectComponent::Start");
    catalog = UIPresentationPresets::Load(CATALOG_PATH);
    if (settings.empty()) LoadPreset(preset); else Normalize();
    const auto owner_ = GetGameObject();
    if (!owner_) return;
    const std::weak_ptr<UIVisualEffectComponent> weak_ = owner_->GetComponent<UIVisualEffectComponent>();
    const auto register_ = [&](const char* name_,auto function_)
    {
        owner_->RegisterGlobalAction("UIEffect",name_,[weak_,function_]() { if (const auto component_ = weak_.lock(); component_ && component_->IsEnabled()) function_(*component_); });
    };
    register_("EffectPlay",[](UIVisualEffectComponent& component_) { component_.Play(); });
    register_("EffectStop",[](UIVisualEffectComponent& component_) { component_.Stop(); });
    register_("EffectPause",[](UIVisualEffectComponent& component_) { component_.paused = true; });
    register_("EffectResume",[](UIVisualEffectComponent& component_) { component_.paused = false; });
    if (!maskTexturePath.empty() && !maskTexture) UIPresentationPresets::LoadTexture(maskTexturePath,maskTexture);
    restored = false;
    if (playOnStart && GetManager().GetEditorPlayMode() == EditorPlayMode::PLAY) Play();
}
void UIVisualEffectComponent::OnDestroy() { ClearTrails(); maskTexture.Reset(); }
bool UIVisualEffectComponent::LoadPreset(const std::string& name_)
{
    if (catalog.empty()) catalog = UIPresentationPresets::Load(CATALOG_PATH);
    const auto found_ = catalog.find(name_);
    if (found_ == catalog.end() || !found_->is_object()) { status = "Preset unavailable; existing settings retained"; return false; }
    preset = name_; settings = *found_; Normalize(); ClearTrails(); status = "Preset loaded"; return true;
}
void UIVisualEffectComponent::Play(bool preview_)
{
    restored = false; preview = preview_; playing = true; paused = false; playTime = progress = 0; ClearTrails();
}
void UIVisualEffectComponent::Stop() { restored = true; playing = false; paused = false; preview = false; playTime = progress = 0; ClearTrails(); }
void UIVisualEffectComponent::SetProgress(float progress_) { restored = false; progress = std::isfinite(progress_) ? std::clamp(progress_,0.0f,1.0f) : 0; playing = false; }
void UIVisualEffectComponent::SetRuntimeStrength(float strength_)
{
    runtimeStrength = std::isfinite(strength_) ? std::clamp(strength_,0.0f,1.0f) : 0.0f;
}
void UIVisualEffectComponent::UpdateUnscaled(float deltaTime_)
{
    BS_PROFILE_SCOPE("UIVisualEffectComponent::UpdateUnscaled");
    ++frameSerial;
    if (!preview && GetManager().GetEditorPlayMode() == EditorPlayMode::EDITOR) return;
    const float step_ = std::isfinite(deltaTime_) ? (std::max)(0.0f,deltaTime_) : 0;
    trailTime += step_;
    while (!trails.empty() && trailTime - trails.front().time >= parameters.trailLifetime) trails.pop_front();
    if (!playing || paused) return;
    playTime += step_ * UIPresentationPresets::Number(settings,"Speed",1,0.05f,10);
    const float duration_ = UIPresentationPresets::Number(settings,"Duration",1,0.01f,30);
    const float delay_ = UIPresentationPresets::Number(settings,"Delay",0,0,30);
    const bool loop_ = UIPresentationPresets::Boolean(settings,"Loop",true), pingPong_ = UIPresentationPresets::Boolean(settings,"PingPong",false);
    float phase_ = (std::max)(0.0f,playTime - delay_) / duration_;
    if (loop_) phase_ = std::fmod(phase_,pingPong_ ? 2.0f : 1.0f);
    else { if (phase_ >= (pingPong_ ? 2.0f : 1.0f)) playing = false; phase_ = (std::min)(phase_,pingPong_ ? 2.0f : 1.0f); }
    if (pingPong_ && phase_ > 1) phase_ = 2 - phase_;
    progress = std::clamp(TransitionSequence::Evaluate(phase_,static_cast<TransitionEasing>(static_cast<int>(UIPresentationPresets::Number(settings,"Easing",3,0,5)))),0.0f,1.0f);
}
void UIVisualEffectComponent::ClearTrails() { trails.clear(); capturedSerial = UINT64_MAX; }
void UIVisualEffectComponent::PrepareTrailView(const DirectX::XMFLOAT2& screenSize_)
{
    if (trailScreenSize.x != screenSize_.x || trailScreenSize.y != screenSize_.y) ClearTrails();
    trailScreenSize = screenSize_;
}
void UIVisualEffectComponent::CaptureTrail(UIEffectTrailSample sample_,const DirectX::XMFLOAT2& screenSize_)
{
    if (!playing || paused || parameters.trailCount <= 0 || capturedSerial == frameSerial) return;
    if (trailScreenSize.x != screenSize_.x || trailScreenSize.y != screenSize_.y) ClearTrails();
    trailScreenSize = screenSize_; capturedSerial = frameSerial;
    if (!trails.empty() && trailTime - trails.back().time < parameters.trailInterval) return;
    sample_.time = trailTime; trails.push_back(std::move(sample_));
    while (trails.size() > static_cast<std::size_t>(parameters.trailCount)) trails.pop_front();
}
// 必須でない項目は無効果を既定値にし、旧SpriteへComponentを付けた場合も設定を選んで追加できる。
void UIVisualEffectComponent::Normalize()
{
    parameters = {};
    const auto path_ = UIPresentationPresets::String(settings,"MaskTexture","");
    if (maskTexturePath != path_) { maskTexturePath = path_; maskTexture.Reset(); if (IsStarted()) UIPresentationPresets::LoadTexture(path_,maskTexture); }
    parameters.maskScale = UIPresentationPresets::Vector2(settings,"MaskScale",{1,1},0.01f,100);
    parameters.maskOffset = UIPresentationPresets::Vector2(settings,"MaskOffset",{0,0},-100,100);
    parameters.maskInvert = UIPresentationPresets::Boolean(settings,"MaskInvert",false);
    const auto number_ = [&](const char* key_,float& value_,float minimum_,float maximum_) { value_ = UIPresentationPresets::Number(settings,key_,value_,minimum_,maximum_); };
    parameters.glowColor = UIPresentationPresets::Color(settings,"GlowColor",parameters.glowColor);
    parameters.outlineColor = UIPresentationPresets::Color(settings,"OutlineColor",parameters.outlineColor);
    parameters.shadowColor = UIPresentationPresets::Color(settings,"ShadowColor",parameters.shadowColor);
    parameters.sweepColor = UIPresentationPresets::Color(settings,"SweepColor",parameters.sweepColor);
    parameters.gradientTop = UIPresentationPresets::Color(settings,"GradientTop",parameters.gradientTop);
    parameters.gradientBottom = UIPresentationPresets::Color(settings,"GradientBottom",parameters.gradientBottom);
    parameters.burnColor = UIPresentationPresets::Color(settings,"BurnColor",parameters.burnColor);
    parameters.trailColor = UIPresentationPresets::Color(settings,"TrailColor",parameters.trailColor);
    parameters.shadowOffset = UIPresentationPresets::Vector2(settings,"ShadowOffset",parameters.shadowOffset,-64,64);
    parameters.uvScroll = UIPresentationPresets::Vector2(settings,"UVScroll",parameters.uvScroll,-5,5);
    number_("GlowIntensity",parameters.glowIntensity,0,5); number_("GlowRadius",parameters.glowRadius,0,32);
    number_("OutlineWidth",parameters.outlineWidth,0,12); number_("ShadowOpacity",parameters.shadowOpacity,0,1); number_("ShadowSoftness",parameters.shadowSoftness,0,16);
    number_("SweepIntensity",parameters.sweepIntensity,0,5); number_("SweepWidth",parameters.sweepWidth,0.01f,1); number_("SweepAngle",parameters.sweepAngle,-360,360); number_("SweepSpeed",parameters.sweepSpeed,-5,5);
    number_("PulseAmount",parameters.pulseAmount,0,3); number_("PulseSpeed",parameters.pulseSpeed,0,10);
    number_("Grayscale",parameters.grayscale,0,1); number_("Sepia",parameters.sepia,0,1); number_("Invert",parameters.invert,0,1); number_("PixelSize",parameters.pixelSize,1,64);
    number_("BlurRadius",parameters.blurRadius,0,12); number_("GradientStrength",parameters.gradientStrength,0,1); number_("Dissolve",parameters.dissolve,0,1); number_("EdgeWidth",parameters.edgeWidth,0.001f,0.25f); number_("NoiseScale",parameters.noiseScale,1,80);
    parameters.trailCount = static_cast<int>(UIPresentationPresets::Number(settings,"TrailCount",0,0,6));
    number_("TrailLifetime",parameters.trailLifetime,0.03f,1); number_("TrailInterval",parameters.trailInterval,0.01f,0.5f); number_("TrailScale",parameters.trailScale,0.5f,2);
    tweenStrength = UIPresentationPresets::Boolean(settings,"TweenStrength",false);
}
void UIVisualEffectComponent::Gui()
{
    BS_PROFILE_SCOPE("UIVisualEffectComponent::Gui");
    if (catalog.empty()) catalog = UIPresentationPresets::Load(CATALOG_PATH);
    if (UIPresentationPresets::DrawPicker(catalog,preset)) { LoadPreset(preset); Play(true); }
    ImGui::Checkbox("Play On Start",&playOnStart);
    if (ImGui::TreeNode("Mask Texture (MeshVFX Asset Slot)"))
    {
        if (UIPresentationPresets::DrawTexture(settings,"MaskTexture","Dissolve / Burn Mask")) Normalize();
        auto scale_ = parameters.maskScale; auto offset_ = parameters.maskOffset; bool invert_ = parameters.maskInvert;
        if (ImGui::DragFloat2("Mask Scale",&scale_.x,.01f,.01f,100)) { settings["MaskScale"] = {scale_.x,scale_.y}; Normalize(); }
        if (ImGui::DragFloat2("Mask Offset",&offset_.x,.01f,-100,100)) { settings["MaskOffset"] = {offset_.x,offset_.y}; Normalize(); }
        if (ImGui::Checkbox("Invert Mask",&invert_)) { settings["MaskInvert"] = invert_; Normalize(); }
        ImGui::TextWrapped("Dissolve uses the mask red channel; an empty slot uses procedural noise.");
        ImGui::TreePop();
    }
    const auto number_ = [&](const char* key_,float fallback_,float minimum_,float maximum_)
    {
        float value_ = UIPresentationPresets::Number(settings,key_,fallback_,minimum_,maximum_);
        if (ImGui::DragFloat(key_,&value_,0.01f,minimum_,maximum_)) { settings[key_] = value_; Normalize(); }
    };
    const auto color_ = [&](const char* key_,DirectX::XMFLOAT4 fallback_)
    {
        auto value_ = UIPresentationPresets::Color(settings,key_,fallback_);
        if (ImGui::ColorEdit4(key_,&value_.x)) { settings[key_] = {value_.x,value_.y,value_.z,value_.w}; Normalize(); }
    };
    if (ImGui::TreeNode("Tone / Sampling"))
    {
        number_("Grayscale",0,0,1); number_("Sepia",0,0,1); number_("Invert",0,0,1); number_("PixelSize",1,1,64); number_("BlurRadius",0,0,12); ImGui::TreePop();
    }
    if (ImGui::TreeNode("Glow / Outline / Shadow"))
    {
        number_("GlowIntensity",0,0,5); number_("GlowRadius",8,0,32); color_("GlowColor",parameters.glowColor);
        number_("OutlineWidth",0,0,12); color_("OutlineColor",parameters.outlineColor);
        number_("ShadowOpacity",0,0,1); number_("ShadowSoftness",2,0,16); color_("ShadowColor",parameters.shadowColor);
        auto offset_ = parameters.shadowOffset; if (ImGui::DragFloat2("Shadow Offset",&offset_.x,0.1f,-64,64)) { settings["ShadowOffset"] = {offset_.x,offset_.y}; Normalize(); }
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Shine / Pulse / Gradient / Dissolve"))
    {
        number_("SweepIntensity",0,0,5); number_("SweepWidth",0.15f,0.01f,1); number_("SweepAngle",-30,-360,360); number_("SweepSpeed",0.6f,-5,5); color_("SweepColor",parameters.sweepColor);
        number_("PulseAmount",0,0,3); number_("PulseSpeed",1,0,10); number_("GradientStrength",0,0,1); color_("GradientTop",parameters.gradientTop); color_("GradientBottom",parameters.gradientBottom);
        number_("Dissolve",0,0,1); number_("EdgeWidth",0.06f,0.001f,0.25f); number_("NoiseScale",12,1,80); color_("BurnColor",parameters.burnColor);
        auto scroll_ = parameters.uvScroll; if (ImGui::DragFloat2("UV Scroll",&scroll_.x,0.01f,-5,5)) { settings["UVScroll"] = {scroll_.x,scroll_.y}; Normalize(); }
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Afterimage"))
    {
        number_("TrailCount",0,0,6); number_("TrailLifetime",0.25f,0.03f,1); number_("TrailInterval",0.035f,0.01f,0.5f); number_("TrailScale",1,0.5f,2); color_("TrailColor",parameters.trailColor); ImGui::TreePop();
    }
    for (const auto key_ : {"TweenStrength","Loop","PingPong"}) { bool value_ = UIPresentationPresets::Boolean(settings,key_,std::string(key_) == "Loop"); if (ImGui::Checkbox(key_,&value_)) { settings[key_] = value_; Normalize(); } }
    number_("Duration",1,0.01f,30); number_("Delay",0,0,30); number_("Speed",1,0.05f,10);
    number_("Easing",3,0,5);
    float progress_ = progress; if (ImGui::SliderFloat("Effect Progress",&progress_,0,1)) SetProgress(progress_);
    if (ImGui::Button("Preview / Restart")) Play(true);
    ImGui::SameLine(); if (ImGui::Button("Pause / Resume")) paused = !paused;
    ImGui::SameLine(); if (ImGui::Button("Stop / Restore")) Stop();
    ImGui::InputText("Save Name",&saveName);
    if (ImGui::Button("Save Effect Preset")) { if (!saveName.empty()) { catalog[saveName] = settings; status = UIPresentationPresets::Save(CATALOG_PATH,catalog) ? "Saved" : "Save failed"; } }
    ImGui::SameLine(); if (ImGui::Button("Reload Effect Presets")) catalog = UIPresentationPresets::Load(CATALOG_PATH);
    ImGui::TextWrapped("%s",status.c_str());
}
void UIVisualEffectComponent::Serialize(nlohmann::json& json_) const { json_ = {{"Preset",preset},{"PlayOnStart",playOnStart},{"Settings",settings}}; }
void UIVisualEffectComponent::Deserialize(const nlohmann::json& json_)
{
    preset = UIPresentationPresets::String(json_,"Preset","SoftGlow"); playOnStart = UIPresentationPresets::Boolean(json_,"PlayOnStart",true);
    settings = json_.contains("Settings") && json_["Settings"].is_object() ? json_["Settings"] : nlohmann::json::object(); Normalize(); Stop();
}

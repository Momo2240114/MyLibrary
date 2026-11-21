#include "PrecompiledHeader.h"
#include "ScreenTransitionManager.h"
#include "ShaderTransitionEffect.h"
#include "UIPresentationPresets.h"
#include <cmath>
#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Debug/TracyProfiler.h"
#include <imgui.h>
#include <algorithm>
#include <fstream>
#include <utility>
#include <cstring>

namespace
{
    constexpr const char* SETTINGS_PATH = "Data/UI/SceneTransitions.json";
}

ScreenTransitionManager& ScreenTransitionManager::Instance()
{
    static ScreenTransitionManager manager_;
    return manager_;
}

void ScreenTransitionManager::Initialize()
{
    Shutdown();
    LoadSettings();
}

void ScreenTransitionManager::Shutdown()
{
    BS_PROFILE_SCOPE("ScreenTransitionManager::Shutdown");
    previewCycle = false;
    previewWait = 0;
    Cancel(handle);
    effect.reset();
    ShaderTransitionEffect::ShutdownSharedResources();
}

bool ScreenTransitionManager::IsOwnerAlive() const
{
    if (!trackOwner)
    {
        return true;
    }
    const auto component_ = owner.lock();
    const auto object_ = component_ ? component_->GetGameObject() : nullptr;
    return component_ && component_->IsEnabled() && object_ && !object_->IsPendingDestroy();
}

TransitionHandle ScreenTransitionManager::Request(const TransitionParameters& parameters_,TransitionPlayback mode_,
    std::function<void()> onCovered_,std::function<void()> onCompleted_,std::weak_ptr<Component> owner_,bool trackOwner_,bool persistent_,int priority_,bool preview_)
{
    BS_PROFILE_SCOPE("ScreenTransitionManager::Request");
    if (!preview_)
    {
        previewCycle = false;
    }
    if (IsActive() && priority_ <= priority && !(preview && preview_))
    {
        return 0;
    }
    Cancel(handle);
    parameters = TransitionParameters::Deserialize(TransitionParameters::Serialize(parameters_));
    effect = TransitionEffectFactory::Create(TransitionParameters::GetTypeName(parameters.type));
    if (effect)
    {
        effect->SetParameters(parameters);
        effect->Initialize();
    }
    owner = owner_;
    trackOwner = trackOwner_;
    persistent = persistent_;
    priority = priority_;
    preview = preview_;
    onCovered = std::move(onCovered_);
    onCompleted = std::move(onCompleted_);
    handle = nextHandle++;
    sequence.Start(parameters,mode_);
    return handle;
}

// コールバックは描画から分離し、更新ループ開始時に一度だけ取り出す。
void ScreenTransitionManager::Update(float rawDeltaTime_)
{
    BS_PROFILE_SCOPE("ScreenTransitionManager::Update");
    previewWait = IsActive() ? 0 : previewWait + (std::isfinite(rawDeltaTime_) ? (std::max)(0.0f,rawDeltaTime_) : 0);
    if (!IsActive())
    {
        return;
    }
    if (!persistent && !IsOwnerAlive())
    {
        Cancel(handle);
        return;
    }
    sequence.Update(rawDeltaTime_);
    const auto dispatch_ = [&](std::function<void()>& source_)
    {
        auto callback_ = std::move(source_);
        if (!callback_ || !IsOwnerAlive())
        {
            return;
        }
        try
        {
            callback_();
        }
        catch (const std::exception& exception_)
        {
            status = exception_.what();
            sequence.Reveal();
        }
        catch (...)
        {
            status = "Transition callback failed";
            sequence.Reveal();
        }
    };
    const auto current_ = handle;
    if (sequence.TakeCovered())
    {
        dispatch_(onCovered);
    }
    if (handle != current_)
    {
        return;
    }
    if (sequence.TakeCompleted())
    {
        dispatch_(onCompleted);
        if (handle == current_)
        {
            owner.reset();
            onCovered = {};
            onCompleted = {};
            preview = false;
            handle = 0;
            effect.reset();
        }
    }
}

void ScreenTransitionManager::Render(ID3D11RenderTargetView* renderTarget_)
{
    BS_PROFILE_SCOPE("ScreenTransitionManager::Render");
    if (!IsActive())
    {
        return;
    }
    bool rendered_ = false;
    if (effect && effect->IsReady())
    {
        effect->SetProgress(sequence.GetProgress());
        effect->Update(sequence.GetEffectTime(),sequence.GetState() == TransitionState::TRANSITION_IN);
        rendered_ = effect->Render(renderTarget_);
    }
    else
    {
        rendered_ = fallbackQueued;
    }
    if (rendered_ && GraphicsDeviceManager::Instance().GetRenderTargetView())
    {
        sequence.NotifyRendered();
    }
    fallbackQueued = false;
}

void ScreenTransitionManager::Hold(TransitionHandle handle_)
{
    if (Owns(handle_))
    {
        sequence.Hold();
    }
}

void ScreenTransitionManager::Reveal(TransitionHandle handle_)
{
    if (Owns(handle_))
    {
        sequence.Reveal();
    }
}

void ScreenTransitionManager::CompleteWork(TransitionHandle handle_)
{
    if (Owns(handle_))
    {
        sequence.CompleteWork();
    }
}

void ScreenTransitionManager::Cancel(TransitionHandle handle_,TransitionCancelMode mode_)
{
    if (!Owns(handle_))
    {
        return;
    }
    sequence.Cancel(mode_);
    onCovered = {};
    onCompleted = {};
    owner.reset();
    trackOwner = false;
    preview = false;
    if (mode_ == TransitionCancelMode::CLEAR)
    {
        handle = 0;
        effect.reset();
    }
}

TransitionHandle ScreenTransitionManager::RetainCovered()
{
    if (!sequence.IsCovered() || preview)
    {
        return 0;
    }
    persistent = true;
    trackOwner = false;
    owner.reset();
    onCovered = {};
    onCompleted = {};
    priority = 100;
    handle = nextHandle++;
    sequence.Hold();
    return handle;
}

bool ScreenTransitionManager::LoadSettings()
{
    BS_PROFILE_SCOPE("ScreenTransitionManager::LoadSettings");
    std::ifstream file_(SETTINGS_PATH);
    if (!file_)
    {
        status = "Settings missing: using FADE";
        return false;
    }
    auto document_ = nlohmann::json::parse(file_,nullptr,false);
    if (!document_.is_object())
    {
        status = "Invalid settings: previous settings retained";
        return false;
    }
    settings = std::move(document_);
    status = "Settings loaded";
    return true;
}

TransitionParameters ScreenTransitionManager::GetPreset(const std::string& name_) const
{
    if (settings.contains("Presets") && settings["Presets"].is_object())
    {
        return TransitionParameters::Deserialize(settings["Presets"].value(name_,nlohmann::json::object()));
    }
    return {};
}

TransitionParameters ScreenTransitionManager::GetProfile(const std::string& profile_,const std::string& context_) const
{
    nlohmann::json selected_ = nlohmann::json::object();
    if (settings.contains("Profiles") && settings["Profiles"].is_object())
    {
        selected_ = settings["Profiles"].value(profile_,selected_);
    }
    if (!selected_.is_object())
    {
        selected_ = nlohmann::json::object();
    }
    if (settings.contains("Scenes") && settings["Scenes"].is_object() && settings["Scenes"].contains(context_) && settings["Scenes"][context_].is_object())
    {
        const auto& contextual_ = settings["Scenes"][context_];
        selected_.update(contextual_);
        if (contextual_.contains(profile_) && contextual_[profile_].is_object())
        {
            selected_.update(contextual_[profile_]);
        }
    }
    return TransitionParameters::Deserialize(selected_);
}

bool ScreenTransitionManager::SavePreset(const std::string& name_,const TransitionParameters& parameters_)
{
    if (name_.empty())
    {
        return false;
    }
    if (!settings.contains("Presets") || !settings["Presets"].is_object())
    {
        settings["Presets"] = nlohmann::json::object();
    }
    settings["Presets"][name_] = TransitionParameters::Serialize(parameters_);
    std::ofstream file_(SETTINGS_PATH,std::ios::binary);
    std::string text_ = settings.dump(4);
    size_t offset_ = 0;
    while ((offset_ = text_.find('\n',offset_)) != std::string::npos)
    {
        text_.replace(offset_,1,"\r\n");
        offset_ += 2;
    }
    file_ << text_ << "\r\n";
    status = file_ ? "Settings saved" : "Cannot save settings";
    return !!file_;
}

// 各Effectへ固有UIを委譲し、種類を追加してもここへ分岐を増やさない。
void ScreenTransitionManager::DrawParameters(TransitionParameters& parameters_)
{
    constexpr const char* TYPE_NAMES[] = {"FADE","RADIAL","SLASH","DISSOLVE","SHUTTER","WIPE","TEXTURE_MASK","DIAMOND","CURTAIN","CHECKER","SPIRAL","GLASS"};
    constexpr const char* EASING_NAMES[] = {"LINEAR","EASE_IN_CUBIC","EASE_OUT_CUBIC","EASE_IN_OUT_CUBIC","EASE_OUT_EXPO","EASE_OUT_BACK"};
    int type_ = static_cast<int>(parameters_.type), easing_ = static_cast<int>(parameters_.easing);
    if (ImGui::Combo("Transition Type",&type_,TYPE_NAMES,12))
    {
        parameters_.type = static_cast<TransitionType>(type_);
    }
    if (ImGui::Combo("Easing",&easing_,EASING_NAMES,6))
    {
        parameters_.easing = static_cast<TransitionEasing>(easing_);
    }
    ImGui::ColorEdit3("Color",&parameters_.color.x);
    ImGui::DragFloat("Duration Out",&parameters_.durationOut,0.01f,0,10);
    ImGui::DragFloat("Duration In",&parameters_.durationIn,0.01f,0,10);
    ImGui::DragFloat("Delay",&parameters_.delay,0.01f,0,10);
    ImGui::DragFloat("Hold Duration",&parameters_.holdDuration,0.01f,0,60);
    ImGui::DragFloat("Playback Speed",&parameters_.playbackSpeed,0.05f,0.05f,10);
    ImGui::Checkbox("Block Input",&parameters_.blockInput);
    auto editor_ = TransitionEffectFactory::Create(TransitionParameters::GetTypeName(parameters_.type));
    if (editor_)
    {
        editor_->Gui(parameters_);
    }
}

void ScreenTransitionManager::DrawGUI(bool editor_)
{
    BS_PROFILE_SCOPE("ScreenTransitionManager::DrawGUI");
    if (editor_ && ImGui::IsKeyPressed(ImGuiKey_F9,false))
    {
        showSettings = !showSettings;
    }
    if (editor_ && showSettings)
    {
        if (ImGui::Begin("Screen Transition (F9)",&showSettings))
        {
            ImGui::InputText("Preset Name",presetName,sizeof(presetName));
            ImGui::InputText("Filter Presets",presetFilter,sizeof(presetFilter));
            nlohmann::json filtered_ = nlohmann::json::object();
            if (settings.contains("Presets") && settings["Presets"].is_object())
            {
                for (const auto& item_ : settings["Presets"].items())
                {
                    if (item_.key().find(presetFilter) != std::string::npos)
                    {
                        filtered_[item_.key()] = item_.value();
                    }
                }
            }
            std::string selected_ = presetName;
            if (UIPresentationPresets::DrawPicker(filtered_,selected_))
            {
                strncpy_s(presetName,selected_.c_str(),_TRUNCATE);
                editing = GetPreset(presetName);
                if (!IsActive() || preview)
                {
                    Request(editing,TransitionPlayback::PLAY,{},{},{},false,false,-100,true);
                }
            }
            ImGui::Checkbox("Cycle Preview",&previewCycle);
            ImGui::SliderFloat("Preview Interval",&previewGap,.1f,5);
            ImGui::TextDisabled("%zu matching presets",filtered_.size());
            const auto before_ = TransitionParameters::Serialize(editing);
            DrawParameters(editing);
            if (!IsActive() || preview)
            {
                if (ImGui::Button("Cover"))
                {
                    Request(editing,TransitionPlayback::COVER,{},{},{},false,false,-100,true);
                }
                ImGui::SameLine();
                if (ImGui::Button("Reveal"))
                {
                    Request(editing,TransitionPlayback::REVEAL,{},{},{},false,false,-100,true);
                }
                ImGui::SameLine();
                if (ImGui::Button("Play"))
                {
                    Request(editing,TransitionPlayback::PLAY,{},{},{},false,false,-100,true);
                }
                if (preview && before_ != TransitionParameters::Serialize(editing))
                {
                    Request(editing,TransitionPlayback::PLAY,{},{},{},false,false,-100,true);
                }
                if (preview && ImGui::Button("Cancel Preview"))
                {
                    previewCycle = false;
                    Cancel(handle);
                }
            }
            if (ImGui::Button("Save Preset"))
            {
                SavePreset(presetName,editing);
            }
            ImGui::SameLine();
            if (ImGui::Button("Reload"))
            {
                LoadSettings();
                editing = GetPreset(presetName);
            }
            constexpr const char* PROFILES[] = {"BigShine","ANATHEMA"};
            ImGui::Combo("Scene Profile",&editingProfile,PROFILES,2);
            if (ImGui::Button("Load Profile"))
            {
                editing = GetProfile(PROFILES[editingProfile],"");
            }
            ImGui::SameLine();
            if (ImGui::Button("Save Profile"))
            {
                if (!settings.contains("Profiles") || !settings["Profiles"].is_object())
                {
                    settings["Profiles"] = nlohmann::json::object();
                }
                settings["Profiles"][PROFILES[editingProfile]] = TransitionParameters::Serialize(editing);
                SavePreset(presetName,editing);
            }
            ImGui::Text("State: %d | Progress: %.3f",static_cast<int>(sequence.GetState()),sequence.GetProgress());
            ImGui::TextWrapped("%s",status.c_str());
        }
        ImGui::End();
    }
    if (!showSettings || !editor_)
    {
        previewCycle = false;
        if (preview)
        {
            Cancel(handle);
        }
    }
    if (previewCycle && !IsActive() && previewWait >= previewGap)
    {
        // Sceneの更新後に試写を要求し、本来のScene遷移を優先する。
        std::vector<std::string> names_;
        if (settings.contains("Presets") && settings["Presets"].is_object())
        {
            for (const auto& item_ : settings["Presets"].items())
            {
                if (item_.key().find(presetFilter) != std::string::npos)
                {
                    names_.push_back(item_.key());
                }
            }
        }
        if (!names_.empty())
        {
            const auto found_ = std::find(names_.begin(),names_.end(),presetName);
            const auto index_ = found_ == names_.end() ? 0 : (static_cast<std::size_t>(found_-names_.begin())+1)%names_.size();
            strncpy_s(presetName,names_[index_].c_str(),_TRUNCATE);
            editing = GetPreset(presetName);
            Request(editing,TransitionPlayback::PLAY,{},{},{},false,false,-100,true);
            previewWait = 0;
        }
    }
    if (IsActive() && (!effect || !effect->IsReady()))
    {
        const auto viewport_ = ImGui::GetMainViewport();
        ImGui::GetForegroundDrawList(viewport_)->AddRectFilled(viewport_->Pos,ImVec2(viewport_->Pos.x+viewport_->Size.x,viewport_->Pos.y+viewport_->Size.y),
            ImGui::ColorConvertFloat4ToU32(ImVec4(parameters.color.x,parameters.color.y,parameters.color.z,std::clamp(TransitionSequence::Evaluate(sequence.GetProgress(),parameters.easing),0.0f,1.0f))));
        fallbackQueued = true;
    }
}

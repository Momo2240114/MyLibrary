#include "PrecompiledHeader.h"
#include "ScreenTransitionComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include <imgui.h>
#include <imgui_stdlib.h>

const std::string& ScreenTransitionComponent::GetName() const
{
    static const std::string NAME = "ScreenTransitionComponent";
    return NAME;
}

// Global Actionは既存のOwner名付き登録を使い、別Objectの同種Componentと競合させない。
void ScreenTransitionComponent::Start()
{
    BS_PROFILE_SCOPE("ScreenTransitionComponent::Start");
    const auto object_ = GetGameObject();
    if (!object_)
    {
        return;
    }
    const std::weak_ptr<ScreenTransitionComponent> weak_ = object_->GetComponent<ScreenTransitionComponent>();
    const auto register_ = [&](const char* name_,auto function_)
    {
        object_->RegisterGlobalAction("ScreenTransition",name_,[weak_,function_]()
        {
            const auto component_ = weak_.lock();
            if (component_ && component_->IsEnabled())
            {
                function_(*component_);
            }
        });
    };
    register_("Cover",[](ScreenTransitionComponent& component_)
    {
        component_.Cover();
    });
    register_("Reveal",[](ScreenTransitionComponent& component_)
    {
        component_.Reveal();
    });
    register_("Play",[](ScreenTransitionComponent& component_)
    {
        component_.Play();
    });
    register_("Hold",[](ScreenTransitionComponent& component_)
    {
        component_.Hold();
    });
    register_("CompleteWork",[](ScreenTransitionComponent& component_)
    {
        component_.CompleteWork();
    });
    register_("Cancel",[](ScreenTransitionComponent& component_)
    {
        component_.Cancel();
    });
    if (playOnStart)
    {
        Play();
    }
}

void ScreenTransitionComponent::OnDestroy()
{
    Cancel();
}

// 所有Componentの生存確認をManagerへ渡し、描画後通知から破棄済みObjectを操作しない。
bool ScreenTransitionComponent::Begin(TransitionPlayback mode_,std::function<void()> onCovered_,std::function<void()> onCompleted_)
{
    BS_PROFILE_SCOPE("ScreenTransitionComponent::Begin");
    const auto object_ = GetGameObject();
    if (!object_ || object_->IsPendingDestroy())
    {
        return false;
    }
    const auto self_ = object_->GetComponent<ScreenTransitionComponent>();
    const std::weak_ptr<ScreenTransitionComponent> weak_ = self_;
    auto& manager_ = ScreenTransitionManager::Instance();
    const auto selected_ = usePreset ? manager_.GetPreset(preset) : parameters;
    const auto requested_ = manager_.Request(selected_,mode_,[weak_,callback_ = std::move(onCovered_)]()
    {
        const auto component_ = weak_.lock();
        const auto owner_ = component_ ? component_->GetGameObject() : nullptr;
        if (!owner_ || owner_->IsPendingDestroy())
        {
            return;
        }
        if (component_->waitForCompletion)
        {
            component_->Hold();
        }
        if (!component_->coveredAction.empty())
        {
            owner_->ExecuteAction(component_->coveredAction);
        }
        if (callback_)
        {
            callback_();
        }
    },[weak_,callback_ = std::move(onCompleted_)]()
    {
        const auto component_ = weak_.lock();
        const auto owner_ = component_ ? component_->GetGameObject() : nullptr;
        if (!owner_ || owner_->IsPendingDestroy())
        {
            return;
        }
        if (!component_->completedAction.empty())
        {
            owner_->ExecuteAction(component_->completedAction);
        }
        if (callback_)
        {
            callback_();
        }
    },self_,true,false,priority);
    if (!requested_)
    {
        return false;
    }
    handle = requested_;
    return true;
}

bool ScreenTransitionComponent::Cover(std::function<void()> onCovered_)
{
    return Begin(TransitionPlayback::COVER,std::move(onCovered_),{});
}

bool ScreenTransitionComponent::Play(std::function<void()> onCovered_,std::function<void()> onCompleted_)
{
    return Begin(TransitionPlayback::PLAY,std::move(onCovered_),std::move(onCompleted_));
}

bool ScreenTransitionComponent::Reveal()
{
    auto& manager_ = ScreenTransitionManager::Instance();
    if (!manager_.Owns(handle))
    {
        return Begin(TransitionPlayback::REVEAL,{},{});
    }
    manager_.Reveal(handle);
    return true;
}

void ScreenTransitionComponent::Hold()
{
    ScreenTransitionManager::Instance().Hold(handle);
}

void ScreenTransitionComponent::CompleteWork()
{
    ScreenTransitionManager::Instance().CompleteWork(handle);
}

void ScreenTransitionComponent::Cancel(TransitionCancelMode mode_)
{
    ScreenTransitionManager::Instance().Cancel(handle,mode_);
    if (mode_ == TransitionCancelMode::CLEAR)
    {
        handle = 0;
    }
}

bool ScreenTransitionComponent::IsCovered() const
{
    return ScreenTransitionManager::Instance().IsCovered(handle);
}

void ScreenTransitionComponent::Gui()
{
    ImGui::InputText("Preset",&preset);
    ImGui::Checkbox("Use Preset",&usePreset);
    if (ImGui::Button("Load Preset Parameters"))
    {
        parameters = ScreenTransitionManager::Instance().GetPreset(preset);
        usePreset = false;
    }
    ScreenTransitionManager::DrawParameters(parameters);
    ImGui::InputText("On Covered Action",&coveredAction);
    ImGui::InputText("On Completed Action",&completedAction);
    ImGui::Checkbox("Wait For CompleteWork",&waitForCompletion);
    ImGui::Checkbox("Play On Start",&playOnStart);
    ImGui::DragInt("Priority",&priority,1,-50,50);
    if (ImGui::Button("Cover"))
    {
        Cover();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reveal"))
    {
        Reveal();
    }
    ImGui::SameLine();
    if (ImGui::Button("Play"))
    {
        Play();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel"))
    {
        Cancel();
    }
    if (ImGui::Button("Complete Work"))
    {
        CompleteWork();
    }
    if (ImGui::Button("Save Preset"))
    {
        ScreenTransitionManager::Instance().SavePreset(preset,parameters);
    }
}

void ScreenTransitionComponent::Serialize(nlohmann::json& json_) const
{
    json_ = {{"Preset",preset},{"UsePreset",usePreset},{"Parameters",TransitionParameters::Serialize(parameters)},
        {"OnCovered",coveredAction},{"OnCompleted",completedAction},{"PlayOnStart",playOnStart},{"WaitForCompletion",waitForCompletion},{"Priority",priority}};
}

void ScreenTransitionComponent::Deserialize(const nlohmann::json& json_)
{
    preset = json_.value("Preset",std::string("DefaultFade"));
    usePreset = json_.value("UsePreset",true);
    parameters = TransitionParameters::Deserialize(json_.value("Parameters",nlohmann::json::object()));
    coveredAction = json_.value("OnCovered",std::string());
    completedAction = json_.value("OnCompleted",std::string());
    playOnStart = json_.value("PlayOnStart",false);
    waitForCompletion = json_.value("WaitForCompletion",false);
    priority = json_.value("Priority",0);
}

#include "AIControllerComponent.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"

#include <imgui.h>
#include <imgui_stdlib.h>

void AIControllerComponent::Start()
{
    EnsureBlackboard();
    graph.EnsureDefaultGraph();
}

void AIControllerComponent::Update(float deltaTime_)
{
    if (!isRunning) return;

    EnsureBlackboard();
    if (!blackboardComponent) return;

    AIBehaviorContext context_;
    context_.owner = GetGameObject().get();
    context_.blackboard = blackboardComponent;
    context_.deltaTime = deltaTime_;
    graph.Tick(context_);
}

void AIControllerComponent::Gui()
{
    EnsureBlackboard();

    ImGui::Checkbox(IM_U8("AI実行##AIControllerComponent_RunAiCheckbox"), &isRunning);
    ImGui::SameLine();
    if (ImGui::Button(IM_U8("ツリーをリセット##AIControllerComponent_ResetTreeButton"))) ResetTree();

    ImGui::InputText(IM_U8("グラフアセット##AIControllerComponent_GraphAssetInput"), &graphAssetPath);
    if (ImGui::Button(IM_U8("グラフ保存##AIControllerComponent_SaveGraphButton")))
    {
        statusText = SaveGraphAsset(graphAssetPath) ? "Saved" : "Save failed";
    }
    ImGui::SameLine();
    if (ImGui::Button(IM_U8("グラフ読み込み##AIControllerComponent_LoadGraphButton")))
    {
        statusText = LoadGraphAsset(graphAssetPath) ? "Loaded" : "Load failed";
    }
    if (!statusText.empty()) ImGui::Text("%s", statusText.c_str());
    ImGui::TextDisabled(IM_U8("グラフ編集はAI Behavior Graphワークスペースで行います。"));
}

void AIControllerComponent::Serialize(nlohmann::json& json_) const
{
    json_["IsRunning"] = isRunning;
    json_["GraphAssetPath"] = graphAssetPath;
    graph.Serialize(json_["Graph"]);
}

void AIControllerComponent::Deserialize(const nlohmann::json& json_)
{
    isRunning = json_.value("IsRunning", true);
    graphAssetPath = json_.value("GraphAssetPath", std::string("Data/AI/BehaviorGraph/DefaultAI.json"));

    if (json_.contains("Graph"))
    {
        graph.Deserialize(json_["Graph"]);
    }
    else if (!graphAssetPath.empty())
    {
        graph.LoadFromFile(graphAssetPath);
    }

}

void AIControllerComponent::Play()
{
    isRunning = true;
}

void AIControllerComponent::Stop()
{
    isRunning = false;
}

void AIControllerComponent::ResetTree()
{
    graph.ResetRuntime();
}

bool AIControllerComponent::SaveGraphAsset(const std::string& path_) const
{
    return graph.SaveToFile(path_);
}

bool AIControllerComponent::LoadGraphAsset(const std::string& path_)
{
    const bool result_ = graph.LoadFromFile(path_);
    if (result_)
    {
        graphAssetPath = path_;
    }
    return result_;
}

void AIControllerComponent::EnsureBlackboard()
{
    std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;

    auto blackboard_ = owner_->GetComponent<BlackboardComponent>();
    if (!blackboard_)
    {
        blackboard_ = owner_->AddComponent<BlackboardComponent>();
    }
    blackboardComponent = blackboard_.get();
}

REGISTER_COMPONENT(AIControllerComponent, "AIControllerComponent")

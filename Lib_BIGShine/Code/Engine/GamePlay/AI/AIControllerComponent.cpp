#include "PrecompiledHeader.h"

#include "AIControllerComponent.h"

#include "Editor/AssetBrowserPayload.h"
#include "AIBlackboardKeys.h"
#include "AIActionComponent.h"
#include "Engine/GamePlay/Core/Component/CharacterMovementComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Game/GameMode/GameModeManager.h"
#include "Engine/Platform/WindowsDialogHelper.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
#include "Engine/Debug/TracyProfiler.h"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <cmath>

namespace
{
    float Length(const DirectX::XMFLOAT3& value_)
    {
        return std::sqrt(value_.x * value_.x + value_.y * value_.y + value_.z * value_.z);
    }

    DirectX::XMFLOAT3 DirectionTo(const DirectX::XMFLOAT3& from_, const DirectX::XMFLOAT3& to_, float& distance_)
    {
        DirectX::XMFLOAT3 direction_ =
        {
            to_.x - from_.x,
            to_.y - from_.y,
            to_.z - from_.z,
        };
        distance_ = Length(direction_);
        if (distance_ <= 0.0001f)
        {
            return { 0.0f, 0.0f, 0.0f };
        }

        direction_.x /= distance_;
        direction_.y /= distance_;
        direction_.z /= distance_;
        return direction_;
    }
}

void AIControllerComponent::Start()
{
    EnsureBlackboard();

    std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!hasSerializedSpawnPosition && owner_ && owner_->transform)
    {
        spawnPosition = owner_->transform->GetWorldPosition();
    }

    graph.EnsureDefaultGraph();
    RegisterDefaultAIActions();
}

void AIControllerComponent::Update(float deltaTime_)
{
    if (!isRunning) return;

    EnsureBlackboard();
    if (!blackboardComponent) return;

    ResolveTarget();
    UpdateAIContext(deltaTime_);
    WriteContextBlackboard();
    graph.Tick(context);
}

void AIControllerComponent::Gui()
{
    EnsureBlackboard();

    ImGui::Checkbox("Run AI##AIControllerComponent_RunAiCheckbox", &isRunning);
    ImGui::SameLine();
    if (ImGui::Button("Reset Tree##AIControllerComponent_ResetTreeButton")) ResetTree();

    ImGui::InputText("Graph Asset##AIControllerComponent_GraphAssetInput", &graphAssetPath);
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload_ = ImGui::AcceptDragDropPayload(AssetBrowserPayload::JSON))
        {
            const char* path_ = static_cast<const char*>(payload_->Data);
            if (path_ && path_[0] != '\0')
            {
                statusText = LoadGraphAsset(path_) ? "Loaded: " + std::string(path_) : "Load failed: " + std::string(path_);
            }
        }
        ImGui::EndDragDropTarget();
    }

    if (ImGui::Button("Save Graph##AIControllerComponent_SaveGraphButton"))
    {
        statusText = SaveGraphAsset(graphAssetPath) ? "Saved" : "Save failed";
    }
    ImGui::SameLine();
    if (ImGui::Button("Load Graph##AIControllerComponent_LoadGraphButton"))
    {
        statusText = LoadGraphAsset(graphAssetPath) ? "Loaded" : "Load failed";
    }
    ImGui::SameLine();
    if (ImGui::Button("Browse...##AIControllerComponent_BrowseGraphButton"))
    {
        const std::string selectedPath_ = WindowsDialogHelper::OpenJsonDialog(
            L"Open AI Behavior Graph",
            "Data/AI/BehaviorGraph");
        if (!selectedPath_.empty())
        {
            statusText = LoadGraphAsset(selectedPath_)
                ? "Loaded: " + selectedPath_
                : "Load failed: " + selectedPath_;
        }
    }
    if (!statusText.empty()) ImGui::Text("%s", statusText.c_str());

    ImGui::SeparatorText("Enemy AI");
    ImGui::InputText("Target Name", &targetName);
    ImGui::DragFloat("Detect Range", &detectRange, 0.1f, 0.0f, 9999.0f);
    ImGui::DragFloat("Attack Range", &attackRange, 0.1f, 0.0f, 9999.0f);
    ImGui::DragFloat("Lost Range", &lostRange, 0.1f, 0.0f, 9999.0f);
    ImGui::DragFloat("Return Stop Distance", &returnStopDistance, 0.05f, 0.0f, 9999.0f);
    ImGui::DragFloat("Chase Speed", &chaseSpeed, 0.05f, 0.0f, 9999.0f);
    ImGui::DragFloat("Return Speed", &returnSpeed, 0.05f, 0.0f, 9999.0f);
    ImGui::DragFloat3("Spawn Position", &spawnPosition.x, 0.1f);
    ImGui::Checkbox("Write Debug Blackboard", &writeDebugBlackboard);
    ImGui::TextDisabled("Edit the graph from the AI Behavior Graph workspace.");
}

void AIControllerComponent::Serialize(nlohmann::json& json_) const
{
    json_["IsRunning"] = isRunning;
    json_["GraphAssetPath"] = graphAssetPath;
    json_["TargetName"] = targetName;
    json_["DetectRange"] = detectRange;
    json_["AttackRange"] = attackRange;
    json_["LostRange"] = lostRange;
    json_["ReturnStopDistance"] = returnStopDistance;
    json_["ChaseSpeed"] = chaseSpeed;
    json_["ReturnSpeed"] = returnSpeed;
    json_["SpawnPosition"] = { spawnPosition.x, spawnPosition.y, spawnPosition.z };
    json_["WriteDebugBlackboard"] = writeDebugBlackboard;
    graph.Serialize(json_["Graph"]);
}

void AIControllerComponent::Deserialize(const nlohmann::json& json_)
{
    BS_PROFILE_SCOPE("AIControllerComponent::Deserialize");

    isRunning = json_.value("IsRunning", true);
    graphAssetPath = json_.value("GraphAssetPath", std::string("Data/AI/BehaviorGraph/DefaultAI.json"));
    targetName = json_.value("TargetName", std::string("Player"));
    detectRange = json_.value("DetectRange", 12.0f);
    attackRange = json_.value("AttackRange", 2.0f);
    lostRange = json_.value("LostRange", 16.0f);
    returnStopDistance = json_.value("ReturnStopDistance", 0.25f);
    chaseSpeed = json_.value("ChaseSpeed", 4.0f);
    returnSpeed = json_.value("ReturnSpeed", 3.0f);
    writeDebugBlackboard = json_.value("WriteDebugBlackboard", true);

    if (json_.contains("SpawnPosition") && json_["SpawnPosition"].is_array())
    {
        hasSerializedSpawnPosition = true;
        if (json_["SpawnPosition"].size() > 0) spawnPosition.x = json_["SpawnPosition"][0].get<float>();
        if (json_["SpawnPosition"].size() > 1) spawnPosition.y = json_["SpawnPosition"][1].get<float>();
        if (json_["SpawnPosition"].size() > 2) spawnPosition.z = json_["SpawnPosition"][2].get<float>();
    }
    else
    {
        hasSerializedSpawnPosition = false;
    }

    if (json_.contains("Graph"))
    {
        BS_PROFILE_SCOPE("AIController::DeserializeInlineGraph");
        graph.Deserialize(json_["Graph"]);
    }
    else if (!graphAssetPath.empty())
    {
        BS_PROFILE_SCOPE("AIController::LoadGraphAssetOnDeserialize");
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
    BS_PROFILE_SCOPE("AIControllerComponent::LoadGraphAsset");

    const bool result_ = graph.LoadFromFile(path_);
    if (result_)
    {
        graphAssetPath = path_;
        graph.ResetRuntime();
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

void AIControllerComponent::UpdateAIContext(float deltaTime_)
{
    std::shared_ptr<GameObject> owner_ = GetGameObject();
    context.owner = owner_.get();
    context.blackboard = blackboardComponent;
    context.globalBlackboard = nullptr;
    context.spawnPosition = spawnPosition;
    context.deltaTime = deltaTime_;
    context.hasTarget = context.target != nullptr;

    if (owner_)
    {
        if (GameModeManager* gameModeManager_ = owner_->GetManager().GetGameModeManager())
        {
            context.globalBlackboard = &gameModeManager_->GetGlobalBlackboard();
        }
    }

    if (owner_ && owner_->transform)
    {
        context.ownerPosition = owner_->transform->GetWorldPosition();
    }

    if (context.target && context.target->transform)
    {
        context.targetPosition = context.target->transform->GetWorldPosition();
        context.toTargetDirection = DirectionTo(context.ownerPosition, context.targetPosition, context.targetDistance);
    }
    else
    {
        context.targetPosition = context.ownerPosition;
        context.toTargetDirection = { 0.0f, 0.0f, 0.0f };
        context.targetDistance = 0.0f;
    }

    context.toSpawnDirection = DirectionTo(context.ownerPosition, context.spawnPosition, context.spawnDistance);
}

void AIControllerComponent::ResolveTarget()
{
    context.target = nullptr;

    std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;

    if (!targetName.empty())
    {
        std::shared_ptr<GameObject> namedTarget_ = owner_->GetManager().Find(targetName);
        if (namedTarget_)
        {
            context.target = namedTarget_.get();
            return;
        }
    }

    for (const std::shared_ptr<GameObject>& object_ : owner_->GetManager().GetObjects())
    {
        if (!object_ || object_.get() == owner_.get()) continue;
        if (object_->layer == LayerType::Player)
        {
            context.target = object_.get();
            return;
        }
    }
}

void AIControllerComponent::WriteContextBlackboard()
{
    if (!blackboardComponent || !writeDebugBlackboard) return;

    const bool isTargetInDetectRange_ = context.hasTarget && context.targetDistance <= detectRange;
    const bool isTargetInAttackRange_ = context.hasTarget && context.targetDistance <= attackRange;
    const bool isTargetLost_ = !context.hasTarget || context.targetDistance > lostRange;
    const bool isAtSpawn_ = context.spawnDistance <= returnStopDistance;

    blackboardComponent->SetBool(AIBlackboardKeys::CATEGORY_AI, AIBlackboardKeys::HAS_TARGET, context.hasTarget);
    blackboardComponent->SetFloat(AIBlackboardKeys::CATEGORY_AI, AIBlackboardKeys::TARGET_DISTANCE, context.targetDistance);
    blackboardComponent->SetFloat(AIBlackboardKeys::CATEGORY_AI, AIBlackboardKeys::SPAWN_DISTANCE, context.spawnDistance);
    blackboardComponent->SetBool(AIBlackboardKeys::CATEGORY_AI, AIBlackboardKeys::IS_TARGET_IN_DETECT_RANGE, isTargetInDetectRange_);
    blackboardComponent->SetBool(AIBlackboardKeys::CATEGORY_AI, AIBlackboardKeys::IS_TARGET_IN_ATTACK_RANGE, isTargetInAttackRange_);
    blackboardComponent->SetBool(AIBlackboardKeys::CATEGORY_AI, AIBlackboardKeys::IS_TARGET_LOST, isTargetLost_);
    blackboardComponent->SetBool(AIBlackboardKeys::CATEGORY_AI, AIBlackboardKeys::IS_AT_SPAWN, isAtSpawn_);
    blackboardComponent->SetVector3(AIBlackboardKeys::CATEGORY_AI, AIBlackboardKeys::OWNER_POSITION, { context.ownerPosition.x, context.ownerPosition.y, context.ownerPosition.z });
    blackboardComponent->SetVector3(AIBlackboardKeys::CATEGORY_AI, AIBlackboardKeys::TARGET_POSITION, { context.targetPosition.x, context.targetPosition.y, context.targetPosition.z });
    blackboardComponent->SetVector3(AIBlackboardKeys::CATEGORY_AI, AIBlackboardKeys::SPAWN_POSITION, { context.spawnPosition.x, context.spawnPosition.y, context.spawnPosition.z });
    blackboardComponent->SetVector3(AIBlackboardKeys::CATEGORY_AI, AIBlackboardKeys::TO_TARGET_DIRECTION, { context.toTargetDirection.x, context.toTargetDirection.y, context.toTargetDirection.z });
    blackboardComponent->SetVector3(AIBlackboardKeys::CATEGORY_AI, AIBlackboardKeys::TO_SPAWN_DIRECTION, { context.toSpawnDirection.x, context.toSpawnDirection.y, context.toSpawnDirection.z });
}

void AIControllerComponent::RegisterDefaultAIActions()
{
    std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;

    UnregisterDefaultAIActions();

    auto registerAction_ = [this, &owner_](const std::string& actionName_, const ActionRegistry::ActionFunc& function_)
    {
        owner_->RegisterAction(AIBlackboardKeys::CATEGORY_AI, actionName_, function_);
        registeredDefaultActionNames.push_back(owner_->BuildActionName(actionName_));
    };

    registerAction_("AI.ChaseTarget", [this]() { ActionChaseTarget(); });
    registerAction_("AI.ReturnToSpawn", [this]() { ActionReturnToSpawn(); });
    registerAction_("AI.StopMove", [this]() { ActionStopMove(); });
    registerAction_("AI.Attack", [this]() { ActionAttack(); });
    registerAction_("AI.NotifyActionEnd", [this]() { ActionNotifyActionEnd(); });
}

void AIControllerComponent::UnregisterDefaultAIActions()
{
    std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;

    for (const std::string& actionName_ : registeredDefaultActionNames)
    {
        owner_->GetLocalRegistry().RemoveAction(AIBlackboardKeys::CATEGORY_AI, actionName_);
    }
    registeredDefaultActionNames.clear();
}

void AIControllerComponent::ActionChaseTarget()
{
    if (!context.hasTarget) return;
    RequestMove(context.toTargetDirection, chaseSpeed);
}

void AIControllerComponent::ActionReturnToSpawn()
{
    if (context.spawnDistance <= returnStopDistance)
    {
        ActionStopMove();
        return;
    }

    RequestMove(context.toSpawnDirection, returnSpeed);
}

void AIControllerComponent::ActionStopMove()
{
    std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;

    if (std::shared_ptr<CharacterMovementComponent> movement_ = owner_->GetComponent<CharacterMovementComponent>())
    {
        movement_->StopHorizontalMovement();
    }
}

void AIControllerComponent::ActionAttack()
{
    EnsureBlackboard();
    if (!blackboardComponent) return;

    blackboardComponent->SetString(AIBlackboardKeys::CATEGORY_AI, AIBlackboardKeys::CURRENT_ACTION, "AI.Attack");
    blackboardComponent->SetBool(AIBlackboardKeys::CATEGORY_AI, AIBlackboardKeys::ACTION_ENDED, false);
    blackboardComponent->SetBool(AIBlackboardKeys::CATEGORY_AI, AIBlackboardKeys::IS_ACTING, true);
}

void AIControllerComponent::ActionNotifyActionEnd()
{
    EnsureBlackboard();
    if (!blackboardComponent) return;

    blackboardComponent->SetBool(AIBlackboardKeys::CATEGORY_AI, AIBlackboardKeys::ACTION_ENDED, true);
    blackboardComponent->SetBool(AIBlackboardKeys::CATEGORY_AI, AIBlackboardKeys::IS_ACTING, false);
}

void AIControllerComponent::RequestMove(const DirectX::XMFLOAT3& direction_, float speed_)
{
    std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;

    if (std::shared_ptr<CharacterMovementComponent> movement_ = owner_->GetComponent<CharacterMovementComponent>())
    {
        DirectX::XMFLOAT3 velocity_ =
        {
            direction_.x * speed_,
            0.0f,
            direction_.z * speed_,
        };
        movement_->SetHorizontalVelocity(velocity_);
        movement_->SetFacingDirection({ direction_.x, 0.0f, direction_.z });
    }
}

REGISTER_COMPONENT(AIControllerComponent, "AIControllerComponent")

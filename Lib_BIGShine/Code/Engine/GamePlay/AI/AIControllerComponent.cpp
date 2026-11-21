#include "PrecompiledHeader.h"
#include "Engine/GamePlay/System/CombatObservationSystem.h"

#include "AIControllerComponent.h"

#include "Editor/AssetBrowserPayload.h"
#include "AIBlackboardKeys.h"
#include "AIActionComponent.h"
#include "Engine/GamePlay/Core/Component/CharacterMovementComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Game/GameMode/GameModeBlackboardKeys.h"
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

// AI制御開始時にBlackboard・初期位置・BehaviorGraph・標準Actionを準備する。
void AIControllerComponent::Start()
{
    BS_PROFILE_SCOPE("AIControllerComponent::Start");

    // AI判断結果を書き込むBlackboardを、Graph読み込みより先に用意する。
    EnsureBlackboard();

    std::shared_ptr<GameObject> owner_ = GetGameObject();
    // JSONに初期位置が保存されていない場合のみ、配置時のWorld座標を帰還地点として採用する。
    if (!hasSerializedSpawnPosition && owner_ && owner_->transform)
    {
        spawnPosition = owner_->transform->GetWorldPosition();
    }

    if (!graphAssetPath.empty() && graph.GetNodes().empty())
    {
        LoadGraphAsset(graphAssetPath);
    }
    graph.EnsureDefaultGraph();
    RegisterDefaultAIActions();
}

// 毎フレーム判断材料を更新し、Blackboardへ反映した後にBehaviorGraphを1Tick進める。
void AIControllerComponent::Update(float deltaTime_)
{
    if (!isRunning)
    {
        return;
    }

    // Graphの条件ノードが読む値を先にそろえてから、RootからAI判断を進める。
    EnsureBlackboard();
    if (!blackboardComponent)
    {
        return;
    }

    // Object参照を解決してから、距離・方向などの判断材料を計算する。
    ResolveTarget();
    UpdateAIContext(deltaTime_);
    // 計算結果をConditionノードが読めるキーへ書き込み、Rootから判断を進める。
    WriteContextBlackboard();
    bool enemyAILocked_ = false;
    if (context.globalBlackboard &&
        context.globalBlackboard->GetBool(
            GameModeBlackboardKeys::CATEGORY_GAME,
            GameModeBlackboardKeys::ENEMY_AI_LOCKED,
            enemyAILocked_) &&
        enemyAILocked_)
    {
        // GameMode演出中はBehaviorGraphを進めず、現在の移動だけ止める。
        ActionStopMove();
        return;
    }
    graph.Tick(context);
}

// AI実行状態、Graph Asset、索敵距離、移動速度などの調整UIを描画する。
void AIControllerComponent::Gui()
{
    EnsureBlackboard();

    ImGui::Checkbox("Run AI##AIControllerComponent_RunAiCheckbox", &isRunning);
    ImGui::SameLine();
    if (ImGui::Button("Reset Tree##AIControllerComponent_ResetTreeButton"))
    {
        ResetTree();
    }

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
    if (!statusText.empty())
    {
        ImGui::Text("%s", statusText.c_str());
    }

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

// AIController固有の設定値をGameObject保存用JSONへ書き出す。
void AIControllerComponent::Serialize(nlohmann::json& json_) const
{
    BS_PROFILE_SCOPE("AIControllerComponent::Serialize");

    json_["IsRunning"] = isRunning;
    json_["GraphAssetPath"] = graphAssetPath;
    graph.Serialize(json_["Graph"]);
    json_["TargetName"] = targetName;
    json_["DetectRange"] = detectRange;
    json_["AttackRange"] = attackRange;
    json_["LostRange"] = lostRange;
    json_["ReturnStopDistance"] = returnStopDistance;
    json_["ChaseSpeed"] = chaseSpeed;
    json_["ReturnSpeed"] = returnSpeed;
    json_["SpawnPosition"] = { spawnPosition.x, spawnPosition.y, spawnPosition.z };
    json_["WriteDebugBlackboard"] = writeDebugBlackboard;
}

// JSONからAI設定とGraph Assetを復元し、旧形式の埋め込みGraphにも対応する。
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
        if (json_["SpawnPosition"].size() > 0)
        {
            spawnPosition.x = json_["SpawnPosition"][0].get<float>();
        }
        if (json_["SpawnPosition"].size() > 1)
        {
            spawnPosition.y = json_["SpawnPosition"][1].get<float>();
        }
        if (json_["SpawnPosition"].size() > 2)
        {
            spawnPosition.z = json_["SpawnPosition"][2].get<float>();
        }
    }
    else
    {
        hasSerializedSpawnPosition = false;
    }

    if (!graphAssetPath.empty())
    {
        BS_PROFILE_SCOPE("AIController::LoadGraphAssetOnDeserialize");
        graph.LoadFromFile(graphAssetPath);
        if (json_.contains("Graph"))
        {
            graph.RestoreEditorLayout(json_["Graph"]);
        }
    }
    else if (json_.contains("Graph"))
    {
        BS_PROFILE_SCOPE("AIController::DeserializeInlineGraphFallback");
        graph.Deserialize(json_["Graph"]);
    }
}

// BehaviorGraphの更新を再開する。
void AIControllerComponent::Play()
{
    isRunning = true;
}

// BehaviorGraphの更新を停止する。現在のRuntime状態は保持される。
void AIControllerComponent::Stop()
{
    isRunning = false;
}

// 実行中ノード、待機時間、Sequence進行位置などのRuntime状態を初期化する。
void AIControllerComponent::ResetTree()
{
    graph.ResetRuntime();
}

// 現在のBehaviorGraphを指定JSONファイルへ保存する。
bool AIControllerComponent::SaveGraphAsset(const std::string& path_) const
{
    return graph.SaveToFile(path_);
}

// 指定JSONからBehaviorGraphを読み込み、成功時のみAssetパスとRuntime状態を更新する。
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

// OwnerにBlackboardComponentがない場合は追加し、AIが使用する参照を確保する。
void AIControllerComponent::EnsureBlackboard()
{
    std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_)
    {
        return;
    }

    auto blackboard_ = owner_->GetComponent<BlackboardComponent>();
    if (!blackboard_)
    {
        blackboard_ = owner_->AddComponent<BlackboardComponent>();
    }
    blackboardComponent = blackboard_.get();
}

// Owner・Target・Spawnの位置関係を計算し、BehaviorGraphへ渡すContextを更新する。
void AIControllerComponent::UpdateAIContext(float deltaTime_)
{
    std::shared_ptr<GameObject> owner_ = GetGameObject();

    // BehaviorGraphに直接GameObjectを探させないため、必要な参照と距離情報をContextにまとめる。
    // Contextには所有権を持たせず、そのTick中に参照するObjectだけを設定する。
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

// 登録済み参照を使い、対象失効や名前指定の変更時だけ監視Systemから取得し直す。
void AIControllerComponent::ResolveTarget()
{
    context.target = nullptr;
    const auto owner_ = GetGameObject();
    if (!owner_ || !owner_->manager)
    {
        return;
    }
    const auto observation_ = owner_->GetManager().GetCombatObservationSystem();
    if (!observation_)
    {
        return;
    }
    const auto cached_ = cachedTarget.lock();
    const bool namedValid_ = cachedTargetIsNamed && cached_ && cached_->GetName() == targetName;
    const bool fallbackValid_ = !cachedTargetIsNamed && cached_ && cached_ != owner_ && cached_->GetLayer() == LayerType::Player &&
        (targetName.empty() || cachedTargetRegistrationSerial == observation_->GetRegistrationSerial());
    if (cachedTargetName == targetName && cached_ && observation_->IsRegisteredObject(cached_) && (namedValid_ || fallbackValid_))
    {
        context.target = cached_.get();
        return;
    }
    const auto named_ = targetName.empty() ? nullptr : observation_->FindObject(targetName);
    const auto target_ = named_ ? named_ : observation_->FindPlayerObject(owner_);
    cachedTarget = target_;
    cachedTargetName = targetName;
    cachedTargetIsNamed = named_ != nullptr;
    cachedTargetRegistrationSerial = observation_->GetRegistrationSerial();
    context.target = target_.get();
}

// 距離や範囲判定などのContext情報を、Conditionノードから読めるBlackboard値へ変換する。
void AIControllerComponent::WriteContextBlackboard()
{
    if (!blackboardComponent || !writeDebugBlackboard)
    {
        return;
    }

    // EditorやBehaviorGraphから同じキーを読めるよう、AIの判断材料をBlackboardへ集約する。
    // 生の距離値から、Graphで直接利用しやすいBool条件をまとめて生成する。
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

// 追跡・帰還・停止・攻撃など、BehaviorGraphから利用する標準AI Actionを登録する。
void AIControllerComponent::RegisterDefaultAIActions()
{
    std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_)
    {
        return;
    }

    UnregisterDefaultAIActions();

    // BehaviorGraphのPLAY_ACTIONノードから呼ばれる最低限の行動をLocal Actionとして登録する。
    // 登録名の記録まで共通化し、後から確実に解除できるようにする。
    auto registerAction_ = [this, &owner_](const std::string& actionName_, const ActionRegistry::ActionFunc& function_)
        {
            owner_->RegisterAction(AIBlackboardKeys::CATEGORY_AI, actionName_, function_);
            registeredDefaultActionNames.push_back(owner_->BuildActionName(actionName_));
        };

    registerAction_("AI.ChaseTarget", [this]()
    {
        ActionChaseTarget();
    });
    registerAction_("AI.ReturnToSpawn", [this]()
    {
        ActionReturnToSpawn();
    });
    registerAction_("AI.StopMove", [this]()
    {
        ActionStopMove();
    });
    registerAction_("AI.Attack", [this]()
    {
        ActionAttack();
    });
    registerAction_("AI.NotifyActionEnd", [this]()
    {
        ActionNotifyActionEnd();
    });
}

// このコンポーネントが以前登録した標準AI ActionをOwnerのLocal Registryから解除する。
void AIControllerComponent::UnregisterDefaultAIActions()
{
    std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_)
    {
        return;
    }

    for (const std::string& actionName_ : registeredDefaultActionNames)
    {
        owner_->GetLocalRegistry().RemoveAction(AIBlackboardKeys::CATEGORY_AI, actionName_);
    }
    registeredDefaultActionNames.clear();
}

// Target方向へ追跡速度で移動を要求する。
void AIControllerComponent::ActionChaseTarget()
{
    if (!context.hasTarget)
    {
        return;
    }
    RequestMove(context.toTargetDirection, chaseSpeed);
}

// 初期位置へ帰還し、停止距離以内まで戻ったら水平移動を止める。
void AIControllerComponent::ActionReturnToSpawn()
{
    if (context.spawnDistance <= returnStopDistance)
    {
        ActionStopMove();
        return;
    }

    RequestMove(context.toSpawnDirection, returnSpeed);
}

// CharacterMovementComponentの水平移動だけを停止する。
void AIControllerComponent::ActionStopMove()
{
    std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_)
    {
        return;
    }

    if (std::shared_ptr<CharacterMovementComponent> movement_ = owner_->GetComponent<CharacterMovementComponent>())
    {
        movement_->StopHorizontalMovement();
    }
}

// 攻撃開始を表す共通Blackboard値を設定する。実際の攻撃処理は別Actionとの連携を想定する。
void AIControllerComponent::ActionAttack()
{
    EnsureBlackboard();
    if (!blackboardComponent)
    {
        return;
    }

    blackboardComponent->SetString(AIBlackboardKeys::CATEGORY_AI, AIBlackboardKeys::CURRENT_ACTION, "AI.Attack");
    blackboardComponent->SetBool(AIBlackboardKeys::CATEGORY_AI, AIBlackboardKeys::ACTION_ENDED, false);
    blackboardComponent->SetBool(AIBlackboardKeys::CATEGORY_AI, AIBlackboardKeys::IS_ACTING, true);
}

// 現在のAI行動が終了したことをBlackboardへ書き込む。
void AIControllerComponent::ActionNotifyActionEnd()
{
    EnsureBlackboard();
    if (!blackboardComponent)
    {
        return;
    }

    blackboardComponent->SetBool(AIBlackboardKeys::CATEGORY_AI, AIBlackboardKeys::ACTION_ENDED, true);
    blackboardComponent->SetBool(AIBlackboardKeys::CATEGORY_AI, AIBlackboardKeys::IS_ACTING, false);
}

// 指定方向と速度から水平速度を作り、移動方向へOwnerを向ける。
void AIControllerComponent::RequestMove(const DirectX::XMFLOAT3& direction_, float speed_)
{
    std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_)
    {
        return;
    }

    if (std::shared_ptr<CharacterMovementComponent> movement_ = owner_->GetComponent<CharacterMovementComponent>())
    {
        // Y方向は物理や重力処理へ任せ、AIからはXZ平面の速度だけを設定する。
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

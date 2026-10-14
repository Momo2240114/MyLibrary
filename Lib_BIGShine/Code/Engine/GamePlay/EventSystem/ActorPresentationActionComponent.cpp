#include "PrecompiledHeader.h"
#include "ActorPresentationActionComponent.h"
#include "ActionSequence.h"
#include "PrefabSpawnActionComponent.h"
#include "ObjectDeleteActionComponent.h"
#include "Engine/Debug/TracyProfiler.h"
#include "Engine/Common/DebugLog.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/Graphics/Resource/Animation/AnimationControllerComponent.h"
#include "Engine/Graphics/VFX/MeshVFXComponent.h"
#include "Engine/Graphics/Particle/ParticleComponent.h"
#include "Engine/Graphics/Resource/GlTFModel.h"
#include "Engine/GamePlay/Core/Component/CharacterMovementComponent.h"
#include "Engine/GamePlay/Core/Component/MotionWarpComponent.h"
#include "Engine/Physics/ColliderComponent.h"
#include "Engine/Physics/HitBox.h"
#include "Engine/Audio/AudioComponent.h"
#include "Engine/GamePlay/Camera/CameraLockOnComponent.h"
#include "Engine/UI/UITextComponent.h"
#include "Engine/Graphics/VFX/WeaponTrailComponent.h"
#include "Engine/Graphics/VFX/TrailMeshComponent.h"
#include "Game/GameMode/GameModeManager.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <imgui.h>
#include <imgui_stdlib.h>

void ActorPresentationActionComponent::Start()
{
    CaptureMissingSpawnPoints();
    RegisterActions();
}

// 進行役の削除時にも、プレイヤーへ残した一時停止を解除する。
void ActorPresentationActionComponent::OnDestroy()
{
    while (!frozenActors.empty())
    {
        const auto actor_ = frozenActors.back().actor.lock();
        if (!actor_ || actor_->IsPendingDestroy()) frozenActors.pop_back();
        else SetActorFrozen(actor_, false);
    }
    RestoreRuntimePositions();
}

void ActorPresentationActionComponent::RegisterActions()
{
    const auto owner_ = GetGameObject();
    if (!owner_) return;
    const std::weak_ptr<GameObject> weakOwner_ = owner_;
    owner_->RegisterGlobalAction("ActorPresentation", "RestoreSpawnPoints", [weakOwner_]()
    {
        const auto owner_ = weakOwner_.lock();
        const auto component_ = owner_ ? owner_->GetComponent<ActorPresentationActionComponent>() : nullptr;
        if (component_) component_->RestoreSpawnPoints();
    });
    owner_->RegisterGlobalAction("ActorPresentation", "CaptureRuntimePositions", [weakOwner_]()
    {
        const auto owner_ = weakOwner_.lock();
        if (const auto component_ = owner_ ? owner_->GetComponent<ActorPresentationActionComponent>() : nullptr)
            component_->CaptureRuntimePositions();
    });
    owner_->RegisterGlobalAction("ActorPresentation", "RestoreRuntimePositions", [weakOwner_]()
    {
        const auto owner_ = weakOwner_.lock();
        if (const auto component_ = owner_ ? owner_->GetComponent<ActorPresentationActionComponent>() : nullptr)
            component_->RestoreRuntimePositions();
    });
    for (int index_ = 0; index_ < static_cast<int>(presets.size()); ++index_)
    {
        if (presets[index_].actionName.empty()) continue;
        owner_->RegisterGlobalAction("ActorPresentation", presets[index_].actionName, [weakOwner_, index_]()
        {
            const auto owner_ = weakOwner_.lock();
            const auto component_ = owner_ ? owner_->GetComponent<ActorPresentationActionComponent>() : nullptr;
            if (component_) component_->ExecutePreset(index_);
        });
    }
}

void ActorPresentationActionComponent::CaptureMissingSpawnPoints()
{
    BS_PROFILE_SCOPE("ActorPresentationActionComponent::CaptureMissingSpawnPoints");
    const auto owner_ = GetGameObject();
    if (!owner_) return;
    for (const auto& preset_ : presets)
    {
        if (preset_.actorName.empty()) continue;
        const auto existing_ = std::find_if(spawnPoints.begin(), spawnPoints.end(),
            [&](const ActorPresentationSpawnPoint& point_) { return point_.actorName == preset_.actorName; });
        if (existing_ != spawnPoints.end()) continue;
        const auto actor_ = owner_->GetManager().Find(preset_.actorName);
        if (!actor_ || !actor_->transform) continue;
        spawnPoints.push_back({ preset_.actorName,
            actor_->transform->localPosition, actor_->transform->localRotation });
    }
}

void ActorPresentationActionComponent::CaptureSpawnPoints()
{
    BS_PROFILE_SCOPE("ActorPresentationActionComponent::CaptureSpawnPoints");
    CaptureMissingSpawnPoints();
    const auto owner_ = GetGameObject();
    if (!owner_) return;
    for (auto& point_ : spawnPoints)
    {
        const auto actor_ = owner_->GetManager().Find(point_.actorName);
        if (!actor_ || !actor_->transform) continue;
        point_.position = actor_->transform->localPosition;
        point_.rotation = actor_->transform->localRotation;
    }
}

void ActorPresentationActionComponent::RestoreSpawnPoints()
{
    BS_PROFILE_SCOPE("ActorPresentationActionComponent::RestoreSpawnPoints");
    const auto owner_ = GetGameObject();
    if (!owner_) return;
    CaptureMissingSpawnPoints();
    // 前回の移動・旋回補間が、復元直後の配置を上書きしないようにする。
    movingActor.reset();
    turningActor.reset();
    for (const auto& point_ : spawnPoints)
    {
        const auto actor_ = owner_->GetManager().Find(point_.actorName);
        if (!actor_ || !actor_->transform) continue;
        actor_->transform->localPosition = point_.position;
        actor_->transform->localRotation = point_.rotation;
        actor_->transform->MarkDirty();
        actor_->transform->UpdateWorldMatrix();
    }
}

void ActorPresentationActionComponent::CaptureRuntimePositions()
{
    BS_PROFILE_SCOPE("ActorPresentationActionComponent::CaptureRuntimePositions");
    if (!runtimePositions.empty()) return;
    const auto owner_ = GetGameObject();
    if (!owner_) return;
    for (const auto& preset_ : presets)
    {
        const auto duplicate_ = std::find_if(runtimePositions.begin(), runtimePositions.end(),
            [&](const ActorPresentationSpawnPoint& point_) { return point_.actorName == preset_.actorName; });
        if (duplicate_ != runtimePositions.end()) continue;
        const auto actor_ = owner_->GetManager().Find(preset_.actorName);
        if (actor_ && actor_->transform)
            runtimePositions.push_back({ preset_.actorName, actor_->transform->localPosition, actor_->transform->localRotation });
    }
}

void ActorPresentationActionComponent::RestoreRuntimePositions()
{
    BS_PROFILE_SCOPE("ActorPresentationActionComponent::RestoreRuntimePositions");
    const auto owner_ = GetGameObject();
    if (!owner_) return;
    movingActor.reset();
    turningActor.reset();
    for (const auto& point_ : runtimePositions)
    {
        const auto actor_ = owner_->GetManager().Find(point_.actorName);
        if (!actor_ || !actor_->transform) continue;
        actor_->transform->localPosition = point_.position;
        actor_->transform->MarkDirty();
        actor_->transform->SetRotation(point_.rotation);
        actor_->transform->UpdateWorldMatrix();
    }
    runtimePositions.clear();
}

void ActorPresentationActionComponent::ExecutePreset(int index_)
{
    BS_PROFILE_SCOPE("ActorPresentationActionComponent::ExecutePreset");
    if (index_ < 0 || index_ >= static_cast<int>(presets.size())) return;
    const auto owner_ = GetGameObject();
    if (!owner_) return;
    const auto& preset_ = presets[index_];
    // 旧JSONの種別値を残し、生成・削除の実処理は専用Componentへ委譲する。
    if (preset_.type == ActorPresentationActionType::SPAWN_PREFAB)
    {
        if (const auto spawner_ = owner_->GetComponent<PrefabSpawnActionComponent>())
            spawner_->SpawnPrefab(preset_.prefabName, preset_.actorName);
        return;
    }
    if (preset_.type == ActorPresentationActionType::DESTROY_ACTOR)
    {
        if (const auto deleter_ = owner_->GetComponent<ObjectDeleteActionComponent>())
            deleter_->DeleteActor(preset_.actorName, preset_.skipInPreview);
        return;
    }
    if (preset_.type == ActorPresentationActionType::CLEANUP_SPAWNED)
    {
        if (const auto deleter_ = owner_->GetComponent<ObjectDeleteActionComponent>()) deleter_->CleanupSpawnedObjects(true);
        return;
    }
    const auto actor_ = owner_->GetManager().Find(preset_.actorName);
    if (!actor_) return;
    if (preset_.type == ActorPresentationActionType::COMMIT_ACTOR)
    {
        if (const auto spawner_ = owner_->GetComponent<PrefabSpawnActionComponent>()) spawner_->CommitActor(actor_);
        return;
    }
    if (preset_.type == ActorPresentationActionType::SET_UI_TEXT)
    {
        if (const auto text_ = actor_->GetComponent<UITextComponent>()) text_->SetText(preset_.stateName);
        return;
    }
    if (preset_.type == ActorPresentationActionType::SET_LOCK_ON_TARGET)
    {
        const auto target_ = owner_->GetManager().Find(preset_.targetName);
        if (const auto camera_ = actor_->GetComponent<CameraLockOnComponent>())
        {
            camera_->SetLockOnTarget(target_);
            camera_->SetLockOnFocusTarget(target_);
        }
        return;
    }
    if (preset_.type == ActorPresentationActionType::CLOSE_HIT_BOXES)
    {
        // 撃破時点で開いている武器やProjectileの受付窓を閉じる。
        for (const auto& object_ : owner_->GetManager().GetObjects())
            if (object_) for (const auto& hitBox_ : object_->GetComponents<HitBox>()) hitBox_->Deactivate();
        return;
    }
    if (preset_.type == ActorPresentationActionType::FREEZE_ACTOR ||
        preset_.type == ActorPresentationActionType::RELEASE_ACTOR)
    {
        const bool frozen_ = preset_.type == ActorPresentationActionType::FREEZE_ACTOR;
        SetActorFrozen(actor_, frozen_);
        if (preset_.includeChildren && actor_->transform)
        {
            // 本体の非表示後に、部位の物理形状やHurtBoxだけが残らないようにする。
            const auto visit_ = [&](auto&& self_, const std::shared_ptr<Transform>& transform_) -> void
            {
                for (const auto& child_ : transform_->children)
                {
                    if (!child_) continue;
                    if (const auto childActor_ = child_->gameObject.lock()) SetActorFrozen(childActor_, frozen_);
                    self_(self_, child_);
                }
            };
            visit_(visit_, actor_->transform);
        }
        return;
    }
    if (preset_.type == ActorPresentationActionType::SET_VISIBLE ||
        preset_.type == ActorPresentationActionType::SET_HIDDEN)
    {
        if (const auto model_ = actor_->GetComponent<GltfModelCommponennt>())
            model_->SetRuntimeVisible(preset_.type == ActorPresentationActionType::SET_VISIBLE);
        return;
    }
    if (preset_.type == ActorPresentationActionType::PLAY_AUDIO ||
        preset_.type == ActorPresentationActionType::STOP_AUDIO)
    {
        if (const auto audio_ = actor_->GetComponent<AudioComponent>())
        {
            if (preset_.type == ActorPresentationActionType::PLAY_AUDIO) audio_->Play();
            else audio_->Stop();
        }
        return;
    }
    if (preset_.type == ActorPresentationActionType::MOVE_TO_TARGET)
    {
        const auto target_ = owner_->GetManager().Find(preset_.targetName);
        if (!target_ || !target_->transform || !actor_->transform) return;
        startMovePosition = actor_->transform->localPosition;
        // Markerのワールド位置を対象の親座標へ戻し、親付きObjectでも同じ地点へ配置する。
        const auto worldPosition_ = target_->transform->GetWorldPosition();
        auto position_ = DirectX::XMLoadFloat3(&worldPosition_);
        if (const auto parent_ = actor_->transform->parent.lock())
            position_ = DirectX::XMVector3TransformCoord(position_,
                DirectX::XMMatrixInverse(nullptr, parent_->GetWorldMatrix()));
        DirectX::XMStoreFloat3(&targetMovePosition, position_);
        movingActor = actor_;
        moveElapsed = 0.0f;
        moveDuration = (std::max)(preset_.duration, 0.0f);
        smoothMovement = preset_.smoothMovement;
        moveSwayAmplitude = preset_.swayAmplitude;
        moveSwayCycles = preset_.swayCycles;
        moveTrailWidth = preset_.trailWidth;
        UpdateMovement(0.0f);
        if (preset_.matchTargetRotation && moveDuration <= 0.0f)
        {
            // Markerのワールド回転を使い、対象のモデル用Scaleは維持する。
            DirectX::XMVECTOR markerScale_, markerRotation_, markerPosition_;
            if (DirectX::XMMatrixDecompose(&markerScale_, &markerRotation_, &markerPosition_, target_->transform->GetWorldMatrix()))
            {
                const auto scale_ = actor_->transform->GetWorldScale();
                actor_->transform->ApplyWorldMatrix(
                    DirectX::XMMatrixScaling(scale_.x, scale_.y, scale_.z) *
                    DirectX::XMMatrixRotationQuaternion(markerRotation_) * DirectX::XMMatrixTranslationFromVector(markerPosition_));
            }
        }
        return;
    }
    if (preset_.type == ActorPresentationActionType::PLAY_MESH_VFX ||
        preset_.type == ActorPresentationActionType::STOP_MESH_VFX)
    {
        const auto meshVfx_ = actor_->GetComponent<MeshVFXComponent>();
        if (!meshVfx_) return;
        const bool play_ = preset_.type == ActorPresentationActionType::PLAY_MESH_VFX;
        if (const auto trail_ = actor_->GetComponent<WeaponTrailComponent>())
        {
            if (play_) trail_->BeginTrail(true);
            else trail_->EndTrail(true);
            return;
        }
        // Stopだけでは時刻が止まってMeshが残るため、描画フラグも切り替える。
        for (auto& part_ : meshVfx_->GetAsset().parts) part_.renderEnabled = play_;
        if (const auto trail_ = actor_->GetComponent<TrailMeshComponent>())
        {
            trail_->Clear();
            trail_->SetEmitting(play_);
        }
        if (play_)
        {
            meshVfx_->Reset();
            meshVfx_->Play();
        }
        else meshVfx_->Stop();
        return;
    }
    if (preset_.type == ActorPresentationActionType::PLAY_PARTICLE ||
        preset_.type == ActorPresentationActionType::STOP_PARTICLE ||
        preset_.type == ActorPresentationActionType::RESET_PARTICLE)
    {
        const auto particle_ = actor_->GetComponent<ParticleComponent>();
        if (!particle_) return;
        // 通常停止では粒子を寿命まで残し、復元用Resetでは残留粒子も消す。
        if (preset_.type == ActorPresentationActionType::RESET_PARTICLE) particle_->ResetRuntime();
        else if (preset_.type == ActorPresentationActionType::PLAY_PARTICLE) particle_->Play();
        else particle_->Stop();
        return;
    }
    if (preset_.type == ActorPresentationActionType::MOVE_FROM_OFFSET)
    {
        if (!actor_->transform) return;
        targetMovePosition = actor_->transform->localPosition;
        startMovePosition =
        {
            targetMovePosition.x + preset_.positionOffset.x,
            targetMovePosition.y + preset_.positionOffset.y,
            targetMovePosition.z + preset_.positionOffset.z
        };
        movingActor = actor_;
        moveElapsed = 0.0f;
        moveDuration = (std::max)(preset_.duration, 0.0f);
        smoothMovement = false;
        moveSwayAmplitude = 0.0f;
        // 到着地点は変更せず、門の奥へ移してから元の配置へ歩かせる。
        UpdateMovement(0.0f);
        return;
    }
    if (preset_.type == ActorPresentationActionType::FACE_TARGET)
    {
        const auto target_ = owner_->GetManager().Find(preset_.targetName);
        if (!target_ || !actor_->transform || !target_->transform) return;
        const auto actorPosition_ = actor_->transform->GetWorldPosition();
        const auto targetPosition_ = target_->transform->GetWorldPosition();
        const float x_ = targetPosition_.x - actorPosition_.x;
        const float z_ = targetPosition_.z - actorPosition_.z;
        if (x_ * x_ + z_ * z_ <= 0.0001f) return;
        // 水平方向だけを向け、親の回転を差し引いてローカルYawへ戻す。
        startYaw = actor_->transform->localRotation.y;
        const float parentYaw_ = actor_->transform->GetWorldRotation().y - startYaw;
        targetYaw = DirectX::XMConvertToDegrees(std::atan2(x_, z_)) - parentYaw_;
        targetYaw = startYaw + std::remainder(targetYaw - startYaw, 360.0f);
        turningActor = actor_;
        turnElapsed = 0.0f;
        turnDuration = (std::max)(preset_.duration, 0.0f);
        if (turnDuration <= 0.0f)
        {
            auto rotation_ = actor_->transform->localRotation;
            rotation_.y = targetYaw;
            actor_->transform->SetRotation(rotation_);
            turningActor.reset();
        }
        return;
    }
    const auto animation_ = actor_->GetComponent<AnimationControllerComponent>();
    if (!animation_) return;
    // 明示した演出だけAssetを再反映し、既存のScene内Stateを優先する設定は維持する。
    if (!preset_.stateAssetPath.empty() && (preset_.refreshStateAssetOnPlay ||
        animation_->GetStates().find(preset_.stateName) == animation_->GetStates().end()))
    {
        // 演出側でAssetを復元し、既存のpublic APIでStateと再生Actionを登録する。
        std::ifstream file_(preset_.stateAssetPath);
        if (!file_.is_open())
        {
            BS_LOG_INFO(LOG_CATEGORY::GAME, "Presentation state asset could not be opened: " + preset_.stateAssetPath);
            return;
        }
        try
        {
            nlohmann::json assetJson_;
            file_ >> assetJson_;
            if (assetJson_.contains("AssetType") && assetJson_.value("AssetType", "") != "AnimationState") return;
            const AnimState loadedState_ = assetJson_.contains("State")
                ? assetJson_["State"].get<AnimState>() : assetJson_.get<AnimState>();
            if (preset_.stateName.empty() || loadedState_.name != preset_.stateName) return;
            animation_->AddState(preset_.stateName, loadedState_.animationIndex);
            auto& states_ = animation_->GetStates();
            const auto stateIterator_ = states_.find(preset_.stateName);
            if (stateIterator_ == states_.end() || !stateIterator_->second) return;
            // AddStateが用意した実体を保ち、Loop・イベント・RootMotionなどの保存設定を反映する。
            *stateIterator_->second = loadedState_;
        }
        catch (const nlohmann::json::exception&)
        {
            BS_LOG_INFO(LOG_CATEGORY::GAME, "Presentation state asset is invalid: " + preset_.stateAssetPath);
            return;
        }
    }
    if (preset_.type == ActorPresentationActionType::RELEASE_ANIMATION)
    {
        // 復帰Stateを先に反映してから、通常の遷移グラフへ制御を返す。
        if (!preset_.stateName.empty()) animation_->ChangeState(preset_.stateName, preset_.duration);
        animation_->SetPresentationControlled(false);
        return;
    }
    if (preset_.stateName.empty() || animation_->GetStates().find(preset_.stateName) == animation_->GetStates().end())
    {
        BS_LOG_INFO(LOG_CATEGORY::GAME, "Presentation animation is not configured: " + preset_.actionName);
        return;
    }
    animation_->SetPresentationControlled(true);
    // Asset再反映を選んだ演出は、同じStateも先頭から再開始する。
    animation_->ChangeState(preset_.stateName, (std::max)(preset_.duration, 0.0f), preset_.refreshStateAssetOnPlay);
}

void ActorPresentationActionComponent::Update(float deltaTime_)
{
    BS_PROFILE_SCOPE("ActorPresentationActionComponent::Update");
    UpdateMovement(deltaTime_);
    const auto actor_ = turningActor.lock();
    if (!actor_ || actor_->IsPendingDestroy() || !actor_->manager || !actor_->transform) return;
    turnElapsed += (std::max)(deltaTime_, 0.0f);
    const float rate_ = turnDuration <= 0.0f ? 1.0f : std::clamp(turnElapsed / turnDuration, 0.0f, 1.0f);
    const float easedRate_ = rate_ * rate_ * (3.0f - 2.0f * rate_);
    auto rotation_ = actor_->transform->localRotation;
    rotation_.y = startYaw + (targetYaw - startYaw) * easedRate_;
    actor_->transform->SetRotation(rotation_);
    if (rate_ >= 1.0f) turningActor.reset();
}

void ActorPresentationActionComponent::UpdateMovement(float deltaTime_)
{
    BS_PROFILE_SCOPE("ActorPresentationActionComponent::UpdateMovement");
    const auto actor_ = movingActor.lock();
    if (!actor_ || actor_->IsPendingDestroy() || !actor_->manager || !actor_->transform)
    {
        movingActor.reset();
        return;
    }
    moveElapsed += (std::max)(deltaTime_, 0.0f);
    const float rate_ = moveDuration <= 0.0f ? 1.0f : std::clamp(moveElapsed / moveDuration, 0.0f, 1.0f);
    const float movementRate_ = smoothMovement ? rate_ * rate_ * (3.0f - 2.0f * rate_) : rate_;
    const float x_ = targetMovePosition.x - startMovePosition.x;
    const float z_ = targetMovePosition.z - startMovePosition.z;
    const float length_ = std::sqrt(x_ * x_ + z_ * z_);
    // 終端で蛇行量を0へ戻し、到着Markerとの位置ずれを残さない。
    const float sway_ = std::sin(rate_ * DirectX::XM_PI) *
        std::sin(rate_ * DirectX::XM_2PI * moveSwayCycles) * moveSwayAmplitude;
    // 歩行Animationの一定の歩幅に合わせ、登場移動を等速にして足滑りを抑える。
    actor_->transform->localPosition =
    {
        startMovePosition.x + x_ * movementRate_ + (length_ > 0.0001f ? z_ / length_ * sway_ : 0.0f),
        startMovePosition.y + (targetMovePosition.y - startMovePosition.y) * movementRate_,
        startMovePosition.z + z_ * movementRate_ - (length_ > 0.0001f ? x_ / length_ * sway_ : 0.0f)
    };
    // ワールド座標キャッシュを更新し、Colliderの外部Transform変更検出と描画へ反映する。
    actor_->transform->MarkDirty();
    actor_->transform->UpdateWorldMatrix();
    // 曲がる移動経路から世界座標の帯を生成し、泥を点状の塊だけにしない。
    if (const auto trail_ = actor_->GetComponent<TrailMeshComponent>())
    {
        if (trail_->IsEmitting())
        {
            const auto position_ = actor_->transform->GetWorldPosition();
            const float width_ = moveTrailWidth * 0.5f;
            const float sideX_ = length_ > 0.0001f ? z_ / length_ * width_ : width_;
            const float sideZ_ = length_ > 0.0001f ? -x_ / length_ * width_ : 0.0f;
            trail_->AddSample({position_.x + sideX_, position_.y, position_.z + sideZ_},
                {position_.x - sideX_, position_.y, position_.z - sideZ_});
        }
    }
    if (rate_ >= 1.0f) movingActor.reset();
}

// Animationと描画を継続し、通常移動・AI・物理判定だけを一時停止する。
void ActorPresentationActionComponent::SetActorFrozen(const std::shared_ptr<GameObject>& actor_, bool frozen_)
{
    BS_PROFILE_SCOPE("ActorPresentationActionComponent::SetActorFrozen");
    if (!actor_ || actor_->IsPendingDestroy()) return;
    // 単体再生を繰り返しても、破棄した生成物の復帰記録を残さない。
    frozenActors.erase(std::remove_if(frozenActors.begin(), frozenActors.end(), [](const FrozenActor& entry_)
    {
        const auto object_ = entry_.actor.lock();
        return !object_ || object_->IsPendingDestroy();
    }), frozenActors.end());
    const auto existing_ = std::find_if(frozenActors.begin(), frozenActors.end(),
        [&](const FrozenActor& item_) { return item_.actor.lock() == actor_; });
    if (!frozen_)
    {
        if (existing_ == frozenActors.end()) return;
        if (const auto movement_ = actor_->GetComponent<CharacterMovementComponent>())
        {
            movement_->ResetMovementRequests();
            movement_->ResetVerticalVelocity();
        }
        for (const auto& state_ : existing_->componentStates)
        {
            const auto component_ = state_.first.lock();
            if (!component_) continue;
            if (const auto collider_ = std::dynamic_pointer_cast<ColliderComponent>(component_))
                collider_->velocity = { 0.0f, 0.0f, 0.0f };
            // 旧攻撃の受付窓は再開せず、次の戦闘ActionによるActivateを待つ。
            component_->SetRuntimeSuppressed(state_.second);
            if (const auto hitBox_ = std::dynamic_pointer_cast<HitBox>(component_)) hitBox_->Deactivate();
        }
        frozenActors.erase(existing_);
        return;
    }
    if (existing_ != frozenActors.end())
    {
        // 単体再生の切り替えでShowが呼ばれても、最初の復帰状態を保持して停止し直す。
        for (const auto& state_ : existing_->componentStates)
            if (const auto component_ = state_.first.lock()) component_->SetRuntimeSuppressed(true);
        if (const auto movement_ = actor_->GetComponent<CharacterMovementComponent>()) movement_->ResetMovementRequests();
        return;
    }
    FrozenActor item_;
    item_.actor = actor_;
    // 通常戦闘の補正要求が、演出後のIdleやMarker移動へ持ち越されないようにする。
    if (const auto warp_ = actor_->GetComponent<MotionWarpComponent>()) warp_->Clear();
    if (const auto sequence_ = actor_->GetComponent<ActionSequence>())
        sequence_->StopPlayback(sequence_->GetPlaybackGeneration());
    if (const auto movement_ = actor_->GetComponent<CharacterMovementComponent>())
    {
        movement_->ResetMovementRequests();
        movement_->ResetVerticalVelocity();
    }
    for (const auto& component_ : actor_->components)
    {
        if (!component_) continue;
        const auto& name_ = component_->GetName();
        if (name_ != "CharacterMovementComponent" && name_ != "MotionWarpComponent" &&
            name_ != "AIControllerComponent" && name_ != "DragonAIComponent" &&
            name_ != "DragonAttackControllerComponent" && name_ != "AIActionComponent" &&
            name_ != "AttackEmitterComponent" &&
            name_ != "HumanBossAIComponent" && name_ != "HumanBossActionComponent") continue;
        item_.componentStates.push_back({ component_, component_->IsRuntimeSuppressed() });
        component_->SetRuntimeSuppressed(true);
    }
    for (const auto& component_ : actor_->colliderComponents)
    {
        if (!component_) continue;
        item_.componentStates.push_back({ component_, component_->IsRuntimeSuppressed() });
        if (const auto collider_ = std::dynamic_pointer_cast<ColliderComponent>(component_))
        {
            collider_->velocity = { 0.0f, 0.0f, 0.0f };
            // 無効化後はPhysicsのStep対象から外れるため、残ったActorも取り外す。
            // 復帰時には既存のPreparePhysicsStepが新しいTransformから再生成する。
            collider_->currentTriggers.clear();
            collider_->m_CollisionMainFolds.clear();
            collider_->ReleasePhysXActor();
        }
        component_->SetRuntimeSuppressed(true);
    }
    frozenActors.push_back(std::move(item_));
}

void ActorPresentationActionComponent::Gui()
{
    ImGui::SeparatorText("Spawn Points");
    ImGui::TextUnformatted("Spawn points are saved separately from current actor transforms.");
    if (ImGui::Button("Capture Current Positions")) CaptureSpawnPoints();
    ImGui::SameLine();
    if (ImGui::Button("Restore Spawn Positions")) RestoreSpawnPoints();
    ImGui::TextUnformatted("Capture the intended layout, then save the scene.");
    for (int index_ = 0; index_ < static_cast<int>(spawnPoints.size()); ++index_)
    {
        ImGui::PushID(index_);
        auto& point_ = spawnPoints[index_];
        if (ImGui::TreeNode(point_.actorName.c_str()))
        {
            ImGui::DragFloat3("Spawn Position", &point_.position.x, 0.05f);
            ImGui::DragFloat3("Spawn Rotation", &point_.rotation.x, 0.5f);
            if (ImGui::Button("Capture This Actor"))
            {
                const auto owner_ = GetGameObject();
                const auto actor_ = owner_ ? owner_->GetManager().Find(point_.actorName) : nullptr;
                if (actor_ && actor_->transform)
                {
                    point_.position = actor_->transform->localPosition;
                    point_.rotation = actor_->transform->localRotation;
                }
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    ImGui::SeparatorText("Actor Actions");
    const char* TYPES[] = { "Play Animation", "Face Target", "Release Animation", "Move From Offset",
        "Play Mesh VFX", "Stop Mesh VFX", "Play Particle", "Stop Particle", "Move To Marker",
        "Show Model", "Hide Model", "Freeze Actor", "Release Actor", "Play Audio", "Stop Audio",
        "Set LockOn Target", "Set UI Text", "Close HitBoxes", "Reset Particle",
        "Spawn Prefab", "Destroy Actor", "Cleanup Spawned Objects", "Commit Battle Actor" };
    for (int index_ = 0; index_ < static_cast<int>(presets.size()); ++index_)
    {
        ImGui::PushID(index_);
        auto& preset_ = presets[index_];
        ImGui::InputText("Action Name", &preset_.actionName);
        ImGui::InputText("Actor Object", &preset_.actorName);
        ImGui::InputText("Prefab Name", &preset_.prefabName);
        ImGui::Checkbox("Skip Destruction In Preview", &preset_.skipInPreview);
        int type_ = static_cast<int>(preset_.type);
        if (ImGui::Combo("Type", &type_, TYPES, IM_ARRAYSIZE(TYPES))) preset_.type = static_cast<ActorPresentationActionType>(type_);
        ImGui::InputText("Animation State", &preset_.stateName);
        ImGui::InputText("State Asset Path", &preset_.stateAssetPath);
        ImGui::Checkbox("Refresh Asset And Restart On Play", &preset_.refreshStateAssetOnPlay);
        ImGui::InputText("Look Target", &preset_.targetName);
        ImGui::DragFloat("Blend / Turn / Move Duration", &preset_.duration, 0.01f, 0.0f, 60.0f);
        if (preset_.type == ActorPresentationActionType::FREEZE_ACTOR ||
            preset_.type == ActorPresentationActionType::RELEASE_ACTOR)
            ImGui::Checkbox("Include Child Actors", &preset_.includeChildren);
        if (preset_.type == ActorPresentationActionType::MOVE_FROM_OFFSET)
            ImGui::DragFloat3("Entry Position Offset", &preset_.positionOffset.x, 0.05f);
        if (preset_.type == ActorPresentationActionType::MOVE_TO_TARGET)
        {
            ImGui::Checkbox("Smooth Movement", &preset_.smoothMovement);
            ImGui::DragFloat("Trail Width", &preset_.trailWidth, 0.05f, 0.1f, 10.0f);
            ImGui::Checkbox("Match Marker Rotation (Teleport)", &preset_.matchTargetRotation);
            ImGui::DragFloat("Sway Amplitude", &preset_.swayAmplitude, 0.05f, 0.0f, 20.0f);
            ImGui::DragFloat("Sway Cycles", &preset_.swayCycles, 0.05f, 0.0f, 20.0f);
        }
        ImGui::Separator();
        ImGui::PopID();
    }
    if (ImGui::Button("Add Actor Action")) presets.emplace_back();
    if (ImGui::Button("Register Actor Actions")) RegisterActions();
}

void ActorPresentationActionComponent::Serialize(nlohmann::json& json_) const
{
    json_["SpawnPoints"] = nlohmann::json::array();
    for (const auto& point_ : spawnPoints)
    {
        json_["SpawnPoints"].push_back({
            {"ActorName", point_.actorName},
            {"Position", {point_.position.x, point_.position.y, point_.position.z}},
            {"Rotation", {point_.rotation.x, point_.rotation.y, point_.rotation.z}}
        });
    }
    json_["Presets"] = nlohmann::json::array();
    for (const auto& preset_ : presets)
        json_["Presets"].push_back({ {"ActionName", preset_.actionName}, {"ActorName", preset_.actorName},
            {"StateName", preset_.stateName}, {"StateAssetPath", preset_.stateAssetPath}, {"RefreshStateAssetOnPlay", preset_.refreshStateAssetOnPlay}, {"TargetName", preset_.targetName}, {"PrefabName", preset_.prefabName}, {"SkipInPreview", preset_.skipInPreview}, {"TrailWidth", preset_.trailWidth},
            {"Type", static_cast<int>(preset_.type)}, {"Duration", preset_.duration},
            {"SmoothMovement", preset_.smoothMovement}, {"SwayAmplitude", preset_.swayAmplitude}, {"SwayCycles", preset_.swayCycles},
            {"MatchTargetRotation", preset_.matchTargetRotation},
            {"IncludeChildren", preset_.includeChildren},
            {"PositionOffset", {preset_.positionOffset.x, preset_.positionOffset.y, preset_.positionOffset.z}} });
}

void ActorPresentationActionComponent::Deserialize(const nlohmann::json& json_)
{
    spawnPoints.clear();
    if (json_.contains("SpawnPoints") && json_["SpawnPoints"].is_array())
    {
        for (const auto& item_ : json_["SpawnPoints"])
        {
            if (!item_.is_object()) continue;
            ActorPresentationSpawnPoint point_;
            point_.actorName = item_.value("ActorName", std::string{});
            if (point_.actorName.empty() || !item_.contains("Position") ||
                !item_["Position"].is_array() || item_["Position"].size() != 3) continue;
            point_.position = { item_["Position"][0], item_["Position"][1], item_["Position"][2] };
            if (item_.contains("Rotation") && item_["Rotation"].is_array() && item_["Rotation"].size() == 3)
                point_.rotation = { item_["Rotation"][0], item_["Rotation"][1], item_["Rotation"][2] };
            spawnPoints.push_back(point_);
        }
    }
    presets.clear();
    turningActor.reset();
    movingActor.reset();
    moveElapsed = 0.0f;
    moveDuration = 0.0f;
    if (!json_.contains("Presets") || !json_["Presets"].is_array()) return;
    for (const auto& item_ : json_["Presets"])
    {
        ActorPresentationActionPreset preset_;
        preset_.actionName = item_.value("ActionName", std::string{});
        preset_.actorName = item_.value("ActorName", std::string{});
        preset_.stateName = item_.value("StateName", std::string{});
        preset_.stateAssetPath = item_.value("StateAssetPath", std::string{});
        preset_.refreshStateAssetOnPlay = item_.value("RefreshStateAssetOnPlay", false);
        preset_.targetName = item_.value("TargetName", std::string{});
        preset_.prefabName = item_.value("PrefabName", std::string{});
        preset_.skipInPreview = item_.value("SkipInPreview", false);
        preset_.trailWidth = (std::max)(item_.value("TrailWidth", 1.2f), 0.1f);
        preset_.type = static_cast<ActorPresentationActionType>(std::clamp(item_.value("Type", 0), 0, static_cast<int>(ActorPresentationActionType::COMMIT_ACTOR)));
        preset_.duration = (std::max)(item_.value("Duration", 0.2f), 0.0f);
        preset_.smoothMovement = item_.value("SmoothMovement", true);
        preset_.matchTargetRotation = item_.value("MatchTargetRotation", false);
        preset_.includeChildren = item_.value("IncludeChildren", false);
        preset_.swayAmplitude = (std::max)(item_.value("SwayAmplitude", 0.0f), 0.0f);
        preset_.swayCycles = (std::max)(item_.value("SwayCycles", 2.0f), 0.0f);
        // 旧JSONではオフセットを0とし、既存のAnimation・旋回設定を維持する。
        if (item_.contains("PositionOffset") && item_["PositionOffset"].is_array() && item_["PositionOffset"].size() == 3)
            preset_.positionOffset = { item_["PositionOffset"][0], item_["PositionOffset"][1], item_["PositionOffset"][2] };
        presets.push_back(preset_);
    }
}

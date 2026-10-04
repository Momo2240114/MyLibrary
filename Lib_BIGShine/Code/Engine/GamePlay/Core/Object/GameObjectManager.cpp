#include "PrecompiledHeader.h"

#include "GameObjectManager.h"
#include "Engine/Audio/AudioManager.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Physics/ColliderComponent.h"
#include "Engine/Physics/MeshCollider.h"
#include "Engine/Physics/PhysXSystem.h"
#include "Engine/Physics/ContainmentVolumeComponent.h"
#include "Engine/GamePlay/Transform/ModelAttachmentComponent.h"
#include "Engine/Physics/Collision.h"
#include "Engine/Physics/BoxCollider.h"
#include <PxPhysicsAPI.h>

#include "Engine/Graphics/Decal/DecalComponent.h"
#include"ImGuizmo.h"
#include <nlohmann/json.hpp>
#include"Engine/GamePlay/Transform/TransformDirector.h"
#include"Engine/GamePlay/Camera/CameraDirectorComponent.h"
#include"Engine/GamePlay/EventSystem/ActionSequence.h"

#include"Engine/Graphics/PostEffect/ColorFilterDirector.h"
#include "Engine/Graphics/PostEffect/MotionBlurDirector.h"
#include "Engine/Graphics/PostEffect/RadialBlurDirector.h"
#include "Engine/Graphics/PostEffect/VignetteDirector.h"
#include "Engine/GamePlay/EventSystem/TimeScaleDirector.h"

#include <algorithm>
#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include"Engine/GamePlay/EventSystem/EventManager.h"

#include "Engine/GamePlay/System/TimeManager.h"
#include "Engine/GamePlay/Core/Component/CharacterMovementComponent.h"
#include "Engine/GamePlay/Camera/CameraManager.h"
#include "Engine/GamePlay/Camera/CameraComponent.h"
#include "Engine/Graphics/VFX/MeshVFXComponent.h"
#include "Engine/GamePlay/Graphics/MotionVelocityComponent.h"
#include "Engine/Graphics/Resource/ModelResourceManager.h"
#include "Engine/Common/DebugLog.h"
#include "Engine/UI/UIVertexModifierComponent.h"
#include "Engine/UI/UIWidgetComponent.h"
#include "Engine/UI/WidgetBoardComponent.h"

#include "Engine/Debug/TracyProfiler.h"
#include "Engine/Platform/WindowsDialogHelper.h"

#include <chrono>

namespace
{
    double GetSceneProfileMilliseconds()
    {
        // Scene Load計測用に、steady_clockの現在時刻をmsで取得する。
        const auto now_ = std::chrono::steady_clock::now().time_since_epoch();
        return std::chrono::duration<double, std::milli>(now_).count();
    }

    std::shared_ptr<WidgetBoardComponent> ResolveWidgetBoard(const std::shared_ptr<GameObject>& object_)
    {
        // UI Spriteが所属するWidgetBoardを、自分から親方向へ探す。
        if (!object_) return nullptr;

        auto board_ = object_->GetComponent<WidgetBoardComponent>();
        if (board_) return board_;

        auto transform_ = object_->transform;
        while (transform_)
        {
            auto parentTransform_ = transform_->parent.lock();
            if (!parentTransform_) break;

            auto parentObject_ = parentTransform_->gameObject.lock();
            if (parentObject_)
            {
                board_ = parentObject_->GetComponent<WidgetBoardComponent>();
                if (board_) return board_;
            }

            transform_ = parentTransform_;
        }

        return nullptr;
    }

    int CalculateHierarchyDepth(const std::shared_ptr<GameObject>& object_)
    {
        // UI描画の同階層順を安定させるため、Transform階層の深さを数える。
        int depth_ = 0;
        if (!object_ || !object_->transform) return depth_;

        auto parentTransform_ = object_->transform->parent.lock();
        while (parentTransform_)
        {
            ++depth_;
            parentTransform_ = parentTransform_->parent.lock();
        }

        return depth_;
    }

    void SaveCurrentIBLSettings(nlohmann::json& graphicsJson_)
    {
        // Sceneごとに使用するIBLを切り替えられるよう、現在のHDRI設定を保存する。
        GraphicsDeviceManager& graphics_ = GraphicsDeviceManager::Instance();
        graphicsJson_["IBL"] = {
            {
                "specularHdrPath",
                WindowsDialogHelper::PathToUtf8(
                    std::filesystem::path(graphics_.GetIBLSpecularHdrPath()))
            },
            {"hdrIntensity", graphics_.GetIBLHdrIntensity()}
        };
    }

    void LoadSceneIBLSettings(const nlohmann::json& graphicsJson_)
    {
        // 古いSceneListとの互換性を保つため、IBL設定が無い場合は現在の設定を維持する。
        if (!graphicsJson_.contains("IBL") || !graphicsJson_["IBL"].is_object())
        {
            return;
        }

        const nlohmann::json& iblJson_ = graphicsJson_["IBL"];
        const std::string specularHdrPath_ = iblJson_.value("specularHdrPath", "");
        if (specularHdrPath_.empty())
        {
            return;
        }

        GraphicsDeviceManager& graphics_ = GraphicsDeviceManager::Instance();
        const float hdrIntensity_ = iblJson_.value("hdrIntensity", graphics_.GetIBLHdrIntensity());
        const std::filesystem::path hdrPath_ = WindowsDialogHelper::Utf8ToPath(specularHdrPath_);
        if (!graphics_.ReloadIBLTextures(hdrPath_.wstring(), hdrIntensity_))
        {
            BS_LOG_WARNING(LOG_CATEGORY::CORE, "LoadScene IBL reload failed: " + graphics_.GetLastIBLError());
        }
    }
}

void GameObjectManager::Initialize()
{
    // Scene全体で共有するSystemを生成し、相互参照と描画Systemを初期化する。
    collisionStepSystem->SetAttackSystem(attackSystem.get());
    collisionStepSystem->SetJustDodgeSystem(justDodgeSystem.get());
    collisionStepSystem->SetJustGuardSystem(justGuardSystem.get());
    collisionStepSystem->SetGuardSystem(guardSystem.get());

    particleRenderSystem = std::make_unique<ParticleRenderSystem>();
    particleRenderSystem->Initialize(GraphicsDeviceManager::Instance().GetDevice());

    meshVFXRenderSystem = std::make_unique<MeshVFXRenderSystem>();
    meshVFXRenderSystem->Initialize(GraphicsDeviceManager::Instance().GetDevice());

    motionVelocityRenderSystem = std::make_unique<MotionVelocityRenderSystem>();
    const bool motionVelocityInitialized_ =
        motionVelocityRenderSystem->Initialize(GraphicsDeviceManager::Instance().GetDevice());
    if (motionVelocityInitialized_)
    {
        GraphicsDeviceManager::Instance()
            .GetPostRenderSystem()
            .GetMotionBlurSystem()
            .SetObjectVelocityRenderer(motionVelocityRenderSystem.get());
    }

    resourceManager = std::make_unique<ResourceManager>();

    gameModeManager = std::make_shared<GameModeManager>();
    gameModeManager->Start();

    timeManager = std::make_shared<TimeManager>();
    effectManager->Initialize(this);
    RegisterAttackCallbacks();
    ModelResourceManager::Instance().LoadPreloadSettings();
    ModelResourceManager::Instance().StartAsyncLoader();

    cameraManager = std::make_shared<CameraManager>();
    cameraManager->Initialize();
}

std::shared_ptr<GameObject> GameObjectManager::Find(const std::string& name_)
{
    // Object名検索は実行時に何度も通るため、文字列比較ではなくNameIdの索引を優先する。
    const auto indexIterator_ = objectNameIndex.find(StringIdUtility::Hash(name_));
    if (indexIterator_ == objectNameIndex.end()) return nullptr;

    auto& candidates_ = indexIterator_->second;
    for (auto candidateIterator_ = candidates_.begin(); candidateIterator_ != candidates_.end();)
    {
        std::shared_ptr<GameObject> object_ = candidateIterator_->lock();
        if (!object_)
        {
            candidateIterator_ = candidates_.erase(candidateIterator_);
            continue;
        }

        if (object_->GetName() == name_) return object_;
        ++candidateIterator_;
    }

    // Hash衝突などで候補が一致しなかった場合だけ、従来検索で安全確認する。
    for (const std::shared_ptr<GameObject>& object_ : objects)
    {
        if (object_ && object_->GetName() == name_) return object_;
    }

    return nullptr;
}

void GameObjectManager::RegisterObjectNameIndex(const std::shared_ptr<GameObject>& object_)
{
    if (!object_) return;

    // 同名ObjectもPool用途では存在し得るため、候補リストとして保持する。
    objectNameIndex[StringIdUtility::Hash(object_->GetName())].push_back(object_);
}

void GameObjectManager::UnregisterObjectNameIndex(
    GameObject* object_,
    const std::string& name_)
{
    if (!object_) return;

    const auto indexIterator_ = objectNameIndex.find(StringIdUtility::Hash(name_));
    if (indexIterator_ == objectNameIndex.end()) return;

    auto& candidates_ = indexIterator_->second;
    candidates_.erase(
        std::remove_if(
            candidates_.begin(),
            candidates_.end(),
            [object_](const std::weak_ptr<GameObject>& weakObject_)
            {
                const std::shared_ptr<GameObject> lockedObject_ = weakObject_.lock();
                return !lockedObject_ || lockedObject_.get() == object_;
            }),
        candidates_.end());

    if (candidates_.empty())
    {
        objectNameIndex.erase(indexIterator_);
    }
}

void GameObjectManager::NotifyObjectNameChanged(
    GameObject* object_,
    const std::string& oldName_,
    const std::string& newName_)
{
    if (!object_ || oldName_ == newName_) return;

    UnregisterObjectNameIndex(object_, oldName_);
    for (const std::shared_ptr<GameObject>& candidate_ : objects)
    {
        if (candidate_.get() == object_)
        {
            RegisterObjectNameIndex(candidate_);
            return;
        }
    }
}

void GameObjectManager::Update(float elapsedTime_)
{
    // Scene Runtime状態に応じて、Load待ち、初期化、通常更新を切り替える。
    switch (sceneRuntimeState)
    {
    case SCENE_RUNTIME_STATE::LOADING:
        UpdateLoadingRuntime(elapsedTime_);
        return;
    case SCENE_RUNTIME_STATE::INITIALIZING:
        FinalizeSceneInitialization();
        return;
    case SCENE_RUNTIME_STATE::FAILED:
        return;
    case SCENE_RUNTIME_STATE::RUNNING:
    default:
        UpdateRunningRuntime(elapsedTime_);
        return;
    }
}

void GameObjectManager::UpdateRunningRuntime(float elapsedTime_)
{
    BS_PROFILE_SCOPE("SceneRuntime::Running");
    BS_PROFILE_PLOT("GameObject.ActiveCount", static_cast<int64_t>(objects.size()));
    float worldDeltaTime_ = elapsedTime_;

    // Hit履歴やJustDodge受付など、Frame単位で消えるSystem状態を先に初期化する。
    {
        BS_PROFILE_SCOPE("AttackSystem::BeginFrame");
        attackSystem->BeginFrame();
    }
    {
        BS_PROFILE_SCOPE("JustDodgeSystem::BeginFrame");
        justDodgeSystem->BeginFrame();
    }
    {
        BS_PROFILE_SCOPE("JustGuardSystem::BeginFrame");
        justGuardSystem->BeginFrame();
    }
    {
        BS_PROFILE_SCOPE("GuardSystem::BeginFrame");
        guardSystem->BeginFrame();
    }

    {
        BS_PROFILE_SCOPE("TimeManager::Update");
        timeManager->Update(elapsedTime_);
    }
    {
        BS_PROFILE_SCOPE("gameModeManager::Update");
        gameModeManager->Update(elapsedTime_);
    }

    {
        BS_PROFILE_SCOPE("ModelResourceManager::UpdateAsyncLoads");
        ModelResourceManager::Instance().UpdateAsyncLoads(
            GraphicsDeviceManager::Instance().GetDevice(),
            2.0f);
    }

    if (!pendingStarts.empty())
    {
        // Update中に追加されたObjectも、準備できたものだけStartして実行対象へ入れる。
        BS_PROFILE_SCOPE("GameObjectManager::ProcessPendingStarts");

        std::vector<std::shared_ptr<GameObject>> startList_ = std::move(pendingStarts);
        std::vector<std::shared_ptr<GameObject>> readyStartList_;
        pendingStarts.clear();

        {
            BS_PROFILE_SCOPE("PendingStarts::StartComponents");

            for (auto& object_ : startList_)
            {
                if (!object_) continue;
                bool isReadyToStart_ = false;
                {
                    BS_PROFILE_SCOPE("PendingStarts::CheckReadyToStart");
                    isReadyToStart_ = object_->IsReadyToStart();
                }
                if (!isReadyToStart_)
                {
                    pendingStarts.push_back(object_);
                    continue;
                }

                if (auto particleComponent_ = object_->GetComponent<ParticleComponent>())
                {
                    BS_PROFILE_SCOPE("PendingStarts::RegisterParticleComponent");
                    particleComponents.push_back(particleComponent_);
                }

                {
                    BS_PROFILE_SCOPE("GameObject::CallAllComponentStart");
                    object_->CallAllComponentStart();
                }
                readyStartList_.push_back(object_);

                {
                    BS_PROFILE_SCOPE("Director::RegisterActions");

                    if (auto director_ = object_->GetComponent<TransformDirector>())
                    {
                        director_->RegisterActions();
                    }

                    if (auto director_ = object_->GetComponent<ColorFilterDirector>())
                    {
                        director_->RegisterActions();
                    }

                    if (auto director_ = object_->GetComponent<MotionBlurDirector>())
                    {
                        director_->RegisterActions();
                    }

                    if (auto director_ = object_->GetComponent<RadialBlurDirector>())
                    {
                        director_->RegisterActions();
                    }

                    if (auto director_ = object_->GetComponent<VignetteDirector>())
                    {
                        director_->RegisterActions();
                    }

                    if (auto director_ = object_->GetComponent<TimeScaleDirector>())
                    {
                        director_->RegisterActions();
                    }

                    if (auto director_ = object_->GetComponent<CameraDirector>())
                    {
                        director_->RegisterActions();
                    }
                }
            }
        }

        {
            BS_PROFILE_SCOPE("PendingStarts::RegisterSequence");

            for (auto& object_ : readyStartList_)
            {
                if (!object_) continue;

                if (auto sequence_ = object_->GetComponent<ActionSequence>())
                {
                    sequence_->RegisterSequence("");
                }
            }
        }
    }

    {
        BS_PROFILE_SCOPE("TimeManager::PrepareCache");
        // LayerごとのTimeScaleをCacheし、PhysX側にも世界Scaleを同期する。
        timeManager->PrepareCache();
        const float worldScale_ = timeManager->GetCachedScale(0u);
        worldDeltaTime_ = elapsedTime_ * worldScale_;
        PhysXSystem::SetTimeScale(worldScale_);
    }

    {
        BS_PROFILE_SCOPE("CollisionStepSystem::SyncFromPhysics");
        // Physicsから現在のCollider状態を取り込み、Game側Component更新の前提にする。
        collisionStepSystem->SyncFromPhysics(worldDeltaTime_);
    }

    {
        BS_PROFILE_SCOPE("GameObjectManager::ObjectsUpdate");
        const bool useUnscaledCameraDelta_ = timeManager && timeManager->ShouldUseUnscaledCameraDelta();
        int updatedObjectCount_ = 0;

        // Animation EventでEffectなどが追加されても、生成直後Objectは次Frameから更新する。
        const size_t updateObjectCount_ = objects.size();
        for (size_t index_ = 0; index_ < updateObjectCount_; ++index_)
        {
            const std::shared_ptr<GameObject> object_ = objects[index_];
            if (!object_) continue;

            const float scale_ = timeManager->GetCachedScale(object_->GetLayerBit());
            float objectDeltaTime_ = elapsedTime_ * scale_;
            if (useUnscaledCameraDelta_ && object_->GetComponent<CameraComponent>())
            {
                objectDeltaTime_ = elapsedTime_;
            }

            if (auto movement_ = object_->GetComponent<CharacterMovementComponent>())
            {
                // TimeScale中の移動速度補正用に、元のDeltaTimeも渡しておく。
                movement_->SetUnscaledDeltaTime(elapsedTime_);
            }

            object_->Update(objectDeltaTime_);
            ++updatedObjectCount_;
        }
        {
            BS_PROFILE_PLOT("GameObject.UpdatedCount", updatedObjectCount_);
        }
    }

    {
        BS_PROFILE_SCOPE("ModelAttachmentComponent::SyncAttachment");

        // ModelのBoneやSocketへ追従するObjectは、通常Update後にAttachmentを同期する。
        for (auto& attachment_ : FindComponents<ModelAttachmentComponent>())
        {
            if (attachment_ && attachment_->IsEnabled())
            {
                attachment_->SyncAttachment();
            }
        }
    }

    {
        BS_PROFILE_SCOPE("ContainmentVolumeComponent::ApplyContainment");

        // 移動後に閉じ込め領域を適用し、CameraやPlayerが範囲外へ出ないようにする。
        for (auto& containment_ : FindComponents<ContainmentVolumeComponent>())
        {
            if (containment_ && containment_->IsEnabled())
            {
                containment_->ApplyContainment();
            }
        }
    }

    {
        BS_PROFILE_SCOPE("CollisionStepSystem::Step");
        // Game側で決まった速度やCollider状態を元に、Collision/Attack/Guard判定を進める。
        collisionStepSystem->Step(worldDeltaTime_);
    }

    {
        BS_PROFILE_SCOPE("GameObjectManager::ObjectsLateUpdate");
        const bool useUnscaledCameraDelta_ = timeManager && timeManager->ShouldUseUnscaledCameraDelta();
        int lateUpdatedObjectCount_ = 0;

        // Update中に生成されたObjectは次FrameからLateUpdateへ参加させる。
        const size_t lateUpdateObjectCount_ = objects.size();
        for (size_t index_ = 0; index_ < lateUpdateObjectCount_; ++index_)
        {
            const std::shared_ptr<GameObject> object_ = objects[index_];
            if (!object_) continue;

            const float scale_ = timeManager->GetCachedScale(object_->GetLayerBit());
            float objectDeltaTime_ = elapsedTime_ * scale_;
            if (useUnscaledCameraDelta_ && object_->GetComponent<CameraComponent>())
            {
                objectDeltaTime_ = elapsedTime_;
            }

            object_->LateUpdate(objectDeltaTime_);
            ++lateUpdatedObjectCount_;
        }

        BS_PROFILE_PLOT("GameObject.LateUpdatedCount", lateUpdatedObjectCount_);
    }

    {
        BS_PROFILE_SCOPE("GameObjectManager::UpdateTransform");

        // 全Component更新後にTransform行列を確定し、描画や親子同期の基準にする。
        for (auto& object_ : objects)
        {
            if (!object_) continue;
            object_->UpdateTransform();
        }
    }

    {
        BS_PROFILE_SCOPE("EffectManager::Update");
        effectManager->Update(worldDeltaTime_);
    }

    if (!pendingDestroys.empty())
    {
        // 削除予約はFrame終端でまとめて反映し、更新中の配列破壊を避ける。
        BS_PROFILE_SCOPE("GameObjectManager::ProcessDestroyedObjects");

        std::vector<std::shared_ptr<GameObject>> currentDestroys_ = std::move(pendingDestroys);
        pendingDestroys.clear();

        for (auto& object_ : currentDestroys_)
        {
            ApplyDestroys(object_);
        }
    }

    {
        BS_PROFILE_SCOPE("ParticleComponents::EraseExpired");

        // weak_ptr化したParticle参照のうち、Object削除済みのものを掃除する。
        particleComponents.erase(
            std::remove_if(
                particleComponents.begin(),
                particleComponents.end(),
                [](const std::weak_ptr<ParticleComponent>& component_)
                {
                    return component_.expired();
                }),
            particleComponents.end()
        );
    }

    {
        BS_PROFILE_SCOPE("CameraManager::Update");
        // Object更新後のTransformを使ってActiveCameraを更新する。
        cameraManager->Update(elapsedTime_, objects);
    }

    {
        BS_PROFILE_SCOPE("AudioManager::UpdateListenerFromCamera");
        AudioManager::Instance().UpdateListenerFromCamera(
            cameraManager->GetActiveCamera().get(),
            elapsedTime_);
    }
}

void GameObjectManager::UpdateLoadingRuntime(float elapsedTime_)
{
    (void)elapsedTime_;
    BS_PROFILE_SCOPE("SceneLoading::Update");

    {
        BS_PROFILE_SCOPE("ModelResourceManager::UpdateAsyncLoads");
        ModelResourceManager::Instance().UpdateAsyncLoads(
            GraphicsDeviceManager::Instance().GetDevice(),
            2.0f);
    }

    ProcessPendingStarts();

    // 必須ObjectのAssetとRuntime準備が揃うまではRUNNINGへ進めない。
    sceneLoadProgress = ModelResourceManager::Instance().GetLoadProgress();

    if (HasRequiredRuntimeFailure())
    {
        FailSceneRuntime("Required scene asset failed to load.");
        return;
    }

    if (!WaitRequiredAssets()) return;
    if (!WaitRuntimeReady()) return;

    sceneRuntimeState = SCENE_RUNTIME_STATE::INITIALIZING;
}

void GameObjectManager::ProcessPendingStarts()
{
    if (pendingStarts.empty()) return;

    // Load中/Running中どちらからでも使う、Object Start待機リストの共通処理。
    BS_PROFILE_SCOPE("GameObjectManager::ProcessPendingStarts");

    std::vector<std::shared_ptr<GameObject>> startList_ = std::move(pendingStarts);
    std::vector<std::shared_ptr<GameObject>> readyStartList_;
    pendingStarts.clear();

    {
        BS_PROFILE_SCOPE("PendingStarts::StartComponents");

        for (auto& object_ : startList_)
        {
            if (!object_) continue;
            bool isReadyToStart_ = false;
            {
                BS_PROFILE_SCOPE("PendingStarts::CheckReadyToStart");
                isReadyToStart_ = object_->IsReadyToStart();
            }
            if (!isReadyToStart_)
            {
                pendingStarts.push_back(object_);
                continue;
            }

            if (auto particleComponent_ = object_->GetComponent<ParticleComponent>())
            {
                BS_PROFILE_SCOPE("PendingStarts::RegisterParticleComponent");
                particleComponents.push_back(particleComponent_);
            }

            {
                BS_PROFILE_SCOPE("GameObject::CallAllComponentStart");
                object_->CallAllComponentStart();
            }
            readyStartList_.push_back(object_);

            {
                BS_PROFILE_SCOPE("Director::RegisterActions");

                if (auto director_ = object_->GetComponent<TransformDirector>())
                {
                    director_->RegisterActions();
                }

                if (auto director_ = object_->GetComponent<ColorFilterDirector>())
                {
                    director_->RegisterActions();
                }

                if (auto director_ = object_->GetComponent<MotionBlurDirector>())
                {
                    director_->RegisterActions();
                }

                if (auto director_ = object_->GetComponent<RadialBlurDirector>())
                {
                    director_->RegisterActions();
                }

                if (auto director_ = object_->GetComponent<VignetteDirector>())
                {
                    director_->RegisterActions();
                }

                if (auto director_ = object_->GetComponent<TimeScaleDirector>())
                {
                    director_->RegisterActions();
                }

                if (auto director_ = object_->GetComponent<CameraDirector>())
                {
                    director_->RegisterActions();
                }
            }
        }
    }

    {
        BS_PROFILE_SCOPE("PendingStarts::RegisterSequence");

        for (auto& object_ : readyStartList_)
        {
            if (!object_) continue;

            if (auto sequence_ = object_->GetComponent<ActionSequence>())
            {
                sequence_->RegisterSequence("");
            }
        }
    }
}

bool GameObjectManager::WaitRequiredAssets()
{
    BS_PROFILE_SCOPE("SceneLoading::WaitRequiredAssets");

    // Scene開始に必須なObjectだけ、LoadReadyになるまで待つ。
    for (const std::shared_ptr<GameObject>& object_ : objects)
    {
        if (!object_ || !object_->IsRequiredForSceneStart()) continue;
        const std::string objectName_ = object_->GetName();
        {
            BS_PROFILE_SCOPE_DYNAMIC(objectName_.c_str());
            if (!object_->IsReadyToStart()) return false;
        }
    }

    return true;
}

bool GameObjectManager::WaitRuntimeReady()
{
    BS_PROFILE_SCOPE("SceneLoading::WaitRuntimeReady");

    // Asset Load後、Runtimeで使える状態になったかをComponentへ問い合わせる。
    for (const std::shared_ptr<GameObject>& object_ : objects)
    {
        if (!object_ || !object_->IsRequiredForSceneStart()) continue;
        const std::string objectName_ = object_->GetName();
        {
            BS_PROFILE_SCOPE_DYNAMIC(objectName_.c_str());
            if (!object_->IsRuntimeReady()) return false;
        }
    }

    return true;
}

bool GameObjectManager::HasRequiredRuntimeFailure()
{
    // 必須ObjectのどれかがRuntime失敗を報告したら、SceneをFAILEDへ移す。
    for (const std::shared_ptr<GameObject>& object_ : objects)
    {
        if (!object_ || !object_->IsRequiredForSceneStart()) continue;
        if (object_->HasRuntimeFailure()) return true;
    }

    return false;
}

void GameObjectManager::FinalizeSceneInitialization()
{
    BS_PROFILE_SCOPE("SceneLoading::FinalizeInitialization");

    // Load完了後にTransformとCameraを一度確定し、通常更新へ移る準備をする。
    for (const std::shared_ptr<GameObject>& object_ : objects)
    {
        if (!object_) continue;
        object_->UpdateTransform();
    }

    for (const std::shared_ptr<MeshCollider>& meshCollider_ : FindComponents<MeshCollider>())
    {
        if (meshCollider_ && meshCollider_->IsEnabled())
        {
            meshCollider_->Update(0.0f);
        }
    }

    for (auto& attachment_ : FindComponents<ModelAttachmentComponent>())
    {
        if (attachment_ && attachment_->IsEnabled())
        {
            attachment_->SyncAttachment();
        }
    }

    collisionStepSystem->PrepareInitialPhysicsState(0.0f);

    for (const std::shared_ptr<GameObject>& object_ : objects)
    {
        if (!object_) continue;
        object_->UpdateTransform();
    }

    sceneLoadProgress = 1.0f;
    sceneRuntimeState = SCENE_RUNTIME_STATE::RUNNING;
}

void GameObjectManager::FailSceneRuntime(const std::string& message_)
{
    // 必須Load失敗など、Sceneを通常更新へ進められない状態を記録する。
    sceneRuntimeError = message_;
    sceneRuntimeState = SCENE_RUNTIME_STATE::FAILED;
    BS_LOG_ERROR(LOG_CATEGORY::GAME, "[SceneRuntime] " + sceneRuntimeError);
}

void GameObjectManager::Clear()
{
    // Scene切り替え前にObject、各System、描画状態、Action/Eventを初期状態へ戻す。
    sceneLoadProgress = 0.0f;
    sceneRuntimeError.clear();

    GraphicsDeviceManager& graphics_ = GraphicsDeviceManager::Instance();
    graphics_.ResetFilter();
    graphics_.GetRadialBlurParams() = RadialBlurSystem::Parameters{};
    graphics_.GetVignetteParams() = VignetteSystem::Parameters{};
    if (timeManager)
    {
        timeManager->ResetToDefaults(static_cast<uint32_t>(TarGetLayerType::All), true);
    }

    selectedObject = nullptr;

    collisionStepSystem->Clear();
    attackSystem->Clear();
    guardSystem->Clear();
    effectManager->Clear();

    objects.clear();
    objectNameIndex.clear();
    pendingStarts.clear();
    pendingDestroys.clear();
    particleComponents.clear();

    // Scene objects were removed, so cached effect pool objects must be
    // recreated and registered with this manager before the next scene runs.
    effectManager->RebuildRuntimePools();
}

void GameObjectManager::AddObject(std::shared_ptr<GameObject> object_)
{
    if (!object_) return;

    for (const std::shared_ptr<GameObject>& existing_ : objects)
    {
        if (existing_ == object_) return;
        if (existing_ && existing_->saveFlg && object_->saveFlg &&
            existing_->GetName() == object_->GetName())
        {
            return;
        }
    }

    object_->SetManager(this);
    for (const std::shared_ptr<ColliderComponent>& collider_ :
        object_->GetComponents<ColliderComponent>())
    {
        if (collider_) collisionStepSystem->RegisterCollider(collider_);
    }
    objects.push_back(object_);
    RegisterObjectNameIndex(object_);
    pendingStarts.push_back(object_);
}

void GameObjectManager::DetachObject(const std::shared_ptr<GameObject>& object_)
{
    if (!object_) return;

    for (const std::shared_ptr<ColliderComponent>& collider_ :
        object_->GetComponents<ColliderComponent>())
    {
        if (collider_) collisionStepSystem->UnregisterCollider(collider_.get());
    }

    if (selectedObject == object_) selectedObject = nullptr;
    UnregisterObjectNameIndex(object_.get(), object_->GetName());
    objects.erase(
        std::remove(objects.begin(), objects.end(), object_),
        objects.end());
    pendingStarts.erase(
        std::remove(pendingStarts.begin(), pendingStarts.end(), object_),
        pendingStarts.end());
    pendingDestroys.erase(
        std::remove(pendingDestroys.begin(), pendingDestroys.end(), object_),
        pendingDestroys.end());

    const std::shared_ptr<ParticleComponent> particle_ =
        object_->GetComponent<ParticleComponent>();
    particleComponents.erase(
        std::remove_if(
            particleComponents.begin(),
            particleComponents.end(),
            [&particle_](const std::weak_ptr<ParticleComponent>& component_)
            {
                return component_.expired() || component_.lock() == particle_;
            }),
        particleComponents.end());
}
void GameObjectManager::RemoveObject(const std::string& name_)
{
    for (const std::shared_ptr<GameObject>& object_ : objects)
    {
        if (!object_ || object_->GetName() != name_) continue;
        for (const std::shared_ptr<ColliderComponent>& collider_ :
            object_->GetComponents<ColliderComponent>())
        {
            collisionStepSystem->UnregisterCollider(collider_.get());
        }
        UnregisterObjectNameIndex(object_.get(), object_->GetName());
    }

    const auto iterator_ = std::remove_if(
        objects.begin(),
        objects.end(),
        [&name_](const std::shared_ptr<GameObject>& object_)
        {
            return object_ && object_->GetName() == name_;
        });

    if (iterator_ == objects.end()) return;
    if (selectedObject && selectedObject->GetName() == name_) selectedObject = nullptr;
    objects.erase(iterator_, objects.end());
}

size_t GameObjectManager::SetAllCollisionShapeRenderEnabled(bool enabled_)
{
    size_t changedCount_ = 0;
    for (const std::shared_ptr<GameObject>& object_ : objects)
    {
        if (!object_) continue;
        for (const std::shared_ptr<ColliderComponent>& collider_ :
            object_->GetComponents<ColliderComponent>())
        {
            if (!collider_) continue;
            collider_->shapeRenderEnabled = enabled_;
            ++changedCount_;
        }
    }
    return changedCount_;
}

size_t GameObjectManager::SetCollisionShapeRenderEnabledInHierarchy(
    const std::string& rootObjectName_,
    bool enabled_,
    bool includeChildren_)
{
    size_t changedCount_ = 0;
    std::vector<std::shared_ptr<GameObject>> pendingObjects_;
    const std::shared_ptr<GameObject> rootObject_ = Find(rootObjectName_);
    if (!rootObject_) return 0;

    pendingObjects_.push_back(rootObject_);
    while (!pendingObjects_.empty())
    {
        const std::shared_ptr<GameObject> object_ = pendingObjects_.back();
        pendingObjects_.pop_back();
        if (!object_) continue;

        for (const std::shared_ptr<ColliderComponent>& collider_ :
            object_->GetComponents<ColliderComponent>())
        {
            if (!collider_) continue;
            collider_->shapeRenderEnabled = enabled_;
            ++changedCount_;
        }

        if (!includeChildren_ || !object_->transform) continue;
        for (const std::shared_ptr<Transform>& childTransform_ :
            object_->transform->children)
        {
            if (!childTransform_) continue;
            if (const std::shared_ptr<GameObject> childObject_ =
                childTransform_->gameObject.lock())
            {
                pendingObjects_.push_back(childObject_);
            }
        }
    }

    return changedCount_;
}

size_t GameObjectManager::CountAllCollisionShapes()
{
    size_t colliderCount_ = 0;
    for (const std::shared_ptr<GameObject>& object_ : objects)
    {
        if (!object_) continue;
        colliderCount_ += object_->GetComponents<ColliderComponent>().size();
    }
    return colliderCount_;
}

size_t GameObjectManager::CountCollisionShapesInHierarchy(
    const std::string& rootObjectName_,
    bool includeChildren_)
{
    size_t colliderCount_ = 0;
    std::vector<std::shared_ptr<GameObject>> pendingObjects_;
    const std::shared_ptr<GameObject> rootObject_ = Find(rootObjectName_);
    if (!rootObject_) return 0;

    pendingObjects_.push_back(rootObject_);
    while (!pendingObjects_.empty())
    {
        const std::shared_ptr<GameObject> object_ = pendingObjects_.back();
        pendingObjects_.pop_back();
        if (!object_) continue;

        colliderCount_ += object_->GetComponents<ColliderComponent>().size();

        if (!includeChildren_ || !object_->transform) continue;
        for (const std::shared_ptr<Transform>& childTransform_ :
            object_->transform->children)
        {
            if (!childTransform_) continue;
            if (const std::shared_ptr<GameObject> childObject_ =
                childTransform_->gameObject.lock())
            {
                pendingObjects_.push_back(childObject_);
            }
        }
    }

    return colliderCount_;
}

std::vector<std::string> GetSceneDirectories()
{
    std::vector<std::string> folders_;
    std::string path_ = "Data/Scenes/";

    if (!std::filesystem::exists(path_)) return folders_;

    for (const auto& entry_ : std::filesystem::directory_iterator(path_)) {
        if (entry_.is_directory()) {
            folders_.push_back(entry_.path().filename().string());
        }
    }
    return folders_;
}

void GameObjectManager::Gui()
{
}

void GameObjectManager::DrawHierarchyGui()
{
    // Scene内Objectの作成、選択、階層表示、Prefab生成を行うHierarchy UI。
    static char createName_[64] = "GameObject";

    ImGui::Text(IM_U8("ゲームオブジェクト作成"));
    ImGui::InputText("##GameObjectManager_CreateNameInput", createName_, IM_ARRAYSIZE(createName_));

    if (ImGui::Button(IM_U8("作成##GameObjectManager_CreateObjectButton")))
    {
        selectedObject = Create(createName_);
    }

    ImGui::Separator();

    if (ImGui::CollapsingHeader(IM_U8("プレハブ##GameObjectManager_PrefabsHeader"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::Button(IM_U8("更新##GameObjectManager_RefreshPrefabListButton")))
        {
            RefreshPrefabList();
        }

        static char newPrefabName_[64] = "";

        if (!prefabFiles.empty())
        {
            const char* currentItem_ = prefabFiles[selectedPrefabIndex].c_str();

            if (ImGui::BeginCombo(IM_U8("プレハブ選択##GameObjectManager_SelectPrefabCombo"), currentItem_))
            {
                for (int index_ = 0; index_ < prefabFiles.size(); index_++)
                {
                    if (ImGui::Selectable(prefabFiles[index_].c_str(), selectedPrefabIndex == index_))
                    {
                        selectedPrefabIndex = index_;
                    }
                }

                ImGui::EndCombo();
            }

            if (ImGui::Button(IM_U8("プレハブから作成##GameObjectManager_InstantiatePrefabButton")))
            {
                selectedObject = Instantiate(prefabFiles[selectedPrefabIndex]);
            }
        }

        if (selectedObject)
        {
            ImGui::InputText(IM_U8("新規プレハブ名##GameObjectManager_NewPrefabNameInput"), newPrefabName_, IM_ARRAYSIZE(newPrefabName_));

            if (ImGui::Button(IM_U8("選択中のオブジェクトをプレハブとして保存##GameObjectManager_SaveSelectedAsPrefabButton")))
            {
                if (strlen(newPrefabName_) > 0)
                {
                    SaveAsPrefab(selectedObject, newPrefabName_);
                    RefreshPrefabList();
                    memset(newPrefabName_, 0, sizeof(newPrefabName_));
                }
            }
        }
    }

    ImGui::Separator();

    GameObject* objectToDelete_ = nullptr;

    // 内部IDなので英語のまま
    ImGui::BeginChild("HierarchyRegion", ImVec2(0, 0), true);

    for (auto& object_ : objects)
    {
        if (!object_ || !object_->saveFlg) continue;
        if (object_->transform->parent.expired())
        {
            DrawHierarchyNode(object_, &objectToDelete_);
        }
    }

    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload_ = ImGui::AcceptDragDropPayload("DND_GAMEOBJECT"))
        {
            std::shared_ptr<GameObject> draggedObject_ = *(std::shared_ptr<GameObject>*)payload_->Data;
            draggedObject_->SetParent(nullptr);
        }

        ImGui::EndDragDropTarget();
    }

    ImGui::EndChild();

    if (objectToDelete_)
    {
        if (selectedObject && selectedObject.get() == objectToDelete_)
        {
            selectedObject = nullptr;
        }

        auto iterator_ = std::find_if(
            objects.begin(),
            objects.end(),
            [&](const std::shared_ptr<GameObject>& object_)
            {
                return object_.get() == objectToDelete_;
            });

        if (iterator_ != objects.end())
        {
            auto targetObject_ = *iterator_;

            for (auto& childTransform_ : targetObject_->transform->children)
            {
                if (auto childObject_ = childTransform_->gameObject.lock())
                {
                    childObject_->SetParent(nullptr);
                }
            }

            if (selectedObject == targetObject_)
            {
                selectedObject = nullptr;
            }

            UnregisterObjectNameIndex(targetObject_.get(), targetObject_->GetName());
            objects.erase(iterator_);
        }
    }
}

void GameObjectManager::DrawInspectorGui()
{
    // 選択中ObjectのComponent GUIとCamera操作を表示するInspector UI。
    if (!selectedObject) return;

    ImGui::SeparatorText(selectedObject->GetName().c_str());
    selectedObject->Gui();

    auto cameraComponent_ = cameraManager->GetActiveCamera();
    if (cameraComponent_) {
        ImGuizmo::BeginFrame();
        DirectX::XMFLOAT4X4 view_ = cameraComponent_->GetView();
        DirectX::XMFLOAT4X4 projection_ = cameraComponent_->GetProjection();
        selectedObject->DrawGizmo(&view_._11, &projection_._11);
    }
}
void GameObjectManager::DrawSceneModals()
{
    // Scene保存/Load用のPopupをまとめて描画する。
    if (sceneState.showSavePopup)
    {
        GetSceneDirectories();

        // Popup IDなので内部IDとして英語のまま維持
        ImGui::OpenPopup("SaveScenePopup");

        sceneState.showSavePopup = false;
    }

    if (ImGui::BeginPopupModal("SaveScenePopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (ImGui::BeginCombo(IM_U8("シーン選択 / 入力##SceneModal_SelectInputSceneCombo"), sceneState.name))
        {
            for (const auto& file_ : sceneState.sceneFiles)
            {
                bool isSelected_ = (strcmp(sceneState.name, file_.c_str()) == 0);

                if (ImGui::Selectable(file_.c_str(), isSelected_))
                {
                    strcpy_s(sceneState.name, file_.c_str());
                }
            }

            ImGui::EndCombo();
        }

        ImGui::InputText(IM_U8("ファイル名##SceneModal_FilenameInput"), sceneState.name, IM_ARRAYSIZE(sceneState.name));

        if (ImGui::Button(IM_U8("保存##SceneModal_SaveSceneButton"), ImVec2(120, 0)))
        {
            SaveScene(sceneState.name);
            GetSceneDirectories();
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button(IM_U8("キャンセル##SceneModal_SaveCancelButton"), ImVec2(120, 0)))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    if (sceneState.showLoadPopup)
    {
        GetSceneDirectories();

        // Popup IDなので内部IDとして英語のまま維持
        ImGui::OpenPopup("LoadScenePopup");

        sceneState.showLoadPopup = false;
    }

    if (ImGui::BeginPopupModal("LoadScenePopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (ImGui::BeginCombo(IM_U8("シーン選択##SceneModal_SelectSceneCombo"), sceneState.name))
        {
            for (const auto& file_ : sceneState.sceneFiles)
            {
                bool isSelected_ = (strcmp(sceneState.name, file_.c_str()) == 0);

                if (ImGui::Selectable(file_.c_str(), isSelected_))
                {
                    strcpy_s(sceneState.name, file_.c_str());
                }
            }

            ImGui::EndCombo();
        }

        if (ImGui::Button(IM_U8("読み込み##SceneModal_LoadSceneButton"), ImVec2(120, 0)))
        {
            if (strlen(sceneState.name) > 0)
            {
                LoadScene(sceneState.name);
            }

            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button(IM_U8("キャンセル##SceneModal_LoadCancelButton"), ImVec2(120, 0)))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}
void GameObjectManager::DrawHierarchyNode(std::shared_ptr<GameObject> object_, GameObject** objectToDelete_)
{
    ImGuiTreeNodeFlags flags_ =
        ImGuiTreeNodeFlags_OpenOnArrow |
        ImGuiTreeNodeFlags_OpenOnDoubleClick |
        ImGuiTreeNodeFlags_SpanAvailWidth;

    if (object_ == selectedObject)
    {
        flags_ |= ImGuiTreeNodeFlags_Selected;
    }

    if (object_->transform->children.empty())
    {
        flags_ |= ImGuiTreeNodeFlags_Leaf;
    }

    bool opened_ = ImGui::TreeNodeEx(
        (void*)(uintptr_t)object_.get(),
        flags_,
        "%s",
        object_->GetName().c_str());

    static std::weak_ptr<GameObject> pressedObject_;
    static ImVec2 pressedMousePos_ = ImVec2(0.0f, 0.0f);
    static double pressedTime_ = 0.0;

    if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
    {
        pressedObject_ = object_;
        pressedMousePos_ = ImGui::GetIO().MousePos;
        pressedTime_ = ImGui::GetTime();
    }

    if (ImGui::BeginDragDropSource())
    {
        ImGui::SetDragDropPayload("DND_GAMEOBJECT", &object_, sizeof(std::shared_ptr<GameObject>));

        ImGui::Text(IM_U8("移動中: %s"), object_->GetName().c_str());

        ImGui::EndDragDropSource();
    }

    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload_ = ImGui::AcceptDragDropPayload("DND_GAMEOBJECT"))
        {
            std::shared_ptr<GameObject> draggedObject_ = *(std::shared_ptr<GameObject>*)payload_->Data;

            if (draggedObject_ != object_)
            {
                draggedObject_->SetParent(object_);
            }
        }

        ImGui::EndDragDropTarget();
    }

    if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        auto pressedObjectLock_ = pressedObject_.lock();

        ImVec2 releaseMousePos_ = ImGui::GetIO().MousePos;
        float deltaX_ = releaseMousePos_.x - pressedMousePos_.x;
        float deltaY_ = releaseMousePos_.y - pressedMousePos_.y;

        float dragThreshold_ = 6.0f;

        bool isSameObject_ = pressedObjectLock_ == object_;
        bool isShortPress_ = (ImGui::GetTime() - pressedTime_) < 0.25;
        bool isNotDragged_ = (deltaX_ * deltaX_ + deltaY_ * deltaY_) < (dragThreshold_ * dragThreshold_);

        if (isSameObject_ && isShortPress_ && isNotDragged_)
        {
            selectedObject = object_;
        }

        pressedObject_.reset();
    }

    if (ImGui::BeginPopupContextItem())
    {
        selectedObject = object_;

        if (ImGui::MenuItem(IM_U8("ゲームオブジェクトを削除##GameObjectManager_DeleteGameObjectMenuItem")))
        {
            Destroy(object_->GetName());
        }

        ImGui::EndPopup();
    }

    if (opened_)
    {
        for (auto& childTransform_ : object_->transform->children)
        {
            if (auto childObject_ = childTransform_->gameObject.lock())
            {
                if (!childObject_->saveFlg) continue;
                DrawHierarchyNode(childObject_, objectToDelete_);
            }
        }

        ImGui::TreePop();
    }
}

void GameObjectManager::RenderAll(
    SpriteRenderSystem& spriteSystem_,
    ModelRenderSystem& modelSystem_,
    ID3D11RenderTargetView* finalRenderTargetView_,
    const CameraComponent* overrideCamera_,
    bool clearSceneRenderTargets_)
{
    // 3D、VFX、Particle、PostEffect、Spriteの順にScene全体を描画する。
    BS_PROFILE_SCOPE("Render::All");
    auto deviceContext_ = GraphicsDeviceManager::Instance().GetContext();
    auto& graphicsDeviceManager_ = GraphicsDeviceManager::Instance();
    auto tracyGpuContext_ = graphicsDeviceManager_.GetTracyGpuContext();

    if (clearSceneRenderTargets_)
    {
        const float clearColor_[4] = { 0.5f, 0.5f, 0.5f, 1.0f };
        // 別視点のEditor SceneViewは通常描画後に再度Scene中間バッファを使うため、ここで描画結果を分離する。
        graphicsDeviceManager_.ClearSceneRenderTargets(clearColor_);
    }

    // --- 追加: ライト情報の収集 ---
    LightManager lightManager_;
    // デフォルトの平行光源設定（必要に応じて）
    DirectionalLight directionalLight_;
    directionalLight_.direction = {1, 1, 1 };
    lightManager_.SetDirectionalLight(directionalLight_);

    DecalManager decalManager_; // デカール用マネージャー

    for (auto& object_ : objects)
    {
        auto lightComponent_ = object_->GetComponent<LightComponent>();
        if (lightComponent_ && lightComponent_->IsEnabled())
        {
            // A. 平行光源
            if (lightComponent_->type == LightComponent::Type::Directional)
            {
                DirectionalLight directionalLight_;
                directionalLight_.direction = lightComponent_->finalWorldDirection;
                directionalLight_.color = lightComponent_->color;
                directionalLight_.intensity = lightComponent_->intensity;
                lightManager_.AddDirectionalLight(directionalLight_);
            }
            // B. 点光源
            else if (lightComponent_->type == LightComponent::Type::Point)
            {
                PointLight pointLight_;
                pointLight_.position = lightComponent_->finalWorldPosition;
                pointLight_.color = lightComponent_->color;
                pointLight_.intensity = lightComponent_->intensity;
                pointLight_.range = lightComponent_->range;
                lightManager_.AddPointLight(pointLight_);
            }
            // C. スポットライト
            else if (lightComponent_->type == LightComponent::Type::Spot)
            {
                SpotLight spotLight_;
                spotLight_.position = lightComponent_->finalWorldPosition;
                spotLight_.direction = lightComponent_->finalWorldDirection;
                spotLight_.color = lightComponent_->color;
                spotLight_.intensity = lightComponent_->intensity;
                spotLight_.range = lightComponent_->range;
                spotLight_.innerAngle = lightComponent_->innerAngle;
                spotLight_.outerAngle = lightComponent_->outerAngle;
                lightManager_.AddSpotLight(spotLight_);
            }
        }

        auto decalComponent_ = object_->GetComponent<DecalComponent>();

        // デカールの収集
        if (decalComponent_ && decalComponent_->IsEnabled())
        {
            decalComponent_->SubmitDecal(decalManager_);
        }


    }


    for (auto& object_ : objects)
    {
        auto modelComponent_ = object_->GetComponent<GltfModelCommponennt>();
        if (modelComponent_)
            modelSystem_.PushSubmit(modelComponent_->GetShaderID(), modelComponent_);
    }
    RenderContext renderContext_;

    renderContext_.device = GraphicsDeviceManager::Instance().GetDevice();
    renderContext_.deviceContext = deviceContext_;
    renderContext_.camera = overrideCamera_ ? overrideCamera_ : cameraManager->GetActiveCamera().get();
    renderContext_.renderState = GraphicsDeviceManager::Instance().GetRenderState();
    renderContext_.lights = &lightManager_;
    renderContext_.decal = &decalManager_;
    renderContext_.baseColorSRV = GraphicsDeviceManager::Instance().GetSceneSRV();
    renderContext_.depthSRV = GraphicsDeviceManager::Instance().GetMainDepthSRV();
    renderContext_.shapeSystem = &GraphicsDeviceManager::Instance().GetShapeRendererSystem();
    renderContext_.cascadeShadowMap = &GraphicsDeviceManager::Instance().GetCascadeShadowMap();

    // ここでレンダー状態をリセット
    deviceContext_->OMSetBlendState(renderContext_.renderState->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);
    deviceContext_->OMSetDepthStencilState(renderContext_.renderState->GetDepthStencilState(DepthState::TestAndWrite), 0);
    deviceContext_->RSSetState(renderContext_.renderState->GetRasterizerState(RasterizerState::SolidCullBack));
    ID3D11SamplerState* samplers_[] = { renderContext_.renderState->GetSamplerState(SamplerState::LinearWrap) };
    deviceContext_->PSSetSamplers(0, 1, samplers_);

    // 1. シーンバッファ（HDR）へ描画開始
    {
        BS_PROFILE_GPU_SCOPE(tracyGpuContext_, "GPU::Main Model Pass");
        graphicsDeviceManager_.SetSceneRenderTarget();
        modelSystem_.RenderAll(renderContext_);
    }


    if(shapeRenderEnabled)
    {
        BS_PROFILE_GPU_SCOPE(tracyGpuContext_, "GPU::Shape Render");
        GraphicsDeviceManager::Instance().GetShapeRendererSystem().Render(deviceContext_, renderContext_.camera->GetView(), renderContext_.camera->GetProjection());
        deviceContext_->OMSetDepthStencilState(
            renderContext_.renderState->GetDepthStencilState(DepthState::TestAndWrite), 0);
    }

    graphicsDeviceManager_.SetParticleRenderTarget();

    for (auto& obj : objects)
    {
        if (!obj) continue;
        for (const auto& meshVFX_ : obj->GetComponents<MeshVFXComponent>())
        {
            if (meshVFX_ && meshVFX_->IsEnabled())
            {
                meshVFXRenderSystem->PushSubmit(meshVFX_.get());
            }
        }
    }
    {
        BS_PROFILE_GPU_SCOPE(tracyGpuContext_, "GPU::MeshVFX");
        meshVFXRenderSystem->RenderAll(renderContext_);
    }

    // MeshVFX can switch between temporary and distortion render targets.
    // Rebind the scene color target so particles are included in the post-process input.
    graphicsDeviceManager_.SetParticleRenderTarget();

    for (auto& obj : objects)
    {
        if (!obj) continue;

        auto particle = obj->GetComponent<ParticleComponent>();
        if (particle && particle->IsEnabled())
        {
            particleRenderSystem->PushSubmit(particle.get());
        }
    }

    {
        BS_PROFILE_GPU_SCOPE(tracyGpuContext_, "GPU::Particle");
        particleRenderSystem->RenderAll(renderContext_);
    }

    ID3D11RenderTargetView* finalRTV_ = finalRenderTargetView_ ? finalRenderTargetView_ : graphicsDeviceManager_.GetRenderTargetView();

    // 2. ポストレンダーシステムを実行
    {
        BS_PROFILE_GPU_SCOPE(tracyGpuContext_, "GPU::PostEffect");
        graphicsDeviceManager_.GetPostRenderSystem().Execute(
            renderContext_,
            graphicsDeviceManager_.GetSceneSRV(), // 描画結果のSRV
            finalRTV_           // 最終出力先
        );
    }


    // SpriteRenderSubmit
    for (auto& object_ : objects)
    {
        auto sr = object_->GetComponent<SpriteRenderer>();
        if (sr && sr->enabled)
        {
            SpriteRenderSystem::SpriteRenderCommand command_;
            command_.sprite = sr;
            command_.shaderID = sr->GetSpriteShaderID();

            auto widget_ = object_->GetComponent<UIWidgetComponent>();
            auto vertexModifier_ = object_->GetComponent<UIVertexModifierComponent>();
            auto board_ = ResolveWidgetBoard(object_);

            command_.widget = widget_.get();
            command_.vertexModifier = vertexModifier_.get();
            command_.widgetBoard = board_.get();
            command_.boardLayer = board_ ? board_->GetBoardLayer() : 0;
            command_.boardZOrder = board_ ? board_->GetZOrder() : 0;
            command_.widgetZOrder = widget_ ? widget_->GetZOrder() : 0;
            command_.hierarchyOrder = CalculateHierarchyDepth(object_);
            spriteSystem_.Submit(command_);
        }
    }

    const BlendState spriteBlendState_ = finalRenderTargetView_
        ? BlendState::TransparencyKeepAlpha
        : BlendState::Transparency;
    // Spriteの透明部分がSceneView用RTのAlphaを消さないよう、描画先に応じて合成状態を切り替える。
    deviceContext_->OMSetBlendState(renderContext_.renderState->GetBlendState(spriteBlendState_), nullptr, 0xFFFFFFFF);
    deviceContext_->OMSetDepthStencilState(renderContext_.renderState->GetDepthStencilState(DepthState::NoTestNoWrite), 0);
    deviceContext_->RSSetState(renderContext_.renderState->GetRasterizerState(RasterizerState::SolidCullNone));

    {
        BS_PROFILE_GPU_SCOPE(tracyGpuContext_, "GPU::Sprite");
        spriteSystem_.RenderAll(deviceContext_);
    }

    // Sprite描画で変更した2D用ステートを戻し、後続のImGui/Viewport描画へ状態を残さない。
    deviceContext_->OMSetBlendState(renderContext_.renderState->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);
    deviceContext_->OMSetDepthStencilState(renderContext_.renderState->GetDepthStencilState(DepthState::TestAndWrite), 0);
    deviceContext_->RSSetState(renderContext_.renderState->GetRasterizerState(RasterizerState::SolidCullBack));
}

void GameObjectManager::RegisterMotionVelocityComponent(
    const std::shared_ptr<MotionVelocityComponent>& component_)
{
    if (!motionVelocityRenderSystem || !component_) return;
    motionVelocityRenderSystem->Register(component_);
}

void GameObjectManager::UnregisterMotionVelocityComponent(MotionVelocityComponent* component_)
{
    if (!motionVelocityRenderSystem || component_ == nullptr) return;
    motionVelocityRenderSystem->Unregister(component_);
}



void GameObjectManager::Destroy(std::shared_ptr<GameObject> object_)
{
    if (!object_) return;
    // Update中の配列破壊を避けるため、即座に消さず予約リストに入れる。
    pendingDestroys.push_back(object_);
}

void GameObjectManager::Destroy(std::string objectName_)
{
    // 名前からObjectを探し、見つかった最初の1つを削除予約する。
    for (auto& object_ : objects)
    {
        if (object_ && object_->GetName() == objectName_)
        {
            Destroy(object_); // 上のDestroyを呼ぶ
            return; // 1つ見つけたら終了（全部消すならreturnしない）
        }
    }
}


// ヘルパー関数：親→子の順番で名前をリストに追加する
void CollectObjectNames(const std::shared_ptr<GameObject>& obj, nlohmann::json& listJson) {
    if (!obj || !obj->saveFlg) return;

    // 自分の名前を登録
    listJson["ObjectList"].push_back(obj->nameComponent->name);

    // 子供たちの名前を再帰的に登録
    for (auto& child : obj->transform->children) {
        CollectObjectNames(child ? child->gameObject.lock() : nullptr, listJson);
    }
}

void CollectPrefabObjects(const std::shared_ptr<GameObject>& object_, std::vector<std::shared_ptr<GameObject>>& objects_)
{
    // Prefab保存対象のRootと子階層を、復元しやすい親先行の順番で集める。
    if (!object_ || !object_->saveFlg) return;

    objects_.push_back(object_);
    for (const std::shared_ptr<Transform>& childTransform_ : object_->transform->children)
    {
        if (!childTransform_) continue;
        CollectPrefabObjects(childTransform_->gameObject.lock(), objects_);
    }
}

void GameObjectManager::SaveScene(const std::string& sceneName_)
{
    // SceneListとObject個別JSONに分けて保存し、親子順はSceneList側へ記録する。
    std::string basePath = "Data/Scenes/" + sceneName_ + "/";
    std::string objectsPath = basePath + "Objects/";
    std::filesystem::create_directories(objectsPath);

    nlohmann::json listJson;
    listJson["SceneName"] = sceneName_;
    listJson["ObjectList"] = nlohmann::json::array(); // 配列を初期化

    // --- グラフィックス設定 ---
    auto& params = GraphicsDeviceManager::Instance().GetBloomParams();
    listJson["GraphicsSettings"]["Bloom"] = {
        {"enabled", params.enabled},
        {"threshold", params.threshold},
        {"intensity", params.intensity},
        {"kawaseScale", params.kawaseScale},
        {"blurPasses", params.blurPasses},
        {"toneMappingEnabled", params.toneMappingEnabled},
        {"exposure", params.exposure}
    };
    SaveCurrentIBLSettings(listJson["GraphicsSettings"]);

    if (modelRenderSystem)
    {
        modelRenderSystem->SerializeGraphicsSettings(listJson["GraphicsSettings"]);
    }

    // 1. 各オブジェクトの個別ファイルを保存
    // (ここは順不同でOKなので、まずは全ファイルを書き出す)
    for (auto& obj : objects) {
        if (!obj->saveFlg) continue;

        std::string filePath = objectsPath + obj->nameComponent->name + ".json";
        nlohmann::json objJson = obj->Save();
        std::ofstream file(filePath);
        if (file.is_open()) {
            file << objJson.dump(4);
        }
    }

    // 2. リスト JSON に「親 → 子」の順で名前を記録
    // 親がいない（ルート）オブジェクトを探して、そこから再帰を開始する
    for (auto& obj : objects) {
        if (obj->saveFlg && obj->transform->parent.lock() == nullptr) {
            CollectObjectNames(obj, listJson);
        }
    }

    // 3. 管理用リストファイルを保存
    std::ofstream listFile(basePath + "SceneList.json");
    if (listFile.is_open()) {
        listFile << listJson.dump(4);
    }

    printf("Scene '%s' saved with ordered SceneList.json\n", sceneName_.c_str());
}

void GameObjectManager::LoadScene(const std::string& sceneName_)
{
    BS_PROFILE_SCOPE("GameObjectManager::LoadScene");
    // SceneListに記録された順番でObject JSONを読み込み、最後に親子関係を復元する。
    const double loadSceneStartTimeMs_ = GetSceneProfileMilliseconds();
    BS_PROFILE_PLOT("ModelLoad.LoadSceneStartTime", loadSceneStartTimeMs_);

    Clear(); // 現在のシーンをクリア
    std::string basePath = "Data/Scenes/" + sceneName_ + "/";
    std::string listFilePath = basePath + "SceneList.json";
    sceneRuntimeState = SCENE_RUNTIME_STATE::LOADING;
    sceneLoadProgress = 0.0f;
    sceneRuntimeError.clear();

    std::ifstream listFile(listFilePath);
    if (!listFile.is_open()) {
        printf("Error: SceneList.json not found!\n");
        FailSceneRuntime("SceneList.json not found: " + listFilePath);
        return;
    }
    nlohmann::json listJson;
    {
        BS_PROFILE_SCOPE("LoadScene::ReadSceneListJson");
        listFile >> listJson;
    }

    ActionRegistry::Instance().Clear();
    RegisterAllActions();
    if (gameModeManager)
    {
        gameModeManager->ResetBattleState();
        gameModeManager->RegisterGlobalActions();
    }

    EventManager::Clear();


    // --- グラフィックス設定の読み込み ---
    if (listJson.contains("GraphicsSettings")) {
        auto& graphicsJson = listJson["GraphicsSettings"];

        if (graphicsJson.contains("Bloom")) {
            auto& bloomJson = graphicsJson["Bloom"];
            auto& params = GraphicsDeviceManager::Instance().GetBloomParams();

            params.enabled = bloomJson.value("enabled", true);
            params.threshold = bloomJson.value("threshold", 1.0f);
            params.intensity = bloomJson.value("intensity", 1.0f);
            params.kawaseScale = bloomJson.value("kawaseScale", 1.0f);
            params.blurPasses = bloomJson.value("blurPasses", 4);
            params.toneMappingEnabled = bloomJson.value("toneMappingEnabled", true);
            params.exposure = bloomJson.value("exposure", 1.0f);
        }

        LoadSceneIBLSettings(graphicsJson);

        if (modelRenderSystem)
        {
            modelRenderSystem->DeserializeGraphicsSettings(graphicsJson);
        }
    }



    {
        BS_PROFILE_SCOPE("LoadScene::BuildModelLoadRequestList");
        const double enqueueLoadGroupStartTimeMs_ = GetSceneProfileMilliseconds();
        BS_PROFILE_PLOT("ModelLoad.EnqueueLoadGroupStartTime", enqueueLoadGroupStartTimeMs_);
        BS_PROFILE_PLOT("ModelLoad.LoadSceneToEnqueueGapMs", enqueueLoadGroupStartTimeMs_ - loadSceneStartTimeMs_);

        {
            BS_PROFILE_SCOPE("LoadScene::EnqueueLoadGroup");
            // SceneListに記載されている名前の順にロード
            std::unordered_set<std::string> loadedObjectNames_;
            for (const auto& name : listJson["ObjectList"]) {
                BS_PROFILE_SCOPE("LoadScene::LoadObject");
                std::string objName = name.get<std::string>();
                if (!loadedObjectNames_.insert(objName).second)
                {
                    BS_LOG_WARNING(LOG_CATEGORY::CORE, "LoadScene skipped duplicate ObjectList entry: " + objName);
                    continue;
                }
                std::string filePath = basePath + "Objects/" + objName + ".json";

                std::ifstream file(filePath);
                if (file.is_open()) {
                    nlohmann::json objJson;
                    {
                        BS_PROFILE_SCOPE("LoadScene::ReadObjectJson");
                        file >> objJson;
                    }

                    // オブジェクト生成
                    auto obj = Create(objName);

                    obj->SetManager(this);

                    // オブジェクト自身にコンポーネントを含めて復元させる
                    {
                        BS_PROFILE_SCOPE("LoadScene::GameObjectLoad");
                        obj->Load(objJson);
                    }
                }
            }
        }

        BS_PROFILE_PLOT("ModelLoad.EnqueueLoadGroupEndTime", GetSceneProfileMilliseconds());
    }

    // ここで親子関係を復元する
    {
        BS_PROFILE_SCOPE("LoadScene::ResolveHierarchy");
        ResolveHierarchy();
    }
    //{
    //    BS_PROFILE_SCOPE("LoadScene::EnsureGlobalActionObjects");
    //    EnsureGlobalActionObjects();
    //}

    printf("Scene '%s' loaded from SceneList.json\n", sceneName_.c_str());
}

void GameObjectManager::ResolveHierarchy()
{
    // Load中に文字列として保持した親Object名を、実際のTransform親子関係へ戻す。
    for (auto& obj : objects)
    {
        if (!obj) continue;

        // ロード時に一時保存した「親の名前」を取得
        std::string pName = obj->transform->parentNameDuringLoad;

        if (!pName.empty())
        {
            // 名前でマネージャー内のオブジェクトを検索
            auto parentObj = Find(pName);
            if (parentObj && parentObj != obj)
            {
                // 親が見つかったら、Transformの親子付けを実行
                obj->SetParent(parentObj);
            }
        }

        // ロード用の一時情報を残すと、後でResolveHierarchyを呼んだ時に古い親へ戻ってしまう
        obj->transform->parentNameDuringLoad.clear();
    }

    // 全体の親子が確定したので、一度行列を正しく計算させる
    for (auto& obj : objects)
    {
        if (obj) obj->UpdateTransform();
    }
}

std::vector<std::string> GameObjectManager::GetSceneDirectories()
{
    sceneState.sceneFiles.clear();
    std::string rootPath = "Data/Scenes";

    if (std::filesystem::exists(rootPath)) {
        for (const auto& entry : std::filesystem::directory_iterator(rootPath)) {
            if (entry.is_directory()) {
                std::string sceneName = entry.path().filename().string();

                // シーンフォルダの中に「SceneList.json」が存在するかを確認する
                std::filesystem::path jsonPath = entry.path() / "SceneList.json";

                if (std::filesystem::exists(jsonPath)) {
                    sceneState.sceneFiles.push_back(sceneName);
                }
            }
        }
    }
    return sceneState.sceneFiles;
}


// フォルダ内のJSONファイルをスキャンする
void GameObjectManager::RefreshPrefabList()
{
    prefabFiles.clear();
    if (!std::filesystem::exists(prefabDirectory)) {
        std::filesystem::create_directories(prefabDirectory);
    }

    for (const auto& entry : std::filesystem::directory_iterator(prefabDirectory)) {
        if (entry.path().extension() == ".json") {
            // ファイル名（拡張子なし）をリストに追加
            prefabFiles.push_back(entry.path().stem().string());
        }
    }

    if (prefabFiles.empty())
    {
        selectedPrefabIndex = 0;
    }
    else
    {
        selectedPrefabIndex = std::clamp(
            selectedPrefabIndex,
            0,
            static_cast<int>(prefabFiles.size()) - 1);
    }
}

void GameObjectManager::SetSelectedPrefabIndex(int selectedPrefabIndex_)
{
    if (prefabFiles.empty())
    {
        selectedPrefabIndex = 0;
        return;
    }

    selectedPrefabIndex = std::clamp(
        selectedPrefabIndex_,
        0,
        static_cast<int>(prefabFiles.size()) - 1);
}

void GameObjectManager::ApplyDestroys(std::shared_ptr<GameObject> object_)
{
    // Collider登録を外し、子Objectを再帰的に削除してからManagerのリストから外す。
    if (!object_) return;
    for (const std::shared_ptr<ColliderComponent>& collider_ :
        object_->GetComponents<ColliderComponent>())
    {
        collisionStepSystem->UnregisterCollider(collider_.get());
    }

    // 1. 子供を先に全て削除する（再帰）
    auto childrenCopy = object_->transform->children;
    for (auto& childTrans : childrenCopy)
    {
        if (auto childGO = childTrans->gameObject.lock())
        {
            ApplyDestroys(Find(childTrans->gameObject.lock()->GetName()));
        }
    }

    // 2. 特殊な参照を外す
    if (selectedObject == object_) selectedObject = nullptr;

    // 3. マネージャーのメインリストから自分を削除
    auto it = std::remove(objects.begin(), objects.end(), object_);
    if (it != objects.end()) {
        UnregisterObjectNameIndex(object_.get(), object_->GetName());
        objects.erase(it, objects.end());
    }
}

// プレハブ保存
void GameObjectManager::SaveAsPrefab(std::shared_ptr<GameObject> object_, const std::string& prefabName_)
{
    // 指定Object単体をPrefab JSONとして保存する。
    if (!object_) return;

    std::filesystem::create_directories(prefabDirectory);
    std::string filePath = prefabDirectory + prefabName_ + ".json";

    std::vector<std::shared_ptr<GameObject>> prefabObjects_;
    CollectPrefabObjects(object_, prefabObjects_);
    if (prefabObjects_.empty()) return;

    // Rootと子階層を1つのPrefab Assetとして保存する。
    nlohmann::json j;
    j["AssetType"] = "Prefab";
    j["Version"] = 2;
    j["RootName"] = object_->GetName();
    j["ObjectList"] = nlohmann::json::array();
    j["Objects"] = nlohmann::json::object();

    for (const std::shared_ptr<GameObject>& prefabObject_ : prefabObjects_)
    {
        if (!prefabObject_) continue;

        const std::string& objectName_ = prefabObject_->GetName();
        nlohmann::json objectJson_ = prefabObject_->Save();
        if (prefabObject_ == object_)
        {
            objectJson_["ParentName"] = "";
        }

        j["ObjectList"].push_back(objectName_);
        j["Objects"][objectName_] = objectJson_;
    }
    std::ofstream file(filePath);
    if (file.is_open()) {
        file << j.dump(4);
        RefreshPrefabList(); // リストを更新
    }
}


void GameObjectManager::InitializeAllComponents()
{
    //// ★ 現在存在しているオブジェクトをスナップショット
    //auto objectsSnapshot = objects;

    for (auto& obj : objects)
    {
        if (!obj) continue;

        // ActionSequence 以外のコンポーネントの Start を呼ぶ
        // (あるいは、ActionSequence を持っていないオブジェクトを先に処理)
        if (!obj->GetComponent<ActionSequence>()) {
            obj->CallAllComponentStart();
        }
    }

    // 2. 次に「アクションを利用する側」の Start を呼ぶ
    // ここで ActionSequence が RegisterSequence を実行する
    for (auto& obj : objects)
    {
        if (!obj) continue;

        if (auto seq = obj->GetComponent<ActionSequence>()) {
            obj->CallAllComponentStart();
        }
    }
}

CameraManager* GameObjectManager::GetCameraManager(){ return cameraManager.get(); }

// プレハブから生成
std::shared_ptr<GameObject> GameObjectManager::Instantiate(const std::string& prefabName_)
{
    // Prefab JSONを読み込み、Scene内で重複しない名前を付けてObject化する。
    std::string filePath = prefabDirectory + prefabName_ + ".json";
    std::ifstream file(filePath);
    if (!file.is_open()) {
        printf("Error: Prefab '%s' not found!\n", prefabName_.c_str());
        return nullptr;
    }

    nlohmann::json prefabJson;
    file >> prefabJson;

    if (prefabJson.contains("Objects"))
    {
        const std::string rootName_ = prefabJson.value("RootName", prefabName_);
        std::unordered_map<std::string, std::string> nameMap_;
        std::unordered_set<std::string> reservedNames_;
        std::vector<std::string> objectNames_;

        for (const auto& nameJson_ : prefabJson["ObjectList"])
        {
            objectNames_.push_back(nameJson_.get<std::string>());
        }

        for (const std::string& objectName_ : objectNames_)
        {
            const std::string baseName_ = (objectName_ == rootName_) ? prefabName_ : objectName_;
            std::string uniqueName_ = MakeUniqueName(baseName_);
            int suffixIndex_ = 1;
            while (reservedNames_.contains(uniqueName_))
            {
                char buffer_[32];
                sprintf_s(buffer_, sizeof(buffer_), "_%02d", suffixIndex_);
                uniqueName_ = baseName_ + buffer_;
                ++suffixIndex_;
            }

            reservedNames_.insert(uniqueName_);
            nameMap_[objectName_] = uniqueName_;
        }

        std::shared_ptr<GameObject> rootObject_ = nullptr;
        for (const std::string& objectName_ : objectNames_)
        {
            if (!prefabJson["Objects"].contains(objectName_)) continue;

            nlohmann::json objectJson_ = prefabJson["Objects"][objectName_];
            objectJson_["Name"] = nameMap_[objectName_];

            const std::string parentName_ = objectJson_.value("ParentName", "");
            if (!parentName_.empty())
            {
                const auto parentNameIterator_ = nameMap_.find(parentName_);
                objectJson_["ParentName"] =
                    parentNameIterator_ != nameMap_.end() ? parentNameIterator_->second : "";
            }

            std::shared_ptr<GameObject> newObject_ = Create(nameMap_[objectName_]);
            newObject_->Load(objectJson_);
            if (objectName_ == rootName_) rootObject_ = newObject_;
        }

        if (!rootObject_ && !objectNames_.empty())
        {
            rootObject_ = Find(nameMap_[objectNames_.front()]);
        }

        ResolveHierarchy();
        selectedObject = rootObject_;

        printf("Instantiated Prefab Hierarchy: %s\n", prefabName_.c_str());
        return rootObject_;
    }

    // 1. 重複しないユニークな名前を生成する
    std::string uniqueName = MakeUniqueName(prefabName_);

    // 2. その名前でオブジェクトを作成
    // ※Create内部でも重複チェックがあるかもしれませんが、
    //   ここで確定させた名前を渡すのが一番確実です。
    auto newObj = Create(uniqueName);

    // 3. Jsonからデータをロード
    newObj->Load(prefabJson);

    // 4. LoadするとJson内の"Name"で上書きされてしまうため、
    //    確定したユニーク名に再度設定し直す
    newObj->SetName(uniqueName.c_str());
    printf("Instantiated: %s\n", uniqueName.c_str());
    return newObj;
}

void GameObjectManager::RegisterAttackCallbacks()
{
    // AttackSystemのHit結果を、HitStop、Effect、SE再生へ橋渡しする。
    attackSystem->AddHitCallback(
        [this](const AttackHitEvent& event_)
        {
            if (timeManager && event_.hitStopDuration > 0.0f)
            {
                timeManager->TriggerHitStop(
                    static_cast<uint32_t>(TarGetLayerType::All),
                    event_.hitStopDuration,
                    event_.hitStopScale);
                PhysXSystem::SetTimeScale(event_.hitStopScale);
            }

            if (effectManager && event_.hasHitContact)
            {
                effectManager->SpawnForHit(event_);
            }

            if (!event_.hitSoundPath.empty())
            {
                AudioManager& audioManager_ = AudioManager::Instance();
                audioManager_.AddAudioFromFile(event_.hitSoundPath, false);
                audioManager_.SetVolumeByPath(event_.hitSoundPath, event_.hitSoundVolume);
                audioManager_.SetMuffledByPath(
                    event_.hitSoundPath,
                    event_.hitSoundMuffled,
                    event_.hitSoundMuffledCutoff);
                audioManager_.PlaySEByPath(event_.hitSoundPath);
            }
        });
}

void GameObjectManager::EnsureGlobalActionObjects()
{
    // JustDodgeなどScene全体で使うActionSequence用の常駐Objectを保証する。
    constexpr const char* JUST_DODGE_EVENT_POOL_NAME_ = "JustDodgeEventPool";
    std::shared_ptr<GameObject> eventPool_ = Find(JUST_DODGE_EVENT_POOL_NAME_);
    if (!eventPool_)
    {
        eventPool_ = Create(JUST_DODGE_EVENT_POOL_NAME_);
    }
    if (!eventPool_) return;

    eventPool_->GetOrAddComponent<ColorFilterDirector>(false);
    eventPool_->GetOrAddComponent<MotionBlurDirector>(false);
    eventPool_->GetOrAddComponent<RadialBlurDirector>(false);
    eventPool_->GetOrAddComponent<VignetteDirector>(false);
    eventPool_->GetOrAddComponent<TimeScaleDirector>(false);
    const std::shared_ptr<ActionSequence> sequence_ =
        eventPool_->GetOrAddComponent<ActionSequence>(false);
    if (sequence_) sequence_->SetRegisterGlobal(true);
}

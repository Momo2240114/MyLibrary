#include "PrecompiledHeader.h"
#include "ActorPresentationActionComponent.h"
#include "Engine/Debug/TracyProfiler.h"
#include "Engine/Common/DebugLog.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/Graphics/Resource/Animation/AnimationControllerComponent.h"
#include "Engine/Graphics/VFX/MeshVFXComponent.h"
#include "Engine/Graphics/Particle/ParticleComponent.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <imgui.h>
#include <imgui_stdlib.h>

void ActorPresentationActionComponent::Start()
{
    RegisterActions();
}

void ActorPresentationActionComponent::RegisterActions()
{
    const auto owner_ = GetGameObject();
    if (!owner_) return;
    const std::weak_ptr<GameObject> weakOwner_ = owner_;
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

void ActorPresentationActionComponent::ExecutePreset(int index_)
{
    BS_PROFILE_SCOPE("ActorPresentationActionComponent::ExecutePreset");
    if (index_ < 0 || index_ >= static_cast<int>(presets.size())) return;
    const auto owner_ = GetGameObject();
    if (!owner_) return;
    const auto& preset_ = presets[index_];
    const auto actor_ = owner_->GetManager().Find(preset_.actorName);
    if (!actor_) return;
    if (preset_.type == ActorPresentationActionType::PLAY_MESH_VFX ||
        preset_.type == ActorPresentationActionType::STOP_MESH_VFX)
    {
        const auto meshVfx_ = actor_->GetComponent<MeshVFXComponent>();
        if (!meshVfx_) return;
        const bool play_ = preset_.type == ActorPresentationActionType::PLAY_MESH_VFX;
        // Stopだけでは時刻が止まってMeshが残るため、描画フラグも切り替える。
        for (auto& part_ : meshVfx_->GetAsset().parts) part_.renderEnabled = play_;
        if (play_)
        {
            meshVfx_->Reset();
            meshVfx_->Play();
        }
        else meshVfx_->Stop();
        return;
    }
    if (preset_.type == ActorPresentationActionType::PLAY_PARTICLE ||
        preset_.type == ActorPresentationActionType::STOP_PARTICLE)
    {
        const auto particle_ = actor_->GetComponent<ParticleComponent>();
        if (!particle_) return;
        // 停止時は新規発生だけを止め、発生済みの粒子は寿命まで自然に消す。
        if (preset_.type == ActorPresentationActionType::PLAY_PARTICLE) particle_->Play();
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
        return;
    }
    const auto animation_ = actor_->GetComponent<AnimationControllerComponent>();
    if (!animation_) return;
    if (!preset_.stateAssetPath.empty() &&
        animation_->GetStates().find(preset_.stateName) == animation_->GetStates().end())
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
    animation_->SetPresentationControlled(true);
    if (preset_.stateName.empty() || animation_->GetStates().find(preset_.stateName) == animation_->GetStates().end())
    {
        BS_LOG_INFO(LOG_CATEGORY::GAME, "Presentation animation is not configured: " + preset_.actionName);
        return;
    }
    animation_->ChangeState(preset_.stateName, (std::max)(preset_.duration, 0.0f));
}

void ActorPresentationActionComponent::Update(float deltaTime_)
{
    BS_PROFILE_SCOPE("ActorPresentationActionComponent::Update");
    UpdateMovement(deltaTime_);
    const auto actor_ = turningActor.lock();
    if (!actor_ || !actor_->transform) return;
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
    if (!actor_ || !actor_->transform)
    {
        movingActor.reset();
        return;
    }
    moveElapsed += (std::max)(deltaTime_, 0.0f);
    const float rate_ = moveDuration <= 0.0f ? 1.0f : std::clamp(moveElapsed / moveDuration, 0.0f, 1.0f);
    // 歩行Animationの一定の歩幅に合わせ、登場移動を等速にして足滑りを抑える。
    actor_->transform->localPosition =
    {
        startMovePosition.x + (targetMovePosition.x - startMovePosition.x) * rate_,
        startMovePosition.y + (targetMovePosition.y - startMovePosition.y) * rate_,
        startMovePosition.z + (targetMovePosition.z - startMovePosition.z) * rate_
    };
    // ワールド座標キャッシュを更新し、Colliderの外部Transform変更検出と描画へ反映する。
    actor_->transform->MarkDirty();
    actor_->transform->UpdateWorldMatrix();
    if (rate_ >= 1.0f) movingActor.reset();
}

void ActorPresentationActionComponent::Gui()
{
    const char* TYPES[] = { "Play Animation", "Face Target", "Release Animation", "Move From Offset",
        "Play Mesh VFX", "Stop Mesh VFX", "Play Particle", "Stop Particle" };
    for (int index_ = 0; index_ < static_cast<int>(presets.size()); ++index_)
    {
        ImGui::PushID(index_);
        auto& preset_ = presets[index_];
        ImGui::InputText("Action Name", &preset_.actionName);
        ImGui::InputText("Actor Object", &preset_.actorName);
        int type_ = static_cast<int>(preset_.type);
        if (ImGui::Combo("Type", &type_, TYPES, IM_ARRAYSIZE(TYPES))) preset_.type = static_cast<ActorPresentationActionType>(type_);
        ImGui::InputText("Animation State", &preset_.stateName);
        ImGui::InputText("State Asset Path", &preset_.stateAssetPath);
        ImGui::InputText("Look Target", &preset_.targetName);
        ImGui::DragFloat("Blend / Turn / Move Duration", &preset_.duration, 0.01f, 0.0f, 60.0f);
        if (preset_.type == ActorPresentationActionType::MOVE_FROM_OFFSET)
            ImGui::DragFloat3("Entry Position Offset", &preset_.positionOffset.x, 0.05f);
        ImGui::Separator();
        ImGui::PopID();
    }
    if (ImGui::Button("Add Actor Action")) presets.emplace_back();
    if (ImGui::Button("Register Actor Actions")) RegisterActions();
}

void ActorPresentationActionComponent::Serialize(nlohmann::json& json_) const
{
    json_["Presets"] = nlohmann::json::array();
    for (const auto& preset_ : presets)
        json_["Presets"].push_back({ {"ActionName", preset_.actionName}, {"ActorName", preset_.actorName},
            {"StateName", preset_.stateName}, {"StateAssetPath", preset_.stateAssetPath}, {"TargetName", preset_.targetName},
            {"Type", static_cast<int>(preset_.type)}, {"Duration", preset_.duration},
            {"PositionOffset", {preset_.positionOffset.x, preset_.positionOffset.y, preset_.positionOffset.z}} });
}

void ActorPresentationActionComponent::Deserialize(const nlohmann::json& json_)
{
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
        preset_.targetName = item_.value("TargetName", std::string{});
        preset_.type = static_cast<ActorPresentationActionType>(std::clamp(item_.value("Type", 0), 0, static_cast<int>(ActorPresentationActionType::STOP_PARTICLE)));
        preset_.duration = (std::max)(item_.value("Duration", 0.2f), 0.0f);
        // 旧JSONではオフセットを0とし、既存のAnimation・旋回設定を維持する。
        if (item_.contains("PositionOffset") && item_["PositionOffset"].is_array() && item_["PositionOffset"].size() == 3)
            preset_.positionOffset = { item_["PositionOffset"][0], item_["PositionOffset"][1], item_["PositionOffset"][2] };
        presets.push_back(preset_);
    }
}

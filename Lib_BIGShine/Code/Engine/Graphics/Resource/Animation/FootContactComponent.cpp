#include "PrecompiledHeader.h"

#include "FootContactComponent.h"

#include "AnimationControllerComponent.h"
#include "Engine/Debug/TracyProfiler.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
#include "Engine/GamePlay/System/EffectManager.h"
#include "Engine/Graphics/Particle/ParticleComponent.h"
#include "Engine/Graphics/Resource/GlTFModel.h"
#include "Engine/Physics/ColliderComponent.h"
#include "Engine/Physics/PhysXSystem.h"

#include <PxPhysicsAPI.h>
#include <SDL3/SDL_timer.h>
#include <algorithm>
#include <cmath>
#include <imgui_stdlib.h>

namespace
{
    class FootContactGroundFilter : public physx::PxQueryFilterCallback
    {
    public:
        explicit FootContactGroundFilter(std::uint32_t ownerId_) : ownerId(ownerId_) {}

        physx::PxQueryHitType::Enum preFilter(const physx::PxFilterData&, const physx::PxShape* shape_,
            const physx::PxRigidActor*, physx::PxHitFlags&) override
        {
            if (!shape_ || shape_->getFlags().isSet(physx::PxShapeFlag::eTRIGGER_SHAPE))
                return physx::PxQueryHitType::eNONE;

            constexpr std::uint32_t COLLISION_OWNER_MASK = 0x7FFFFFFFu;
            const auto data_ = shape_->getSimulationFilterData();
            if ((ownerId != 0 && (data_.word3 & COLLISION_OWNER_MASK) == ownerId) ||
                data_.word2 != static_cast<physx::PxU32>(ColliderComponent::CollisionRole::PHYSICAL))
                return physx::PxQueryHitType::eNONE;

            return physx::PxQueryHitType::eBLOCK;
        }

        physx::PxQueryHitType::Enum postFilter(const physx::PxFilterData&, const physx::PxQueryHit& hit_,
            const physx::PxShape*, const physx::PxRigidActor*) override
        {
            // 下向きのRayに対して、床として扱える上向きの面だけを採用する。
            const auto& rayHit_ = static_cast<const physx::PxRaycastHit&>(hit_);
            return rayHit_.normal.y >= 0.5f ? physx::PxQueryHitType::eBLOCK : physx::PxQueryHitType::eNONE;
        }

    private:
        std::uint32_t ownerId;
    };
}

FootContactComponent::FootContactComponent()
{
    // Animation更新後、同じObjectの足IKより後で最終姿勢を読む。
    SetLateUpdatePriority(100);
}

const std::string& FootContactComponent::GetName() const
{
    static const std::string NAME = "FootContactComponent";
    return NAME;
}

void FootContactComponent::LateUpdate(float deltaTime_)
{
    BS_PROFILE_SCOPE("FootContactComponent::LateUpdate");
    const auto owner_ = GetGameObject();
    const auto model_ = owner_ ? owner_->GetComponent<GltfModelCommponennt>() : nullptr;
    auto* scene_ = PhysXSystem::GetScene();
    if (!contactEnabled || feet.empty() || !owner_ || !owner_->manager || !model_ || !model_->IsModelReady() ||
        !scene_ || !std::isfinite(deltaTime_) || deltaTime_ <= 0.00001f)
    {
        for (auto& foot_ : feet)
        {
            foot_.hasSample = false;
            foot_.grounded = false;
            foot_.velocityY = 0.0f;
            foot_.boneFound = false;
        }
        previousSampleTime = 0;
        return;
    }

    model_->UpdateTransformIfDirty();
    const auto animation_ = owner_->GetComponent<AnimationControllerComponent>();
    const auto* state_ = animation_ ? animation_->GetCurrentState() : nullptr;
    const float animationTime_ = animation_ ? animation_->GetAnimatorTime() : 0.0f;
    const std::string animationName_ = state_ ? state_->name : "";
    const auto now_ = SDL_GetTicks();
    constexpr std::uint64_t MAX_SAMPLE_GAP_MS = 150;
    // クリップ切り替えや時間の巻き戻しでは前の姿勢を速度に混ぜない。
    const bool resetSample_ = previousSampleTime == 0 || now_ - previousSampleTime > MAX_SAMPLE_GAP_MS ||
        previousAnimation != animationName_ || animationTime_ < previousAnimationTime;
    previousSampleTime = now_;
    previousAnimation = animationName_;
    previousAnimationTime = animationTime_;

    const auto collider_ = owner_->GetComponent<ColliderComponent>();
    constexpr std::uintptr_t COLLISION_OWNER_MASK = 0x7FFFFFFFu;
    const auto ownerId_ = collider_ ? collider_->GetCollisionOwnerId()
        : static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(owner_.get()) & COLLISION_OWNER_MASK);
    FootContactGroundFilter filter_(ownerId_);
    const physx::PxQueryFilterData query_(physx::PxFilterData(),
        physx::PxQueryFlag::eSTATIC | physx::PxQueryFlag::eDYNAMIC |
        physx::PxQueryFlag::ePREFILTER | physx::PxQueryFlag::ePOSTFILTER);
    const float release_ = (std::max)(contactDistance, releaseDistance);
    const float rayLift_ = (std::max)(release_, 0.25f);
    const auto& nodes_ = model_->GetNodes();

    for (auto& foot_ : feet)
    {
        foot_.cooldownRemaining = (std::max)(0.0f, foot_.cooldownRemaining - deltaTime_);
        const int index_ = foot_.enabled && !foot_.boneName.empty() ? model_->GetNodeIndex(foot_.boneName.c_str()) : -1;
        foot_.boneFound = index_ >= 0 && static_cast<std::size_t>(index_) < nodes_.size();
        if (!foot_.boneFound)
        {
            foot_.hasSample = false;
            foot_.grounded = false;
            foot_.velocityY = 0.0f;
            continue;
        }

        const auto& world_ = nodes_[index_].worldTransform;
        const DirectX::XMFLOAT3 position_{ world_._41, world_._42, world_._43 };
        const bool canMeasure_ = foot_.hasSample && !resetSample_;
        foot_.velocityY = canMeasure_ ? (position_.y - foot_.previousPosition.y) / deltaTime_ : 0.0f;
        foot_.previousPosition = position_;
        foot_.hasSample = true;

        physx::PxRaycastBuffer hit_;
        const bool hasGround_ = scene_->raycast(
            { position_.x, position_.y + rayLift_, position_.z }, { 0.0f, -1.0f, 0.0f },
            rayLift_ + release_, hit_, physx::PxHitFlag::eDEFAULT, query_, &filter_) && hit_.hasBlock;
        const bool wasGrounded_ = foot_.grounded;
        const float threshold_ = canMeasure_ && wasGrounded_ ? release_ : contactDistance;
        foot_.grounded = hasGround_ && position_.y - hit_.block.position.y <= threshold_;
        if (!canMeasure_ || wasGrounded_ || !foot_.grounded) continue;

        foot_.lastImpactSpeed = (std::max)(0.0f, -foot_.velocityY);
        if (foot_.lastImpactSpeed <= 0.0f || foot_.lastImpactSpeed < minimumImpactSpeed ||
            foot_.cooldownRemaining > 0.0f || foot_.particleEffect.empty())
            continue;

        ParticleSpawnRequest request_;
        request_.assetPath = foot_.particleEffect;
        request_.position = { hit_.block.position.x, hit_.block.position.y, hit_.block.position.z };
        request_.normal = { hit_.block.normal.x, hit_.block.normal.y, hit_.block.normal.z };
        request_.normalOffset = 0.025f;
        const auto effect_ = owner_->GetManager().GetEffectManager().SpawnParticle(request_);
        if (!effect_) continue;
        if (const auto particle_ = effect_->GetComponent<ParticleComponent>())
            particle_->SetRuntimeVisualScale(foot_.effectScale);
        foot_.cooldownRemaining = effectCooldown;
        ++foot_.effectCount;
    }
}

void FootContactComponent::Gui()
{
    if (ImGui::Checkbox("Foot Contact Enabled", &contactEnabled))
        for (auto& foot_ : feet) foot_.hasSample = false;
    ImGui::DragFloat("Contact Distance", &contactDistance, 0.01f, 0.0f, 5.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat("Release Distance", &releaseDistance, 0.01f, 0.0f, 5.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
    releaseDistance = (std::max)(contactDistance, releaseDistance);
    ImGui::DragFloat("Minimum Impact Speed", &minimumImpactSpeed, 0.05f, 0.0f, 100.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat("Effect Cooldown (s)", &effectCooldown, 0.01f, 0.0f, 5.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);

    if (ImGui::Button("Add Foot")) feet.emplace_back();
    for (std::size_t index_ = 0; index_ < feet.size();)
    {
        ImGui::PushID(static_cast<int>(index_));
        auto& foot_ = feet[index_];
        const char* label_ = foot_.boneName.empty() ? "New Foot" : foot_.boneName.c_str();
        bool remove_ = false;
        if (ImGui::TreeNode("Foot", "%s", label_))
        {
            if (ImGui::Checkbox("Enabled", &foot_.enabled)) foot_.hasSample = false;
            if (ImGui::InputText("Bone Name", &foot_.boneName)) foot_.hasSample = false;
            ImGui::InputText("Particle Effect", &foot_.particleEffect);
            ImGui::DragFloat("Effect Scale", &foot_.effectScale, 0.05f, 0.01f, 20.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
            ImGui::Text("Bone: %s | Contact: %s", foot_.boneFound ? "Found" : "Unavailable", foot_.grounded ? "Grounded" : "Airborne");
            ImGui::Text("Y Speed: %.2f | Last Impact: %.2f | Effects: %u", foot_.velocityY, foot_.lastImpactSpeed, foot_.effectCount);
            remove_ = ImGui::Button("Remove Foot");
            ImGui::TreePop();
        }
        ImGui::PopID();
        if (remove_) feet.erase(feet.begin() + index_);
        else ++index_;
    }
}

void FootContactComponent::Serialize(nlohmann::json& json_) const
{
    json_["ContactEnabled"] = contactEnabled;
    json_["ContactDistance"] = contactDistance;
    json_["ReleaseDistance"] = releaseDistance;
    json_["MinimumImpactSpeed"] = minimumImpactSpeed;
    json_["EffectCooldown"] = effectCooldown;
    json_["Feet"] = nlohmann::json::array();
    for (const auto& foot_ : feet)
        json_["Feet"].push_back({
            { "BoneName", foot_.boneName }, { "ParticleEffect", foot_.particleEffect },
            { "EffectScale", foot_.effectScale }, { "Enabled", foot_.enabled }
        });
}

void FootContactComponent::Deserialize(const nlohmann::json& json_)
{
    contactEnabled = json_.value("ContactEnabled", true);
    contactDistance = std::clamp(json_.value("ContactDistance", 0.16f), 0.0f, 5.0f);
    releaseDistance = std::clamp(json_.value("ReleaseDistance", 0.28f), contactDistance, 5.0f);
    minimumImpactSpeed = std::clamp(json_.value("MinimumImpactSpeed", 0.8f), 0.0f, 100.0f);
    effectCooldown = std::clamp(json_.value("EffectCooldown", 0.12f), 0.0f, 5.0f);
    previousSampleTime = 0;
    feet.clear();
    const auto list_ = json_.find("Feet");
    if (list_ == json_.end() || !list_->is_array()) return;
    for (const auto& item_ : *list_)
    {
        if (!item_.is_object()) continue;
        auto& foot_ = feet.emplace_back();
        foot_.boneName = item_.value("BoneName", std::string{});
        foot_.particleEffect = item_.value("ParticleEffect", std::string{});
        foot_.effectScale = std::clamp(item_.value("EffectScale", 1.0f), 0.01f, 20.0f);
        foot_.enabled = item_.value("Enabled", true);
    }
}

REGISTER_COMPONENT(FootContactComponent, "FootContactComponent")

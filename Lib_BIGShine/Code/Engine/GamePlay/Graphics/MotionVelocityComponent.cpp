#include "MotionVelocityComponent.h"

#include <algorithm>

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Transform/Transform.h"

const std::string& MotionVelocityComponent::GetName() const
{
    static std::string name_ = "MotionVelocityComponent";
    return name_;
}

void MotionVelocityComponent::Start()
{
    ResetHistory();
    CaptureCurrentWorld();
    CommitHistory();

    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (owner_)
    {
        owner_->GetManager().RegisterMotionVelocityComponent(
            owner_->GetComponent<MotionVelocityComponent>());
    }
}

void MotionVelocityComponent::LateUpdate(float deltaTime_)
{
    (void)deltaTime_;
}

void MotionVelocityComponent::Gui()
{
    ImGui::Checkbox("Velocity Enabled", &velocityEnabled);

    if (ImGui::DragFloat("Velocity Scale", &velocityScale, 0.01f, 0.0f, 8.0f, "%.2f"))
    {
        velocityScale = ClampScale(velocityScale);
    }

    if (ImGui::TreeNode("Target Types"))
    {
        ImGui::Checkbox("Model", &affectModel);
        ImGui::SameLine();
        if (ImGui::DragFloat("Model Scale", &modelVelocityScale, 0.01f, 0.0f, 8.0f, "%.2f"))
        {
            modelVelocityScale = ClampScale(modelVelocityScale);
        }

        ImGui::Checkbox("Particle", &affectParticle);
        ImGui::SameLine();
        if (ImGui::DragFloat("Particle Scale", &particleVelocityScale, 0.01f, 0.0f, 8.0f, "%.2f"))
        {
            particleVelocityScale = ClampScale(particleVelocityScale);
        }

        ImGui::Checkbox("MeshVFX", &affectMeshVFX);
        ImGui::SameLine();
        if (ImGui::DragFloat("MeshVFX Scale", &meshVFXVelocityScale, 0.01f, 0.0f, 8.0f, "%.2f"))
        {
            meshVFXVelocityScale = ClampScale(meshVFXVelocityScale);
        }
        ImGui::TreePop();
    }

    if (ImGui::Button("Reset Motion History"))
    {
        ResetHistory();
        CaptureCurrentWorld();
    }

    ImGui::Text("History: %s", hasHistory ? "Ready" : "Reset");
}

void MotionVelocityComponent::Serialize(nlohmann::json& json_) const
{
    json_["VelocityEnabled"] = velocityEnabled;
    json_["VelocityScale"] = velocityScale;
    json_["AffectModel"] = affectModel;
    json_["AffectParticle"] = affectParticle;
    json_["AffectMeshVFX"] = affectMeshVFX;
    json_["ModelVelocityScale"] = modelVelocityScale;
    json_["ParticleVelocityScale"] = particleVelocityScale;
    json_["MeshVFXVelocityScale"] = meshVFXVelocityScale;
}

void MotionVelocityComponent::Deserialize(const nlohmann::json& json_)
{
    velocityEnabled = json_.value("VelocityEnabled", true);
    velocityScale = json_.value("VelocityScale", 1.0f);
    affectModel = json_.value("AffectModel", true);
    affectParticle = json_.value("AffectParticle", true);
    affectMeshVFX = json_.value("AffectMeshVFX", true);
    modelVelocityScale = json_.value("ModelVelocityScale", 1.0f);
    particleVelocityScale = json_.value("ParticleVelocityScale", 1.0f);
    meshVFXVelocityScale = json_.value("MeshVFXVelocityScale", 1.0f);
    velocityScale = ClampScale(velocityScale);
    modelVelocityScale = ClampScale(modelVelocityScale);
    particleVelocityScale = ClampScale(particleVelocityScale);
    meshVFXVelocityScale = ClampScale(meshVFXVelocityScale);
    ResetHistory();
}

void MotionVelocityComponent::CaptureCurrentWorld()
{
    DirectX::XMMATRIX world_ = DirectX::XMMatrixIdentity();
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (owner_ && owner_->transform)
    {
        world_ = owner_->transform->GetWorldMatrix();
    }

    DirectX::XMStoreFloat4x4(&currentWorld, world_);
}

void MotionVelocityComponent::CommitHistory()
{
    previousWorld = currentWorld;
    hasHistory = true;
}

void MotionVelocityComponent::ResetHistory()
{
    DirectX::XMStoreFloat4x4(&currentWorld, DirectX::XMMatrixIdentity());
    previousWorld = currentWorld;
    hasHistory = false;
}

float MotionVelocityComponent::ClampScale(float scale_) const
{
    return (std::max)(0.0f, scale_);
}

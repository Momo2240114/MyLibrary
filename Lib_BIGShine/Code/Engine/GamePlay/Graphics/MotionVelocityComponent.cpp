#include "PrecompiledHeader.h"

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
    // 登録直後の初回Frameで巨大な速度が出ないよう、現在Transformで履歴を初期化する。
    ResetHistory();
    CaptureCurrentWorld();
    CommitHistory();

    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (owner_)
    {
        // GameObjectManager経由でMotionVelocityRenderSystemへ描画対象として登録する。
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
    // MotionBlurへの影響有無と、対象種別ごとの倍率をEditorから調整する。
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
        // WarpやTeleport後に残像が伸びすぎる場合、履歴を手動で現在位置へリセットする。
        ResetHistory();
        CaptureCurrentWorld();
    }

    ImGui::Text("History: %s", hasHistory ? "Ready" : "Reset");
}

void MotionVelocityComponent::Serialize(nlohmann::json& json_) const
{
    // Velocity描画の有効状態と倍率だけを保存する。Frame履歴はRuntime状態なので保存しない。
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
    // 古いSceneに項目が無くても既定値で復元し、倍率は0以上に丸める。
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
    // OwnerのTransformが無い場合はIdentityとして扱い、Shaderへ不定行列を渡さない。
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
    // Velocity描画後に呼び、現在Worldを次FrameのPreviousとして使う。
    previousWorld = currentWorld;
    hasHistory = true;
}

void MotionVelocityComponent::ResetHistory()
{
    // 履歴をIdentityで初期化し、次のCapture/Commitで現在Transformに揃える。
    DirectX::XMStoreFloat4x4(&currentWorld, DirectX::XMMatrixIdentity());
    previousWorld = currentWorld;
    hasHistory = false;
}

float MotionVelocityComponent::ClampScale(float scale_) const
{
    // 負の倍率は速度方向反転になって扱いづらいため、0以上に制限する。
    return (std::max)(0.0f, scale_);
}

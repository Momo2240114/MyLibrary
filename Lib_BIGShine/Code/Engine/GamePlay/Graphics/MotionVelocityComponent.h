#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"

class MotionVelocityComponent : public Component
{
public:
    const std::string& GetName() const override;

    void Start() override;
    void LateUpdate(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    void CaptureCurrentWorld();
    void CommitHistory();
    void ResetHistory();

    bool HasHistory() const { return hasHistory; }
    bool IsVelocityEnabled() const { return velocityEnabled; }
    float GetVelocityScale() const { return velocityScale; }
    bool ShouldAffectModel() const { return affectModel; }
    bool ShouldAffectParticle() const { return affectParticle; }
    bool ShouldAffectMeshVFX() const { return affectMeshVFX; }
    float GetModelVelocityScale() const { return modelVelocityScale; }
    float GetParticleVelocityScale() const { return particleVelocityScale; }
    float GetMeshVFXVelocityScale() const { return meshVFXVelocityScale; }

    void SetVelocityEnabled(bool velocityEnabled_) { velocityEnabled = velocityEnabled_; }
    void SetVelocityScale(float velocityScale_) { velocityScale = ClampScale(velocityScale_); }
    void SetAffectModel(bool affectModel_) { affectModel = affectModel_; }
    void SetAffectParticle(bool affectParticle_) { affectParticle = affectParticle_; }
    void SetAffectMeshVFX(bool affectMeshVFX_) { affectMeshVFX = affectMeshVFX_; }

    const DirectX::XMFLOAT4X4& GetCurrentWorld() const { return currentWorld; }
    const DirectX::XMFLOAT4X4& GetPreviousWorld() const { return previousWorld; }

private:
    float ClampScale(float scale_) const;

    bool velocityEnabled = true;
    bool hasHistory = false;
    float velocityScale = 1.0f;
    bool affectModel = true;
    bool affectParticle = true;
    bool affectMeshVFX = true;
    float modelVelocityScale = 1.0f;
    float particleVelocityScale = 1.0f;
    float meshVFXVelocityScale = 1.0f;
    DirectX::XMFLOAT4X4 currentWorld{};
    DirectX::XMFLOAT4X4 previousWorld{};
};

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

    // 現在FrameのWorld行列を取得して保持する。
    void CaptureCurrentWorld();
    // 現在Worldを次Frameのpreviousとして確定する。
    void CommitHistory();
    // 履歴を初期化し、初回FrameでVelocityが暴れないようにする。
    void ResetHistory();

    // previousWorldが有効かどうか。
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
    // Velocity倍率は負値にならないように丸める。
    float ClampScale(float scale_) const;

    // MotionBlur用Velocityを書き込むかどうか。
    bool velocityEnabled = true;
    // previousWorldが有効な状態かどうか。
    bool hasHistory = false;
    // 全対象に掛かる基本Velocity倍率。
    float velocityScale = 1.0f;
    // Velocityを反映する描画対象の種類。
    bool affectModel = true;
    bool affectParticle = true;
    bool affectMeshVFX = true;
    // 対象種類ごとの追加Velocity倍率。
    float modelVelocityScale = 1.0f;
    float particleVelocityScale = 1.0f;
    float meshVFXVelocityScale = 1.0f;
    // Velocity計算用の現在Frame/前FrameのWorld行列。
    DirectX::XMFLOAT4X4 currentWorld{};
    DirectX::XMFLOAT4X4 previousWorld{};
};

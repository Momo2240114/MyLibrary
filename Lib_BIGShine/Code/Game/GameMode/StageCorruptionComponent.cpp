#include "PrecompiledHeader.h"
#include "StageCorruptionComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/Physics/ContainmentVolumeComponent.h"
#include "Engine/Graphics/Particle/ParticleComponent.h"
#include "Engine/Graphics/VFX/MeshVFXComponent.h"
#include "Engine/Math/Curve/CurveComponent.h"
#include "Engine/GamePlay/Camera/CameraLockOnComponent.h"
#include "Engine/GamePlay/Camera/SpecialAttackCameraComponent.h"
#include "Engine/GamePlay/Camera/ThirdPersonCameraComponent.h"
#include "Engine/GamePlay/Camera/CameraCollisionComponent.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <imgui_stdlib.h>

void StageCorruptionComponent::Start()
{
    RegisterActions();
    if (Prepare())
    {
        Restore();
    }
}

void StageCorruptionComponent::RegisterActions()
{
    const auto owner_ = GetGameObject();
    if (!owner_)
    {
        return;
    }
    const std::weak_ptr<GameObject> weakOwner_ = owner_;
    owner_->RegisterGlobalAction("StageCorruption", "Begin", [weakOwner_]()
    {
        const auto owner_ = weakOwner_.lock();
        if (const auto component_ = owner_ ? owner_->GetComponent<StageCorruptionComponent>() : nullptr)
        {
            component_->Begin();
        }
    });
    owner_->RegisterGlobalAction("StageCorruption", "Restore", [weakOwner_]()
    {
        const auto owner_ = weakOwner_.lock();
        if (const auto component_ = owner_ ? owner_->GetComponent<StageCorruptionComponent>() : nullptr)
        {
            component_->Restore();
        }
    });
    owner_->RegisterGlobalAction("StageCorruption", "ApplyCameraProfile", [weakOwner_]()
    {
        const auto owner_ = weakOwner_.lock();
        if (const auto component_ = owner_ ? owner_->GetComponent<StageCorruptionComponent>() : nullptr)
        {
            component_->ApplyCameraProfile();
        }
    });
    owner_->RegisterGlobalAction("StageCorruption", "Complete", [weakOwner_]()
    {
        const auto owner_ = weakOwner_.lock();
        if (const auto component_ = owner_ ? owner_->GetComponent<StageCorruptionComponent>() : nullptr)
        {
            component_->Complete();
        }
    });
}

bool StageCorruptionComponent::Prepare()
{
    BS_PROFILE_SCOPE("StageCorruptionComponent::Prepare");
    if (prepared)
    {
        return true;
    }
    const auto owner_ = GetGameObject();
    if (!owner_)
    {
        return false;
    }
    const auto barrier_ = owner_->GetManager().Find(barrierName);
    const auto volume_ = barrier_ ? barrier_->GetComponent<ContainmentVolumeComponent>() : nullptr;
    const auto mesh_ = barrier_ ? barrier_->GetComponent<MeshVFXComponent>() : nullptr;
    const auto first_ = owner_->GetManager().Find(firstParticleName);
    const auto second_ = owner_->GetManager().Find(secondParticleName);
    if (!volume_ || !mesh_ || mesh_->GetAsset().parts.empty() || !barrier_->transform ||
        !first_ || !second_ || !first_->GetComponent<ParticleComponent>() || !second_->GetComponent<ParticleComponent>())
    {
        status = "Barrier / environment reference missing";
        return false;
    }
    try
    {
        std::ifstream file_(targetBarrierAssetPath);
        nlohmann::json asset_;
        if (!file_.is_open())
        {
            status = "HumanBoss barrier asset unavailable";
            return false;
        }
        file_ >> asset_;
        float targetRadius_ = 0.0f;
        for (const auto& component_ : asset_.at("Components"))
        {
            if (component_.value("TypeName", "") == "ContainmentVolumeComponent")
            {
                targetRadius_ = component_.at("Data").at("Radius").get<float>();
            }
        }
        const auto& scale_ = asset_.at("Transform").at("Scale");
        const float targetScale_ = (std::max)(std::abs(scale_[0].get<float>()), std::abs(scale_[2].get<float>()));
        const auto ownerScale_ = barrier_->transform->GetWorldScale();
        const float currentScale_ = (std::max)(std::abs(ownerScale_.x), std::abs(ownerScale_.z));
        if (!std::isfinite(targetRadius_) || targetRadius_ <= 0.0f || currentScale_ <= 0.0001f)
        {
            status = "HumanBoss barrier radius invalid";
            return false;
        }
        // 参照先のワールド半径を現在のバリア座標へ換算し、設定値の二重管理を避ける。
        secondRadius = targetRadius_ * targetScale_ / currentScale_;
    }
    catch (const nlohmann::json::exception&)
    {
        status = "HumanBoss barrier JSON invalid";
        return false;
    }
    firstRadius = volume_->radius;
    const auto& part_ = mesh_->GetAsset().parts.front();
    visualRadius = (std::max)(std::abs(part_.scale.x) * part_.shapeCylinderRadius, 0.001f);
    firstColor = part_.color;
    firstEdgeColor = part_.edgeColor;
    if (const auto particle_ = barrier_->GetComponent<ParticleComponent>())
    {
        if (!particle_->GetTracks().empty())
        {
            firstRingColor = particle_->GetTracks().front().settings.manualColor;
        }
    }
    prepared = true;
    status = "Ready";
    return true;
}

void StageCorruptionComponent::Begin()
{
    // 第一形態で編集された設定と参照先Assetを、再生直前に読み直す。
    if (currentRate <= 0.0f)
    {
        prepared = false;
    }
    if (!Prepare() || playing)
    {
        return;
    }
    elapsed = 0.0f;
    playing = true;
    currentRate = 0.0f;
    const auto owner_ = GetGameObject();
    const auto second_ = owner_->GetManager().Find(secondParticleName);
    if (const auto particle_ = second_ ? second_->GetComponent<ParticleComponent>() : nullptr)
    {
        particle_->SetEmissionScale(0.0f);
        particle_->Play();
    }
    Apply(0.0f);
}

void StageCorruptionComponent::Restore()
{
    BS_PROFILE_SCOPE("StageCorruptionComponent::Restore");
    const auto profileOwner_ = GetGameObject();
    const auto cameraObject_ = profileOwner_ && profileOwner_->manager ? profileOwner_->GetManager().Find(cameraObjectName) : nullptr;
    if (cameraObject_)
    {
        if (const auto camera_ = cameraObject_->GetComponent<CameraLockOnComponent>())
        {
            camera_->ClearRuntimeSettings();
        }
        if (const auto camera_ = cameraObject_->GetComponent<ThirdPersonCameraComponent>())
        {
            camera_->ClearRuntimeSettings();
        }
        if (const auto camera_ = cameraObject_->GetComponent<CameraCollisionComponent>())
        {
            camera_->ClearRuntimeSettings();
        }
    }
    cameraProfileApplied = false;
    if (!Prepare())
    {
        return;
    }
    playing = false;
    elapsed = 0.0f;
    currentRate = 0.0f;
    Apply(0.0f);
    const auto owner_ = GetGameObject();
    const auto first_ = owner_->GetManager().Find(firstParticleName);
    const auto second_ = owner_->GetManager().Find(secondParticleName);
    if (const auto particle_ = first_ ? first_->GetComponent<ParticleComponent>() : nullptr)
    {
        particle_->Play();
    }
    // デバッグ復元では第二形態の残留煙も消し、次の再生へ持ち越さない。
    if (const auto particle_ = second_ ? second_->GetComponent<ParticleComponent>() : nullptr)
    {
        particle_->ResetRuntime();
    }
}

void StageCorruptionComponent::Complete()
{
    if (!Prepare())
    {
        return;
    }
    playing = false;
    currentRate = 1.0f;
    Apply(1.0f);
    const auto owner_ = GetGameObject();
    const auto second_ = owner_->GetManager().Find(secondParticleName);
    if (const auto particle_ = second_ ? second_->GetComponent<ParticleComponent>() : nullptr)
    {
        particle_->Play();
    }
}

void StageCorruptionComponent::Update(float deltaTime_)
{
    BS_PROFILE_SCOPE("StageCorruptionComponent::Update");
    if (!playing)
    {
        return;
    }
    elapsed += (std::max)(deltaTime_, 0.0f);
    const float rate_ = duration <= 0.0f ? 1.0f : std::clamp(elapsed / duration, 0.0f, 1.0f);
    currentRate = smoothTransition ? rate_ * rate_ * (3.0f - 2.0f * rate_) : rate_;
    const auto owner_ = GetGameObject();
    const auto curve_ = owner_ ? owner_->GetComponent<CurveComponent>() : nullptr;
    if (curve_ && !curveName.empty() && curve_->HasTrack(curveName))
    {
        currentRate = std::clamp(curve_->GetValue(curveName, rate_), 0.0f, 1.0f);
    }
    if (rate_ >= 1.0f)
    {
        currentRate = 1.0f;
    }
    Apply(currentRate);
    if (rate_ >= 1.0f)
    {
        playing = false;
    }
}

void StageCorruptionComponent::Apply(float rate_)
{
    BS_PROFILE_SCOPE("StageCorruptionComponent::Apply");
    const auto owner_ = GetGameObject();
    if (!owner_)
    {
        return;
    }
    const auto barrier_ = owner_->GetManager().Find(barrierName);
    const auto volume_ = barrier_ ? barrier_->GetComponent<ContainmentVolumeComponent>() : nullptr;
    const auto mesh_ = barrier_ ? barrier_->GetComponent<MeshVFXComponent>() : nullptr;
    if (!volume_ || !mesh_)
    {
        return;
    }
    const float radius_ = firstRadius + (secondRadius - firstRadius) * rate_;
    if (rate_ <= 0.0f)
    {
        volume_->ClearRuntimeRadius();
    }
    else
    {
        volume_->SetRuntimeRadius(radius_);
    }
    // 見た目と実際の移動制限に同じ半径を使う。物理壁は既存のStep前同期が再構築する。
    const float scale_ = radius_ / visualRadius;
    const auto interpolateColor_ = [rate_](const DirectX::XMFLOAT4& from_, const DirectX::XMFLOAT4& to_)
    {
        return DirectX::XMFLOAT4{ from_.x + (to_.x - from_.x) * rate_, from_.y + (to_.y - from_.y) * rate_,
            from_.z + (to_.z - from_.z) * rate_, from_.w + (to_.w - from_.w) * rate_ };
    };
    // 第一形態の基本色・輪郭色・外周粒子色をそれぞれ維持してから補間する。
    const auto color_ = interpolateColor_(firstColor, targetColor);
    const auto edgeColor_ = interpolateColor_(firstEdgeColor, targetEdgeColor);
    if (rate_ <= 0.0f)
    {
        mesh_->ClearRuntimeAppearance();
    }
    else
    {
        mesh_->SetRuntimeAppearance({ scale_, 1.0f, scale_ }, color_, edgeColor_);
    }
    const auto worldScale_ = barrier_->transform->GetWorldScale();
    const float worldRadius_ = radius_ * (std::max)(std::abs(worldScale_.x), std::abs(worldScale_.z));
    for (const auto& camera_ : owner_->GetManager().FindComponents<SpecialAttackCameraComponent>())
    {
        if (rate_ <= 0.0f)
        {
            camera_->ClearArenaBounds();
        }
        else
        {
            camera_->SetArenaBounds(barrier_->transform->GetWorldPosition(), worldRadius_);
        }
    }
    const auto first_ = owner_->GetManager().Find(firstParticleName);
    const auto second_ = owner_->GetManager().Find(secondParticleName);
    if (const auto particle_ = first_ ? first_->GetComponent<ParticleComponent>() : nullptr)
    {
        particle_->SetEmissionScale(1.0f - rate_);
        if (rate_ >= 1.0f)
        {
            particle_->Stop();
        }
    }
    if (const auto particle_ = second_ ? second_->GetComponent<ParticleComponent>() : nullptr)
    {
        particle_->SetEmissionScale(rate_);
    }
    // 外周の発生位置も壁に合わせ、元のParticle Assetを上書きしない。
    if (const auto particle_ = barrier_->GetComponent<ParticleComponent>())
    {
        if (rate_ <= 0.0f)
        {
            particle_->ClearRuntimeRingAppearance();
        }
        else
        {
            particle_->SetRuntimeRingAppearance(radius_, interpolateColor_(firstRingColor, targetColor));
        }
    }
}

void StageCorruptionComponent::ApplyCameraProfile()
{
    BS_PROFILE_SCOPE("StageCorruptionComponent::ApplyCameraProfile");
    if (cameraProfileApplied)
    {
        return;
    }
    const auto owner_ = GetGameObject();
    if (!owner_ || !owner_->manager || !Prepare())
    {
        return;
    }
    const auto object_ = owner_->GetManager().Find(cameraObjectName);
    if (!object_)
    {
        status = "Gameplay camera missing";
        return;
    }
    try
    {
        if (targetCameraProfile.is_null())
        {
            std::ifstream file_(targetCameraAssetPath);
            if (!file_.is_open())
            {
                status = "HumanBoss camera asset unavailable";
                return;
            }
            file_ >> targetCameraProfile;
        }
        // 対象の生成前でも適用できるよう、LockOnは戦闘引渡しActionが有効化する。
        for (const auto& component_ : targetCameraProfile.at("Components"))
        {
            const auto type_ = component_.value("TypeName", std::string{});
            auto settings_ = component_.at("Data");
            if (type_ == "CameraLockOnComponent")
            {
                settings_["lockOnEnabled"] = false;
                if (const auto camera_ = object_->GetComponent<CameraLockOnComponent>())
                {
                    camera_->ApplyRuntimeSettings(settings_);
                }
            }
            else if (type_ == "ThirdPersonCameraComponent")
            {
                if (const auto camera_ = object_->GetComponent<ThirdPersonCameraComponent>())
                {
                    camera_->ApplyRuntimeSettings(settings_);
                }
            }
            else if (type_ == "CameraCollisionComponent")
            {
                if (const auto camera_ = object_->GetComponent<CameraCollisionComponent>())
                {
                    camera_->ApplyRuntimeSettings(settings_);
                }
            }
        }
        cameraProfileApplied = true;
        // プロファイル読込で設定を切り替えた後も、移行中の実際のバリア半径を優先する。
        Apply(currentRate);
    }
    catch (const nlohmann::json::exception&)
    {
        status = "HumanBoss camera asset is invalid";
    }
}

void StageCorruptionComponent::Gui()
{
    ImGui::InputText("Barrier Object", &barrierName);
    ImGui::InputText("First Environment", &firstParticleName);
    ImGui::InputText("Second Environment", &secondParticleName);
    ImGui::InputText("HumanBoss Barrier Asset", &targetBarrierAssetPath);
    ImGui::InputText("Gameplay Camera", &cameraObjectName);
    if (ImGui::InputText("HumanBoss Camera Asset", &targetCameraAssetPath))
    {
        targetCameraProfile = nlohmann::json{};
        cameraProfileApplied = false;
    }
    if (ImGui::Button("Apply Second Form Camera"))
    {
        ApplyCameraProfile();
    }
    ImGui::InputText("Curve Track", &curveName);
    ImGui::Checkbox("Smooth Transition", &smoothTransition);
    ImGui::DragFloat("Duration", &duration, 0.05f, 0.0f, 60.0f);
    ImGui::ColorEdit4("Second Barrier Color", &targetColor.x);
    ImGui::ColorEdit4("Second Barrier Edge Color", &targetEdgeColor.x);
    if (ImGui::Button("Begin Corruption"))
    {
        Begin();
    }
    ImGui::SameLine();
    if (ImGui::Button("Complete Corruption"))
    {
        Complete();
    }
    if (ImGui::Button("Restore Environment"))
    {
        Restore();
    }
    if (ImGui::Button("Restore And Reload Settings"))
    {
        Restore();
        prepared = false;
        Prepare();
    }
    ImGui::Text("%s / Rate %.3f / Radius %.2f -> %.2f", status.c_str(), currentRate, firstRadius, secondRadius);
}

void StageCorruptionComponent::Serialize(nlohmann::json& json_) const
{
    json_ = { {"BarrierName", barrierName}, {"FirstParticleName", firstParticleName},
        {"SecondParticleName", secondParticleName}, {"TargetBarrierAssetPath", targetBarrierAssetPath},
        {"CameraObjectName", cameraObjectName}, {"TargetCameraAssetPath", targetCameraAssetPath}, {"CurveName", curveName}, {"Duration", duration}, {"SmoothTransition", smoothTransition},
        {"TargetColor", {targetColor.x, targetColor.y, targetColor.z, targetColor.w}},
        {"TargetEdgeColor", {targetEdgeColor.x, targetEdgeColor.y, targetEdgeColor.z, targetEdgeColor.w}} };
}

void StageCorruptionComponent::Deserialize(const nlohmann::json& json_)
{
    barrierName = json_.value("BarrierName", std::string("Wall"));
    firstParticleName = json_.value("FirstParticleName", std::string("Stage"));
    secondParticleName = json_.value("SecondParticleName", std::string("Phase2Environment"));
    targetBarrierAssetPath = json_.value("TargetBarrierAssetPath", std::string("Data/Scenes/HumanBossTest/Objects/Wall.json"));
    curveName = json_.value("CurveName", std::string{});
    cameraObjectName = json_.value("CameraObjectName", std::string("MainCamera"));
    targetCameraAssetPath = json_.value("TargetCameraAssetPath", std::string("Data/Scenes/HumanBossTest/Objects/MainCamera.json"));
    targetCameraProfile = nlohmann::json{};
    cameraProfileApplied = false;
    duration = (std::max)(json_.value("Duration", 4.0f), 0.0f);
    smoothTransition = json_.value("SmoothTransition", true);
    if (json_.contains("TargetColor") && json_["TargetColor"].is_array() && json_["TargetColor"].size() == 4)
    {
        targetColor = { json_["TargetColor"][0], json_["TargetColor"][1], json_["TargetColor"][2], json_["TargetColor"][3] };
    }
    if (json_.contains("TargetEdgeColor") && json_["TargetEdgeColor"].is_array() && json_["TargetEdgeColor"].size() == 4)
    {
        targetEdgeColor = { json_["TargetEdgeColor"][0], json_["TargetEdgeColor"][1], json_["TargetEdgeColor"][2], json_["TargetEdgeColor"][3] };
    }
    prepared = false;
    playing = false;
}

REGISTER_COMPONENT(StageCorruptionComponent, "StageCorruptionComponent")

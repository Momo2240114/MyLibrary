#include "PrecompiledHeader.h"

#include "DangerAreaComponent.h"

#include "Editor/AssetSlotGui.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
#include "Engine/GamePlay/Transform/Transform.h"
#include "Engine/Graphics/Resource/GlTFModel.h"
#include "Engine/Graphics/VFX/MeshVFXComponent.h"

#include <algorithm>
#include <cmath>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <nlohmann/json.hpp>

namespace
{
    constexpr const char* DANGER_AREA_MODEL_PATH = "__InternalMeshDangerArea__";
    constexpr const char* DANGER_FAN_MODEL_PATH = "__InternalMeshDangerFan__";

    DirectX::XMFLOAT3 ReadVector3(
        const nlohmann::json& json_,
        const char* key_,
        const DirectX::XMFLOAT3& fallback_)
    {
        // JSONのVector3は配列で保存し、壊れた形式なら既定値を維持する。
        const nlohmann::json value_ = json_.value(key_, nlohmann::json::array());
        if (!value_.is_array() || value_.size() < 3u) return fallback_;
        return { value_[0].get<float>(), value_[1].get<float>(), value_[2].get<float>() };
    }

    nlohmann::json WriteVector3(const DirectX::XMFLOAT3& value_)
    {
        return { value_.x, value_.y, value_.z };
    }

    DirectX::XMFLOAT3 ExtractPosition(const DirectX::XMMATRIX& matrix_)
    {
        DirectX::XMFLOAT3 position_{};
        DirectX::XMStoreFloat3(&position_, matrix_.r[3]);
        return position_;
    }

    const char* GetShapeTypeName(DangerAreaShapeType type_)
    {
        static const char* names_[] = { "Circle", "Capsule", "Fan" };
        return names_[std::clamp(static_cast<int>(type_), 0, 2)];
    }
}

void to_json(nlohmann::json& json_, const DangerAreaTemplate& template_)
{
    // 危険範囲の見た目、形状、発生基準、寿命をActionSequenceから呼べる単位で保存する。
    json_ =
    {
        { "Name", template_.name },
        { "ActionName", template_.actionName },
        { "ShapeType", static_cast<int>(template_.shapeType) },
        { "MaterialPath", template_.materialPath },
        { "SourceNodeName", template_.sourceNodeName },
        { "LocalOffset", WriteVector3(template_.localOffset) },
        { "LocalRotation", WriteVector3(template_.localRotation) },
        { "LocalScale", WriteVector3(template_.localScale) },
        { "Radius", template_.radius },
        { "Length", template_.length },
        { "FanAngleDegrees", template_.fanAngleDegrees },
        { "LifeTime", template_.lifeTime },
        { "GroundOffset", template_.groundOffset },
        { "PlaybackSpeed", template_.playbackSpeed },
        { "ProjectToOwnerGround", template_.projectToOwnerGround },
        { "FollowOwner", template_.followOwner },
    };
}

void from_json(const nlohmann::json& json_, DangerAreaTemplate& template_)
{
    // 形状生成に必要な値は読み込み時に最低値へ丸め、エディタで壊れた設定を扱いやすくする。
    template_.name = json_.value("Name", std::string{ "DangerArea" });
    template_.actionName = json_.value("ActionName", std::string{ "DangerArea_Show" });
    template_.shapeType = static_cast<DangerAreaShapeType>(std::clamp(json_.value("ShapeType", 1), 0, 2));
    template_.materialPath = json_.value("MaterialPath", std::string{ "Data/VFXMaterials/EnemyDangerArea.vfxmat" });
    template_.sourceNodeName = json_.value("SourceNodeName", std::string{});
    template_.localOffset = ReadVector3(json_, "LocalOffset", template_.localOffset);
    template_.localRotation = ReadVector3(json_, "LocalRotation", template_.localRotation);
    template_.localScale = ReadVector3(json_, "LocalScale", template_.localScale);
    template_.radius = (std::max)(json_.value("Radius", 1.0f), 0.01f);
    template_.length = (std::max)(json_.value("Length", 4.0f), 0.0f);
    template_.fanAngleDegrees = std::clamp(json_.value("FanAngleDegrees", 60.0f), 0.1f, 360.0f);
    template_.lifeTime = (std::max)(json_.value("LifeTime", 1.0f), 0.01f);
    template_.groundOffset = json_.value("GroundOffset", 0.03f);
    template_.playbackSpeed = (std::max)(json_.value("PlaybackSpeed", 1.0f), 0.0f);
    template_.projectToOwnerGround = json_.value("ProjectToOwnerGround", true);
    template_.followOwner = json_.value("FollowOwner", false);
}

void DangerAreaComponent::Start()
{
    BS_PROFILE_SCOPE("DangerAreaComponent::Start");

    // 空のまま追加してもすぐ試せるよう、標準のカプセル予兆を1つ用意する。
    if (templates.empty())
    {
        templates.push_back(DangerAreaTemplate{});
    }
    RegisterActions();
}

void DangerAreaComponent::Update(float deltaTime_)
{
    BS_PROFILE_SCOPE("DangerAreaComponent::Update");

    const float safeDeltaTime_ = (std::max)(deltaTime_, 0.0f);
    for (DangerAreaInstance& instance_ : instances)
    {
        instance_.lifeTimer -= safeDeltaTime_;
        if (!instance_.fixedPosition && instance_.templateIndex < templates.size() && templates[instance_.templateIndex].followOwner)
        {
            const DangerAreaTemplate& template_ = templates[instance_.templateIndex];
            ApplyInstanceTransform(template_, BuildEmitFrame(template_), instance_.object);
        }
    }

    instances.erase(
        std::remove_if(
            instances.begin(),
            instances.end(),
            [this](DangerAreaInstance& instance_)
            {
                if (instance_.lifeTimer > 0.0f) return false;
                StopInstance(instance_);
                return true;
            }),
        instances.end());
}

void DangerAreaComponent::RegisterActions()
{
    BS_PROFILE_SCOPE("DangerAreaComponent::RegisterActions");

    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;

    // Index、ActionName、Nameの3種類を公開し、ActionSequenceから呼び出しやすくする。
    for (size_t index_ = 0; index_ < templates.size(); ++index_)
    {
        const std::string indexActionName_ = "DangerArea_" + std::to_string(index_);
        owner_->RegisterAction("DangerArea", indexActionName_, [this, index_]() { Show(index_); });

        if (!templates[index_].actionName.empty())
        {
            owner_->RegisterAction("DangerArea", templates[index_].actionName, [this, index_]() { Show(index_); });
        }
        if (!templates[index_].name.empty())
        {
            owner_->RegisterAction("DangerArea", templates[index_].name, [this, index_]() { Show(index_); });
        }
    }
}

void DangerAreaComponent::Show(size_t templateIndex_)
{
    BS_PROFILE_SCOPE("DangerAreaComponent::Show");

    if (templateIndex_ >= templates.size()) return;
    const DangerAreaTemplate& template_ = templates[templateIndex_];
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;

    std::shared_ptr<GameObject> object_ = GetManager().Create("RuntimeDangerArea");
    if (!object_) return;
    object_->SetNoSaveObject();

    std::shared_ptr<MeshVFXComponent> meshVFX_ = object_->AddComponent<MeshVFXComponent>();
    if (!meshVFX_)
    {
        GetManager().Destroy(object_);
        return;
    }

    meshVFX_->SetAsset(BuildDangerAreaAsset(template_));
    meshVFX_->SetPlaybackSpeed(template_.playbackSpeed);
    meshVFX_->Reset();
    meshVFX_->Play();
    ApplyInstanceTransform(template_, BuildEmitFrame(template_), object_);

    DangerAreaInstance instance_{};
    instance_.object = object_;
    instance_.meshVFX = meshVFX_;
    instance_.templateIndex = templateIndex_;
    instance_.lifeTimer = template_.lifeTime;
    instances.push_back(instance_);
}

std::shared_ptr<GameObject> DangerAreaComponent::ShowAt(const DangerAreaTemplate& template_, const DirectX::XMFLOAT3& position_, float yawDegrees_)
{
    BS_PROFILE_SCOPE("DangerAreaComponent::ShowAt");
    const auto owner_ = GetGameObject();
    if (!owner_) return nullptr;
    const auto object_ = GetManager().Create("RuntimeDangerArea");
    if (!object_) return nullptr;
    object_->SetNoSaveObject();
    const auto mesh_ = object_->AddComponent<MeshVFXComponent>();
    if (!mesh_) { GetManager().Destroy(object_); return nullptr; }
    auto asset_ = BuildDangerAreaAsset(template_);
    // 確定地点の予兆では残り時間で色を変え、所有者が消えても表示を終了する。
    for (auto& part_ : asset_.parts) { part_.duration = template_.lifeTime; part_.loop = false; }
    mesh_->SetAsset(asset_);
    mesh_->SetPlaybackSpeed(template_.playbackSpeed);
    mesh_->Reset();
    mesh_->Play();
    object_->transform->ApplyWorldMatrix(DirectX::XMMatrixRotationY(DirectX::XMConvertToRadians(yawDegrees_)) * DirectX::XMMatrixTranslation(position_.x,
        position_.y + template_.groundOffset, position_.z));
    object_->UpdateTransform();
    DangerAreaInstance instance_;
    instance_.object = object_;
    instance_.meshVFX = mesh_;
    // Templateの追従更新から外し、予兆開始後の着地点を固定する。
    instance_.templateIndex = templates.size();
    instance_.groundOffset = template_.groundOffset;
    instance_.fixedPosition = true;
    instance_.lifeTimer = template_.lifeTime;
    instances.push_back(instance_);
    return object_;
}

void DangerAreaComponent::UpdateAt(const std::shared_ptr<GameObject>& object_,
    const DirectX::XMFLOAT3& position_, float yawDegrees_, float lengthScale_)
{
    BS_PROFILE_SCOPE("DangerAreaComponent::UpdateAt");
    const auto found_ = std::find_if(instances.begin(), instances.end(),
        [&object_](const DangerAreaInstance& instance_) { return instance_.object == object_; });
    if (found_ == instances.end() || !object_ || !object_->transform) return;
    object_->transform->ApplyWorldMatrix(DirectX::XMMatrixScaling(1.0f, 1.0f, (std::max)(0.001f, lengthScale_)) *
        DirectX::XMMatrixRotationY(DirectX::XMConvertToRadians(yawDegrees_)) *
        DirectX::XMMatrixTranslation(position_.x, position_.y + found_->groundOffset, position_.z));
    object_->UpdateTransform();
}

void DangerAreaComponent::StopAll()
{
    BS_PROFILE_SCOPE("DangerAreaComponent::StopAll");
    for (auto& instance_ : instances) StopInstance(instance_);
    instances.clear();
}

void DangerAreaComponent::ShowByName(const std::string& templateName_)
{
    for (size_t index_ = 0; index_ < templates.size(); ++index_)
    {
        if (templates[index_].name == templateName_ ||
            templates[index_].actionName == templateName_)
        {
            Show(index_);
            return;
        }
    }
}

DangerAreaComponent::EmitFrame DangerAreaComponent::BuildEmitFrame(
    const DangerAreaTemplate& template_) const
{
    const std::shared_ptr<GameObject> owner_ = gameObject.lock();
    if (!owner_ || !owner_->transform) return {};

    DirectX::XMMATRIX sourceMatrix_ = owner_->transform->GetWorldMatrix();
    if (!template_.sourceNodeName.empty())
    {
        std::shared_ptr<GltfModelCommponennt> model_ = owner_->GetComponent<GltfModelCommponennt>();
        if (model_ && model_->IsModelReady())
        {
            const int nodeIndex_ = model_->GetNodeIndex(template_.sourceNodeName.c_str());
            if (nodeIndex_ >= 0 && nodeIndex_ < static_cast<int>(model_->GetNodes().size()))
            {
                // ボーン名が指定されている場合は、そのワールド姿勢を予兆の発生基準に使う。
                sourceMatrix_ = DirectX::XMLoadFloat4x4(&model_->GetNodes()[nodeIndex_].worldTransform);
            }
        }
    }

    const DirectX::XMMATRIX localMatrix_ = BuildLocalMatrix(
        template_.localOffset,
        template_.localRotation,
        template_.localScale);

    EmitFrame frame_{};
    frame_.sourceMatrix = localMatrix_ * sourceMatrix_;
    frame_.position = ExtractPosition(frame_.sourceMatrix);
    return frame_;
}

DirectX::XMMATRIX DangerAreaComponent::BuildLocalMatrix(
    const DirectX::XMFLOAT3& position_,
    const DirectX::XMFLOAT3& rotation_,
    const DirectX::XMFLOAT3& scale_) const
{
    return DirectX::XMMatrixScaling(scale_.x, scale_.y, scale_.z) *
        DirectX::XMMatrixRotationRollPitchYaw(
            DirectX::XMConvertToRadians(rotation_.x),
            DirectX::XMConvertToRadians(rotation_.y),
            DirectX::XMConvertToRadians(rotation_.z)) *
        DirectX::XMMatrixTranslation(position_.x, position_.y, position_.z);
}

MeshVFXAsset DangerAreaComponent::BuildDangerAreaAsset(const DangerAreaTemplate& template_) const
{
    MeshVFXAsset asset_{};
    MeshVFXPart part_{};
    part_.renderEnabled = true;
    part_.modelPath = template_.shapeType == DangerAreaShapeType::FAN
        ? DANGER_FAN_MODEL_PATH
        : DANGER_AREA_MODEL_PATH;
    part_.materialPath = template_.materialPath;
    if (!template_.materialPath.empty())
    {
        part_.materialPaths.push_back(template_.materialPath);
    }
    part_.meshIndex = 0;
    part_.shapeCylinderRadius = template_.shapeType == DangerAreaShapeType::FAN
        ? (std::max)(template_.length, 0.01f)
        : (std::max)(template_.radius, 0.01f);
    part_.internalMeshSettings.capsuleLength = template_.shapeType == DangerAreaShapeType::CIRCLE
        ? 0.0f
        : (std::max)(template_.length, 0.0f);
    part_.internalMeshSettings.endAngleDegrees = template_.fanAngleDegrees;
    part_.duration = 0.0f;
    part_.loop = true;
    part_.blendMode = MeshVFXBlendMode::ALPHA;
    part_.useMaterialBaseTexture = true;
    part_.useMaterialNormalTexture = true;
    part_.baseSamplerMode = MESH_VFX_SAMPLER_MODE::CLAMP;
    part_.maskSamplerMode = MESH_VFX_SAMPLER_MODE::CLAMP;
    part_.normalSamplerMode = MESH_VFX_SAMPLER_MODE::CLAMP;
    part_.color = { 1.0f, 1.0f, 1.0f, 1.0f };
    part_.alpha = 1.0f;
    asset_.parts.push_back(part_);
    return asset_;
}

void DangerAreaComponent::ApplyInstanceTransform(
    const DangerAreaTemplate& template_,
    const EmitFrame& frame_,
    const std::shared_ptr<GameObject>& object_) const
{
    if (!object_ || !object_->transform) return;

    object_->transform->ApplyWorldMatrix(frame_.sourceMatrix);
    if (template_.projectToOwnerGround)
    {
        const std::shared_ptr<GameObject> owner_ = gameObject.lock();
        if (owner_ && owner_->transform)
        {
            // XZと回転は発生元、YだけOwnerの足元へ寄せて地面予兆として見せる。
            object_->transform->localPosition.x = frame_.position.x;
            object_->transform->localPosition.y = owner_->transform->GetWorldPosition().y + template_.groundOffset;
            object_->transform->localPosition.z = frame_.position.z;
        }
    }
    else
    {
        object_->transform->localPosition.y += template_.groundOffset;
    }
    object_->transform->MarkDirty();
    object_->UpdateTransform();
}

void DangerAreaComponent::StopInstance(DangerAreaInstance& instance_)
{
    if (std::shared_ptr<MeshVFXComponent> meshVFX_ = instance_.meshVFX.lock())
    {
        meshVFX_->Stop();
    }
    if (instance_.object)
    {
        GetManager().Destroy(instance_.object);
    }
    instance_.object.reset();
}

void DangerAreaComponent::Gui()
{
    BS_PROFILE_SCOPE("DangerAreaComponent::Gui");

    if (ImGui::Button("Register Actions")) RegisterActions();
    ImGui::SameLine();
    if (ImGui::Button("Add Template"))
    {
        DangerAreaTemplate template_{};
        template_.name = "DangerArea" + std::to_string(templates.size());
        template_.actionName = "DangerArea_" + std::to_string(templates.size());
        templates.push_back(template_);
        RegisterActions();
    }
    ImGui::Text("Runtime Danger Areas: %d", static_cast<int>(instances.size()));

    int removeTemplateIndex_ = -1;
    static const char* shapeNames_[] = { "Circle", "Capsule", "Fan" };
    for (int templateIndex_ = 0; templateIndex_ < static_cast<int>(templates.size()); ++templateIndex_)
    {
        DangerAreaTemplate& template_ = templates[templateIndex_];
        ImGui::PushID(templateIndex_);
        if (ImGui::TreeNode("Danger Area", "%d: %s [%s]", templateIndex_, template_.name.c_str(), GetShapeTypeName(template_.shapeType)))
        {
            if (ImGui::Button("Preview")) Show(static_cast<size_t>(templateIndex_));
            ImGui::SameLine();
            if (ImGui::Button("Remove")) removeTemplateIndex_ = templateIndex_;

            ImGui::InputText("Name", &template_.name);
            if (ImGui::InputText("Action Name", &template_.actionName)) RegisterActions();
            int shapeType_ = static_cast<int>(template_.shapeType);
            if (ImGui::Combo("Shape Type", &shapeType_, shapeNames_, IM_ARRAYSIZE(shapeNames_)))
            {
                template_.shapeType = static_cast<DangerAreaShapeType>(shapeType_);
            }
            AssetSlotGui::Draw("VFX Material", AssetSlotType::MATERIAL, template_.materialPath, nullptr, "Data/VFXMaterials");
            ImGui::InputText("Source Node Name", &template_.sourceNodeName);
            ImGui::DragFloat3("Local Offset", &template_.localOffset.x, 0.01f);
            ImGui::DragFloat3("Local Rotation", &template_.localRotation.x, 0.1f);
            ImGui::DragFloat3("Local Scale", &template_.localScale.x, 0.01f, 0.001f, 100.0f);
            ImGui::DragFloat("Radius", &template_.radius, 0.01f, 0.01f, 100.0f);
            ImGui::DragFloat("Length / Range", &template_.length, 0.05f, 0.0f, 1000.0f);
            if (template_.shapeType == DangerAreaShapeType::FAN)
            {
                ImGui::DragFloat("Fan Angle", &template_.fanAngleDegrees, 0.5f, 0.1f, 360.0f);
            }
            ImGui::DragFloat("Life Time", &template_.lifeTime, 0.01f, 0.01f, 120.0f);
            ImGui::DragFloat("Ground Offset", &template_.groundOffset, 0.001f, -10.0f, 10.0f);
            ImGui::DragFloat("Playback Speed", &template_.playbackSpeed, 0.01f, 0.0f, 10.0f);
            ImGui::Checkbox("Project To Owner Ground", &template_.projectToOwnerGround);
            ImGui::Checkbox("Follow Owner", &template_.followOwner);
            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    if (removeTemplateIndex_ >= 0)
    {
        templates.erase(templates.begin() + removeTemplateIndex_);
        RegisterActions();
    }
}

void DangerAreaComponent::Serialize(nlohmann::json& json_) const
{
    // 複数の危険範囲テンプレートをScene JSONへ保存する。
    json_["ComponentName"] = componentName;
    json_["Templates"] = templates;
}

void DangerAreaComponent::Deserialize(const nlohmann::json& json_)
{
    templates = json_.value("Templates", std::vector<DangerAreaTemplate>{});
    if (templates.empty())
    {
        templates.push_back(DangerAreaTemplate{});
    }
    RegisterActions();
}

REGISTER_COMPONENT(DangerAreaComponent, "DangerAreaComponent")

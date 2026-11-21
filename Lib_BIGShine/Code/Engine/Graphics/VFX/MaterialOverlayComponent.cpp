#include "PrecompiledHeader.h"

#include "MaterialOverlayComponent.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/Graphics/Resource/GLTFModel.h"

#include <imgui.h>
#include <nlohmann/json.hpp>

#include <algorithm>

MaterialOverlayComponent::MaterialOverlayComponent()
{
    // MaterialOverlayは所有モデルの上に重ねる用途なので、まず1つのOverlay用Partを用意する。
    MeshVFXPart part_;
    part_.renderEnabled = true;
    part_.useMaterialBaseTexture = true;
    part_.useMaterialNormalTexture = true;
    GetAsset().parts.push_back(part_);
}

void MaterialOverlayComponent::Start()
{
    MeshVFXComponent::Start();
    RegisterActions();
}

void MaterialOverlayComponent::StartOverlay()
{
    // 再生開始時はMeshVFXの時間も戻し、FadeInとMaterialアニメーションを同時に始める。
    overlayActive = true;
    Reset();
    Play();

    if (fadeInDuration <= 0.0f)
    {
        overlayWeight = 1.0f;
    }
}

void MaterialOverlayComponent::StopOverlay()
{
    // Stopは即停止ではなく、FadeOut後にMeshVFX自体を止める。
    overlayActive = false;
    if (fadeOutDuration <= 0.0f)
    {
        overlayWeight = 0.0f;
        Stop();
    }
}

void MaterialOverlayComponent::ResetOverlay()
{
    Reset();
}

void MaterialOverlayComponent::Update(float deltaTime_)
{
    // 対象モデルがロード中の間は、Overlayの時間だけ進んで見た目がずれないよう止める。
    if (!IsLoadReady())
    {
        return;
    }

    MeshVFXComponent::Update(deltaTime_);

    const float targetWeight_ = overlayActive ? 1.0f : 0.0f;
    const float duration_ = overlayActive ? fadeInDuration : fadeOutDuration;
    if (duration_ <= 0.0f)
    {
        overlayWeight = targetWeight_;
    }
    else
    {
        const float step_ = (std::max)(deltaTime_, 0.0f) / duration_;
        if (overlayWeight < targetWeight_)
        {
            overlayWeight = (std::min)(overlayWeight + step_, targetWeight_);
        }
        else if (overlayWeight > targetWeight_)
        {
            overlayWeight = (std::max)(overlayWeight - step_, targetWeight_);
        }
    }

    if (!overlayActive && overlayWeight <= 0.0f)
    {
        // 完全に透明になったらMeshVFXComponent側の再生も止め、描画対象から外す。
        Stop();
    }
}

bool MaterialOverlayComponent::IsLoadReady() const
{
    // Overlayは所有モデルのMesh情報を参照するため、モデルロード完了まで待つ。
    const std::shared_ptr<GameObject> owner_ =
        const_cast<MaterialOverlayComponent*>(this)->GetGameObject();
    const std::shared_ptr<GltfModelCommponennt> model_ =
        owner_ ? owner_->GetComponent<GltfModelCommponennt>() : nullptr;
    if (!model_)
    {
        return true;
    }

    model_->UpdateLoadState();
    return !model_->IsModelLoading();
}

float MaterialOverlayComponent::GetOverlayWeight() const
{
    // LinearなweightをSmoothStepへ変換し、出入りの見た目を少し柔らかくする。
    const float weight_ = std::clamp(overlayWeight, 0.0f, 1.0f);
    return weight_ * weight_ * (3.0f - 2.0f * weight_);
}

void MaterialOverlayComponent::RegisterActions()
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_)
    {
        return;
    }

    // ActionSequenceから、攻撃中だけ発光/縁取りなどを重ねられるようにする。
    owner_->RegisterAction("MaterialOverlay", "MaterialOverlay_Start", [this]()
        {
            StartOverlay();
        });
    owner_->RegisterAction("MaterialOverlay", "MaterialOverlay_Stop", [this]()
        {
            StopOverlay();
        });
    owner_->RegisterAction("MaterialOverlay", "MaterialOverlay_Reset", [this]()
        {
            ResetOverlay();
        });
}

void MaterialOverlayComponent::Gui()
{
    MeshVFXComponent::Gui();

    ImGui::SeparatorText("Material Overlay");
    bool active_ = overlayActive;
    if (ImGui::Checkbox("Overlay Active", &active_))
    {
        active_ ? StartOverlay() : StopOverlay();
    }
    ImGui::ProgressBar(GetOverlayWeight(), ImVec2(-1.0f, 0.0f), "Overlay Weight");
    ImGui::DragFloat("Fade In Duration", &fadeInDuration, 0.01f, 0.0f, 5.0f, "%.2f sec");
    ImGui::DragFloat("Fade Out Duration", &fadeOutDuration, 0.01f, 0.0f, 5.0f, "%.2f sec");
    ImGui::TextUnformatted("Actions: MaterialOverlay_Start / Stop / Reset");
    ImGui::TextUnformatted("Mesh -1 applies as the fallback for every mesh.");

    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    const std::shared_ptr<GltfModelCommponennt> model_ =
        owner_ ? owner_->GetComponent<GltfModelCommponennt>() : nullptr;
    if (!model_)
    {
        ImGui::TextDisabled("Owner model is not available.");
        return;
    }
    model_->UpdateLoadState();
    if (!model_->IsModelReady())
    {
        ImGui::TextDisabled("Owner model is loading.");
        return;
    }

    const auto& meshes_ = model_->GetMeshes();
    for (int partIndex_ = 0; partIndex_ < static_cast<int>(GetAsset().parts.size()); ++partIndex_)
    {
        // Partごとに適用対象Meshを選べる。-1は全Meshに対するFallbackとして扱う。
        MeshVFXPart& part_ = GetAsset().parts[partIndex_];
        ImGui::PushID(partIndex_);

        std::string preview_ = "All Meshes (Fallback)";
        if (part_.meshIndex >= 0 && part_.meshIndex < static_cast<int>(meshes_.size()))
        {
            const auto& mesh_ = meshes_[part_.meshIndex];
            const std::string nodeName_ = mesh_.node ? mesh_.node->name : "No Node";
            const std::string materialName_ = mesh_.material ? mesh_.material->name : "No Material";
            preview_ = std::to_string(part_.meshIndex) + ": " + nodeName_ + " / " + materialName_;
        }

        const std::string label_ = "Part " + std::to_string(partIndex_) + " Target";
        if (ImGui::BeginCombo(label_.c_str(), preview_.c_str()))
        {
            if (ImGui::Selectable("All Meshes (Fallback)", part_.meshIndex == -1))
            {
                part_.meshIndex = -1;
            }
            for (int meshIndex_ = 0; meshIndex_ < static_cast<int>(meshes_.size()); ++meshIndex_)
            {
                const auto& mesh_ = meshes_[meshIndex_];
                const std::string nodeName_ = mesh_.node ? mesh_.node->name : "No Node";
                const std::string materialName_ = mesh_.material ? mesh_.material->name : "No Material";
                const std::string meshLabel_ =
                    std::to_string(meshIndex_) + ": " + nodeName_ + " / " + materialName_;
                if (ImGui::Selectable(meshLabel_.c_str(), part_.meshIndex == meshIndex_))
                {
                    part_.meshIndex = meshIndex_;
                }
            }
            ImGui::EndCombo();
        }

        ImGui::PopID();
    }

    if (ImGui::Button("Register Overlay Actions"))
    {
        RegisterActions();
    }
}

void MaterialOverlayComponent::Serialize(nlohmann::json& json_) const
{
    MeshVFXComponent::Serialize(json_);
    json_["OverlayActive"] = overlayActive;
    json_["FadeInDuration"] = fadeInDuration;
    json_["FadeOutDuration"] = fadeOutDuration;
}

void MaterialOverlayComponent::Deserialize(const nlohmann::json& json_)
{
    MeshVFXComponent::Deserialize(json_);
    // 保存時にOverlay中だった場合は、ロード直後から表示状態を復元する。
    overlayActive = json_.value("OverlayActive", false);
    overlayWeight = overlayActive ? 1.0f : 0.0f;
    fadeInDuration = (std::max)(json_.value("FadeInDuration", 0.08f), 0.0f);
    fadeOutDuration = (std::max)(json_.value("FadeOutDuration", 0.25f), 0.0f);
}

#include "PrecompiledHeader.h"

#include "UIVertexModifierComponent.h"

#include "Engine/Debug/TracyProfiler.h"
#include "imgui.h"

namespace
{
    const char* UI_VERTEX_NAMES[] =
    {
        "LEFT_TOP",
        "RIGHT_TOP",
        "LEFT_BOTTOM",
        "RIGHT_BOTTOM"
    };

    size_t ToIndex(UI_VERTEX_INDEX vertexIndex_)
    {
        const size_t index_ = static_cast<size_t>(vertexIndex_);
        if (index_ >= static_cast<size_t>(UI_VERTEX_INDEX::MAX))
        {
            return 0;
        }
        return index_;
    }

    void SerializeFloat2(nlohmann::json& json_, const char* key_, const DirectX::XMFLOAT2& value_)
    {
        json_[key_] = { value_.x, value_.y };
    }

    void SerializeFloat4(nlohmann::json& json_, const char* key_, const DirectX::XMFLOAT4& value_)
    {
        json_[key_] = { value_.x, value_.y, value_.z, value_.w };
    }

    void DeserializeFloat2(const nlohmann::json& json_, const char* key_, DirectX::XMFLOAT2& value_)
    {
        if (!json_.contains(key_) || !json_[key_].is_array() || json_[key_].size() < 2) return;
        value_.x = json_[key_][0].get<float>();
        value_.y = json_[key_][1].get<float>();
    }

    void DeserializeFloat4(const nlohmann::json& json_, const char* key_, DirectX::XMFLOAT4& value_)
    {
        if (!json_.contains(key_) || !json_[key_].is_array() || json_[key_].size() < 4) return;
        value_.x = json_[key_][0].get<float>();
        value_.y = json_[key_][1].get<float>();
        value_.z = json_[key_][2].get<float>();
        value_.w = json_[key_][3].get<float>();
    }
}

void UIVertexModifierComponent::Gui()
{
    BS_PROFILE_SCOPE("UIVertexModifierComponent::Gui");

    for (int index_ = 0; index_ < static_cast<int>(UI_VERTEX_INDEX::MAX); ++index_)
    {
        UIVertexModification& modification_ = vertexModifications[static_cast<size_t>(index_)];
        if (ImGui::TreeNode(UI_VERTEX_NAMES[index_]))
        {
            ImGui::DragFloat2("Position Offset", &modification_.positionOffset.x, 1.0f);
            ImGui::DragFloat2("UV Offset", &modification_.uvOffset.x, 0.001f);
            ImGui::ColorEdit4("Color Multiplier", &modification_.colorMultiplier.x);
            ImGui::TreePop();
        }
    }

    if (ImGui::Button("Reset Vertex Modifications"))
    {
        ResetAllModifications();
    }
}

void UIVertexModifierComponent::Serialize(nlohmann::json& json_) const
{
    BS_PROFILE_SCOPE("UIVertexModifierComponent::Serialize");

    json_["Version"] = 1;
    nlohmann::json verticesJson_;
    for (int index_ = 0; index_ < static_cast<int>(UI_VERTEX_INDEX::MAX); ++index_)
    {
        const UIVertexModification& modification_ = vertexModifications[static_cast<size_t>(index_)];
        nlohmann::json vertexJson_;
        SerializeFloat2(vertexJson_, "PositionOffset", modification_.positionOffset);
        SerializeFloat2(vertexJson_, "UVOffset", modification_.uvOffset);
        SerializeFloat4(vertexJson_, "ColorMultiplier", modification_.colorMultiplier);
        verticesJson_[UI_VERTEX_NAMES[index_]] = vertexJson_;
    }
    json_["Vertices"] = verticesJson_;
}

void UIVertexModifierComponent::Deserialize(const nlohmann::json& json_)
{
    BS_PROFILE_SCOPE("UIVertexModifierComponent::Deserialize");

    if (!json_.contains("Vertices") || !json_["Vertices"].is_object()) return;

    const nlohmann::json& verticesJson_ = json_["Vertices"];
    for (int index_ = 0; index_ < static_cast<int>(UI_VERTEX_INDEX::MAX); ++index_)
    {
        if (!verticesJson_.contains(UI_VERTEX_NAMES[index_])) continue;

        UIVertexModification& modification_ = vertexModifications[static_cast<size_t>(index_)];
        const nlohmann::json& vertexJson_ = verticesJson_[UI_VERTEX_NAMES[index_]];
        DeserializeFloat2(vertexJson_, "PositionOffset", modification_.positionOffset);
        DeserializeFloat2(vertexJson_, "UVOffset", modification_.uvOffset);
        DeserializeFloat4(vertexJson_, "ColorMultiplier", modification_.colorMultiplier);
    }
}

const UIVertexModification& UIVertexModifierComponent::GetVertexModification(UI_VERTEX_INDEX vertexIndex_) const
{
    return vertexModifications[ToIndex(vertexIndex_)];
}

UIVertexModification& UIVertexModifierComponent::GetMutableVertexModification(UI_VERTEX_INDEX vertexIndex_)
{
    return vertexModifications[ToIndex(vertexIndex_)];
}

void UIVertexModifierComponent::SetVertexPositionOffset(
    UI_VERTEX_INDEX vertexIndex_,
    const DirectX::XMFLOAT2& positionOffset_)
{
    vertexModifications[ToIndex(vertexIndex_)].positionOffset = positionOffset_;
}

void UIVertexModifierComponent::ResetAllModifications()
{
    for (UIVertexModification& modification_ : vertexModifications)
    {
        modification_ = UIVertexModification{};
    }
}

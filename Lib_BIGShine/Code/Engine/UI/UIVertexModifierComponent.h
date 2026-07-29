#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"

#include <DirectXMath.h>
#include <array>
#include <string>

enum class UI_VERTEX_INDEX
{
    LEFT_TOP,
    RIGHT_TOP,
    LEFT_BOTTOM,
    RIGHT_BOTTOM,
    MAX
};

struct UIVertexModification
{
    DirectX::XMFLOAT2 positionOffset = { 0.0f, 0.0f };
    DirectX::XMFLOAT2 uvOffset = { 0.0f, 0.0f };
    DirectX::XMFLOAT4 colorMultiplier = { 1.0f, 1.0f, 1.0f, 1.0f };
};

class UIVertexModifierComponent : public Component
{
public:
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    const std::string& GetName() const override
    {
        static std::string name = "UIVertexModifierComponent";
        return name;
    }

    const UIVertexModification& GetVertexModification(UI_VERTEX_INDEX vertexIndex_) const;
    UIVertexModification& GetMutableVertexModification(UI_VERTEX_INDEX vertexIndex_);
    void SetVertexPositionOffset(UI_VERTEX_INDEX vertexIndex_, const DirectX::XMFLOAT2& positionOffset_);
    void ResetAllModifications();

private:
    std::array<UIVertexModification, static_cast<size_t>(UI_VERTEX_INDEX::MAX)> vertexModifications;
};

#include "PrecompiledHeader.h"

#include "NumberSwitcher.h"
#include "Engine/Graphics/Resource/Sprite.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include <algorithm>

void NumberSwitcher::Start()
{
    auto gameObject_ = GetGameObject();
    if (!gameObject_) return;

    spriteRenderer = gameObject_->GetComponent<SpriteRenderer>();
    SetDigit(currentDigit);
}

void NumberSwitcher::SetDigit(int num_)
{
    if (maxColumns < 1) maxColumns = 1;
    if (maxRows < 1) maxRows = 1;

    const int maxIndex_ = (maxColumns * maxRows) - 1;
    currentDigit = std::clamp(num_, 0, maxIndex_);

    auto sr_ = spriteRenderer.lock();
    if (!sr_) return;

    sr_->SetUVGridIndex(currentDigit, maxColumns, digitSize);
}

void NumberSwitcher::Gui() 
{
    if (ImGui::InputInt("Current Digit", &currentDigit)) {
        SetDigit(currentDigit);
    }

    ImGui::Separator();
    ImGui::Text("Settings");

    if (ImGui::InputInt("Max Columns", &maxColumns)) {
        if (maxColumns < 1) maxColumns = 1;
        SetDigit(currentDigit);
    }

    if (ImGui::InputInt("Max Rows", &maxRows)) {
        if (maxRows < 1) maxRows = 1;
        SetDigit(currentDigit);
    }

    if (ImGui::DragFloat2("Digit Size", &digitSize.x, 1.0f)) {
        SetDigit(currentDigit);
    }
}

void NumberSwitcher::Serialize(nlohmann::json& j) const {
    j["CurrentDigit"] = currentDigit;
    j["MaxColumns"] = maxColumns;
    j["MaxRows"] = maxRows;
    j["DigitSize"] = { digitSize.x, digitSize.y };
}

void NumberSwitcher::Deserialize(const nlohmann::json& j)
{
    if (j.contains("CurrentDigit")) currentDigit = j["CurrentDigit"];
    if (j.contains("MaxColumns")) maxColumns = j["MaxColumns"];
    if (j.contains("MaxRows")) maxRows = j["MaxRows"];
    if (j.contains("DigitSize")) {
        digitSize.x = j["DigitSize"][0];
        digitSize.y = j["DigitSize"][1];
    }

    SetDigit(currentDigit);
}
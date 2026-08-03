#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"

#include <memory>
#include <string>

class SpriteRenderer;

class UISpriteSheetComponent : public Component
{
public:
    void Start() override;
    void Update(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    const std::string& GetName() const override
    {
        static std::string name_ = "UISpriteSheetComponent";
        return name_;
    }

    void ApplySlice();

private:
    void ResolveSpriteRenderer();
    void NormalizeSettings();

private:
    int columnCount = 1;
    int rowCount = 1;
    int columnIndex = 0;
    int rowIndex = 0;
    float paddingLeft = 0.0f;
    float paddingTop = 0.0f;
    float paddingRight = 0.0f;
    float paddingBottom = 0.0f;
    bool applyEveryFrame = true;

    std::weak_ptr<SpriteRenderer> spriteRenderer;
};

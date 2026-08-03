#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"

#include <DirectXMath.h>
#include <memory>
#include <string>
#include <vector>

class SpriteRenderer;
class UIWidgetComponent;

enum class UI_TEXT_HORIZONTAL_ALIGN
{
    LEFT,
    CENTER,
    RIGHT
};

enum class UI_TEXT_VERTICAL_ALIGN
{
    TOP,
    CENTER,
    BOTTOM
};

class UITextComponent : public Component
{
public:
    void Start() override;
    void Update(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    const std::string& GetName() const override
    {
        static std::string name_ = "UITextComponent";
        return name_;
    }

    void SetText(const std::string& text_);
    void MarkDirty();

private:
    void ResolveTargets();
    void RebuildTextTexture();
    bool CreateTextureFromPixels(
        const std::vector<unsigned char>& pixels_,
        int width_,
        int height_);
    std::wstring ConvertUtf8ToWide(const std::string& text_) const;
    std::wstring ResolveFontPath() const;
    void NormalizeSettings();

private:
    std::string text = "Text";
    std::string fontPath = "Data/Font/NotoSansJP-Regular.otf";
    float fontSize = 24.0f;
    DirectX::XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
    float maxWidth = 0.0f;
    float padding = 4.0f;
    bool autoSizeWidget = true;
    UI_TEXT_HORIZONTAL_ALIGN horizontalAlign = UI_TEXT_HORIZONTAL_ALIGN::CENTER;
    UI_TEXT_VERTICAL_ALIGN verticalAlign = UI_TEXT_VERTICAL_ALIGN::CENTER;
    bool dirty = true;

    std::weak_ptr<SpriteRenderer> spriteRenderer;
    std::weak_ptr<UIWidgetComponent> widget;
};

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
    // GDI+の横揃えへ変換して使う。
    LEFT,
    CENTER,
    RIGHT
};

enum class UI_TEXT_VERTICAL_ALIGN
{
    // GDI+の縦揃えへ変換して使う。
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

    // Textを変更し、次回UpdateでTextureを再生成する。
    void SetText(const std::string& text_);
    // Text Textureの再生成が必要な状態にする。
    void MarkDirty();

private:
    // 同じGameObject上のSpriteRenderer/UIWidgetを解決する。
    void ResolveTargets();
    // GDI+でTextをBitmap化し、SpriteRenderer用Textureへ作り直す。
    void RebuildTextTexture();
    // CPU上のPixel配列からD3D Texture/SRVを作成する。
    bool CreateTextureFromPixels(
        const std::vector<unsigned char>& pixels_,
        int width_,
        int height_);
    // UTF-8文字列をGDI+で扱うWide文字列へ変換する。
    std::wstring ConvertUtf8ToWide(const std::string& text_) const;
    // Font PathをWindows API/GDI+が扱えるWide Pathへ変換する。
    std::wstring ResolveFontPath() const;
    // Font SizeやPaddingなどを安全な範囲へ丸める。
    void NormalizeSettings();

private:
    // 描画するTextとFont設定。
    std::string text = "Text";
    std::string fontPath = "Data/Font/NotoSansJP-Regular.otf";
    float fontSize = 24.0f;
    DirectX::XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
    float maxWidth = 0.0f;
    float padding = 4.0f;
    // trueなら生成したText TextureのSizeにUIWidgetを合わせる。
    bool autoSizeWidget = true;
    UI_TEXT_HORIZONTAL_ALIGN horizontalAlign = UI_TEXT_HORIZONTAL_ALIGN::CENTER;
    UI_TEXT_VERTICAL_ALIGN verticalAlign = UI_TEXT_VERTICAL_ALIGN::CENTER;
    // trueならTexture再生成が必要。
    bool dirty = true;

    // 生成したTextureを表示するSpriteRendererと、Size反映先のWidget。
    std::weak_ptr<SpriteRenderer> spriteRenderer;
    std::weak_ptr<UIWidgetComponent> widget;
};

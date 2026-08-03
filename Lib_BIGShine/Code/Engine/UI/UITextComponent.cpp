#include "PrecompiledHeader.h"

#include "UITextComponent.h"

#include "Engine/Debug/TracyProfiler.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Graphics/Resource/Sprite.h"
#include "Engine/Platform/WindowsDialogHelper.h"
#include "Engine/UI/ImGuiColorEditor.h"
#include "Engine/UI/UIWidgetComponent.h"
#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <gdiplus.h>
#include <vector>

#pragma comment(lib, "Gdiplus.lib")

namespace
{
    const char* UI_TEXT_HORIZONTAL_ALIGN_NAMES[] =
    {
        "LEFT",
        "CENTER",
        "RIGHT"
    };

    const char* UI_TEXT_VERTICAL_ALIGN_NAMES[] =
    {
        "TOP",
        "CENTER",
        "BOTTOM"
    };

    constexpr int UI_TEXT_HORIZONTAL_ALIGN_COUNT =
        static_cast<int>(sizeof(UI_TEXT_HORIZONTAL_ALIGN_NAMES) / sizeof(UI_TEXT_HORIZONTAL_ALIGN_NAMES[0]));
    constexpr int UI_TEXT_VERTICAL_ALIGN_COUNT =
        static_cast<int>(sizeof(UI_TEXT_VERTICAL_ALIGN_NAMES) / sizeof(UI_TEXT_VERTICAL_ALIGN_NAMES[0]));

    const char* ToString(UI_TEXT_HORIZONTAL_ALIGN align_)
    {
        const int index_ = static_cast<int>(align_);
        if (index_ < 0 || index_ >= UI_TEXT_HORIZONTAL_ALIGN_COUNT)
        {
            return UI_TEXT_HORIZONTAL_ALIGN_NAMES[0];
        }
        return UI_TEXT_HORIZONTAL_ALIGN_NAMES[index_];
    }

    const char* ToString(UI_TEXT_VERTICAL_ALIGN align_)
    {
        const int index_ = static_cast<int>(align_);
        if (index_ < 0 || index_ >= UI_TEXT_VERTICAL_ALIGN_COUNT)
        {
            return UI_TEXT_VERTICAL_ALIGN_NAMES[0];
        }
        return UI_TEXT_VERTICAL_ALIGN_NAMES[index_];
    }

    UI_TEXT_HORIZONTAL_ALIGN HorizontalAlignFromString(const std::string& name_)
    {
        for (int index_ = 0; index_ < UI_TEXT_HORIZONTAL_ALIGN_COUNT; ++index_)
        {
            if (name_ == UI_TEXT_HORIZONTAL_ALIGN_NAMES[index_])
            {
                return static_cast<UI_TEXT_HORIZONTAL_ALIGN>(index_);
            }
        }
        return UI_TEXT_HORIZONTAL_ALIGN::LEFT;
    }

    UI_TEXT_VERTICAL_ALIGN VerticalAlignFromString(const std::string& name_)
    {
        for (int index_ = 0; index_ < UI_TEXT_VERTICAL_ALIGN_COUNT; ++index_)
        {
            if (name_ == UI_TEXT_VERTICAL_ALIGN_NAMES[index_])
            {
                return static_cast<UI_TEXT_VERTICAL_ALIGN>(index_);
            }
        }
        return UI_TEXT_VERTICAL_ALIGN::TOP;
    }

    Gdiplus::StringAlignment ToGdiHorizontalAlign(UI_TEXT_HORIZONTAL_ALIGN align_)
    {
        switch (align_)
        {
        case UI_TEXT_HORIZONTAL_ALIGN::CENTER:
            return Gdiplus::StringAlignmentCenter;
        case UI_TEXT_HORIZONTAL_ALIGN::RIGHT:
            return Gdiplus::StringAlignmentFar;
        case UI_TEXT_HORIZONTAL_ALIGN::LEFT:
        default:
            return Gdiplus::StringAlignmentNear;
        }
    }

    Gdiplus::StringAlignment ToGdiVerticalAlign(UI_TEXT_VERTICAL_ALIGN align_)
    {
        switch (align_)
        {
        case UI_TEXT_VERTICAL_ALIGN::CENTER:
            return Gdiplus::StringAlignmentCenter;
        case UI_TEXT_VERTICAL_ALIGN::BOTTOM:
            return Gdiplus::StringAlignmentFar;
        case UI_TEXT_VERTICAL_ALIGN::TOP:
        default:
            return Gdiplus::StringAlignmentNear;
        }
    }

    void DrawTextInput(const char* label_, std::string& value_, bool& changed_)
    {
        char buffer_[512]{};
        strncpy_s(buffer_, sizeof(buffer_), value_.c_str(), _TRUNCATE);
        if (ImGui::InputText(label_, buffer_, sizeof(buffer_)))
        {
            value_ = buffer_;
            changed_ = true;
        }
    }

    bool EnsureGdiplusStarted()
    {
        static ULONG_PTR gdiplusToken_{};
        static bool started_ = false;
        static bool failed_ = false;

        if (started_) return true;
        if (failed_) return false;

        Gdiplus::GdiplusStartupInput startupInput_{};
        if (Gdiplus::GdiplusStartup(&gdiplusToken_, &startupInput_, nullptr) != Gdiplus::Ok)
        {
            failed_ = true;
            return false;
        }

        started_ = true;
        return true;
    }
}

void UITextComponent::Start()
{
    BS_PROFILE_SCOPE("UITextComponent::Start");
    ResolveTargets();
    NormalizeSettings();
    RebuildTextTexture();
}

void UITextComponent::Update(float deltaTime_)
{
    BS_PROFILE_SCOPE("UITextComponent::Update");
    (void)deltaTime_;

    if (spriteRenderer.expired() || widget.expired())
    {
        ResolveTargets();
    }

    if (dirty)
    {
        RebuildTextTexture();
    }
}

void UITextComponent::Gui()
{
    BS_PROFILE_SCOPE("UITextComponent::Gui");

    bool changed_ = false;
    DrawTextInput("Text", text, changed_);
    DrawTextInput("Font Path", fontPath, changed_);
    changed_ |= ImGui::DragFloat("Font Size", &fontSize, 1.0f, 4.0f, 256.0f);
    changed_ |= ImGuiColorEditor::DrawColorEdit4("Color", &color.x);
    changed_ |= ImGui::DragFloat("Max Width", &maxWidth, 1.0f, 0.0f, 4096.0f);
    changed_ |= ImGui::DragFloat("Padding", &padding, 1.0f, 0.0f, 256.0f);
    changed_ |= ImGui::Checkbox("Auto Size Widget", &autoSizeWidget);

    int horizontalAlignIndex_ = static_cast<int>(horizontalAlign);
    if (ImGui::Combo(
        "Horizontal Align",
        &horizontalAlignIndex_,
        UI_TEXT_HORIZONTAL_ALIGN_NAMES,
        UI_TEXT_HORIZONTAL_ALIGN_COUNT))
    {
        horizontalAlign = static_cast<UI_TEXT_HORIZONTAL_ALIGN>(horizontalAlignIndex_);
        changed_ = true;
    }

    int verticalAlignIndex_ = static_cast<int>(verticalAlign);
    if (ImGui::Combo(
        "Vertical Align",
        &verticalAlignIndex_,
        UI_TEXT_VERTICAL_ALIGN_NAMES,
        UI_TEXT_VERTICAL_ALIGN_COUNT))
    {
        verticalAlign = static_cast<UI_TEXT_VERTICAL_ALIGN>(verticalAlignIndex_);
        changed_ = true;
    }

    if (changed_)
    {
        NormalizeSettings();
        MarkDirty();
    }

    if (ImGui::Button("Rebuild Text Texture"))
    {
        MarkDirty();
        RebuildTextTexture();
    }
}

void UITextComponent::Serialize(nlohmann::json& json_) const
{
    BS_PROFILE_SCOPE("UITextComponent::Serialize");

    json_["Version"] = 1;
    json_["Text"] = text;
    json_["FontPath"] = fontPath;
    json_["FontSize"] = fontSize;
    json_["Color"] = { color.x, color.y, color.z, color.w };
    json_["MaxWidth"] = maxWidth;
    json_["Padding"] = padding;
    json_["AutoSizeWidget"] = autoSizeWidget;
    json_["HorizontalAlign"] = ToString(horizontalAlign);
    json_["VerticalAlign"] = ToString(verticalAlign);
}

void UITextComponent::Deserialize(const nlohmann::json& json_)
{
    BS_PROFILE_SCOPE("UITextComponent::Deserialize");

    text = json_.value("Text", text);
    fontPath = json_.value("FontPath", fontPath);
    fontSize = json_.value("FontSize", fontSize);
    if (json_.contains("Color") && json_["Color"].is_array() && json_["Color"].size() >= 4)
    {
        color.x = json_["Color"][0].get<float>();
        color.y = json_["Color"][1].get<float>();
        color.z = json_["Color"][2].get<float>();
        color.w = json_["Color"][3].get<float>();
    }
    maxWidth = json_.value("MaxWidth", maxWidth);
    padding = json_.value("Padding", padding);
    autoSizeWidget = json_.value("AutoSizeWidget", autoSizeWidget);
    horizontalAlign = HorizontalAlignFromString(json_.value("HorizontalAlign", std::string(ToString(horizontalAlign))));
    verticalAlign = VerticalAlignFromString(json_.value("VerticalAlign", std::string(ToString(verticalAlign))));
    NormalizeSettings();
    MarkDirty();
}

void UITextComponent::SetText(const std::string& text_)
{
    if (text == text_) return;
    text = text_;
    MarkDirty();
}

void UITextComponent::MarkDirty()
{
    dirty = true;
}

void UITextComponent::ResolveTargets()
{
    BS_PROFILE_SCOPE("UITextComponent::ResolveTargets");

    if (const std::shared_ptr<GameObject> gameObject_ = GetGameObject())
    {
        spriteRenderer = gameObject_->GetOrAddComponent<SpriteRenderer>();
        widget = gameObject_->GetComponent<UIWidgetComponent>();
    }
}

void UITextComponent::RebuildTextTexture()
{
    BS_PROFILE_SCOPE("UITextComponent::RebuildTextTexture");

    NormalizeSettings();
    ResolveTargets();

    const std::shared_ptr<SpriteRenderer> spriteRenderer_ = spriteRenderer.lock();
    if (!spriteRenderer_) return;

    if (!EnsureGdiplusStarted())
    {
        return;
    }

    const std::wstring text_ = ConvertUtf8ToWide(text);
    const std::wstring fontPath_ = ResolveFontPath();

    Gdiplus::PrivateFontCollection fontCollection_;
    const bool loadedPrivateFont_ =
        !fontPath_.empty() &&
        fontCollection_.AddFontFile(fontPath_.c_str()) == Gdiplus::Ok &&
        fontCollection_.GetFamilyCount() > 0;

    Gdiplus::FontFamily privateFontFamily_;
    Gdiplus::FontFamily fallbackFontFamily_(L"Arial");
    const Gdiplus::FontFamily* fontFamily_ = &fallbackFontFamily_;
    if (loadedPrivateFont_)
    {
        int familyCount_ = 1;
        fontCollection_.GetFamilies(1, &privateFontFamily_, &familyCount_);
        fontFamily_ = &privateFontFamily_;
    }

    Gdiplus::Font font_(fontFamily_, fontSize, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    Gdiplus::StringFormat stringFormat_;
    stringFormat_.SetFormatFlags(Gdiplus::StringFormatFlagsNoClip);
    stringFormat_.SetAlignment(ToGdiHorizontalAlign(horizontalAlign));
    stringFormat_.SetLineAlignment(ToGdiVerticalAlign(verticalAlign));

    Gdiplus::Bitmap measureBitmap_(1, 1, PixelFormat32bppARGB);
    Gdiplus::Graphics measureGraphics_(&measureBitmap_);
    measureGraphics_.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);

    const float layoutWidth_ = maxWidth > 0.0f ? maxWidth : 4096.0f;
    Gdiplus::RectF measureLayout_(0.0f, 0.0f, layoutWidth_, 4096.0f);
    Gdiplus::RectF measuredBounds_{};
    measureGraphics_.MeasureString(
        text_.c_str(),
        -1,
        &font_,
        measureLayout_,
        &stringFormat_,
        &measuredBounds_);

    const int textureWidth_ = (std::max)(
        1,
        static_cast<int>(std::ceil((maxWidth > 0.0f ? maxWidth : measuredBounds_.Width) + padding * 2.0f)));
    const int textureHeight_ = (std::max)(
        1,
        static_cast<int>(std::ceil(measuredBounds_.Height + padding * 2.0f)));

    Gdiplus::Bitmap bitmap_(textureWidth_, textureHeight_, PixelFormat32bppARGB);
    Gdiplus::Graphics graphics_(&bitmap_);
    graphics_.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
    graphics_.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
    graphics_.Clear(Gdiplus::Color(0, 0, 0, 0));

    const BYTE alpha_ = static_cast<BYTE>(std::clamp(color.w, 0.0f, 1.0f) * 255.0f);
    const BYTE red_ = static_cast<BYTE>(std::clamp(color.x, 0.0f, 1.0f) * 255.0f);
    const BYTE green_ = static_cast<BYTE>(std::clamp(color.y, 0.0f, 1.0f) * 255.0f);
    const BYTE blue_ = static_cast<BYTE>(std::clamp(color.z, 0.0f, 1.0f) * 255.0f);
    Gdiplus::SolidBrush brush_(Gdiplus::Color(alpha_, red_, green_, blue_));
    Gdiplus::RectF drawLayout_(
        padding,
        padding,
        (std::max)(1.0f, static_cast<float>(textureWidth_) - padding * 2.0f),
        (std::max)(1.0f, static_cast<float>(textureHeight_) - padding * 2.0f));
    graphics_.DrawString(text_.c_str(), -1, &font_, drawLayout_, &stringFormat_, &brush_);

    Gdiplus::BitmapData bitmapData_{};
    Gdiplus::Rect lockRect_(0, 0, textureWidth_, textureHeight_);
    std::vector<unsigned char> pixels_(static_cast<size_t>(textureWidth_) * textureHeight_ * 4);
    if (bitmap_.LockBits(
        &lockRect_,
        Gdiplus::ImageLockModeRead,
        PixelFormat32bppARGB,
        &bitmapData_) == Gdiplus::Ok)
    {
        const unsigned char* sourcePixels_ = static_cast<const unsigned char*>(bitmapData_.Scan0);
        for (int row_ = 0; row_ < textureHeight_; ++row_)
        {
            const unsigned char* sourceRow_ = sourcePixels_ + static_cast<size_t>(row_) * bitmapData_.Stride;
            unsigned char* destinationRow_ = pixels_.data() + static_cast<size_t>(row_) * textureWidth_ * 4;
            memcpy(destinationRow_, sourceRow_, static_cast<size_t>(textureWidth_) * 4);
        }
        bitmap_.UnlockBits(&bitmapData_);
    }

    if (CreateTextureFromPixels(pixels_, textureWidth_, textureHeight_) && autoSizeWidget)
    {
        if (const std::shared_ptr<UIWidgetComponent> widget_ = widget.lock())
        {
            UIRectTransform& rectTransform_ = widget_->GetRectTransform();
            rectTransform_.sizeDelta = { static_cast<float>(textureWidth_), static_cast<float>(textureHeight_) };
            widget_->MarkLayoutDirty();
        }
    }

    dirty = false;
}

bool UITextComponent::CreateTextureFromPixels(
    const std::vector<unsigned char>& pixels_,
    int width_,
    int height_)
{
    BS_PROFILE_SCOPE("UITextComponent::CreateTextureFromPixels");

    ID3D11Device* device_ = GraphicsDeviceManager::Instance().GetDevice();
    if (!device_ || pixels_.empty() || width_ <= 0 || height_ <= 0) return false;

    D3D11_TEXTURE2D_DESC textureDesc_{};
    textureDesc_.Width = static_cast<UINT>(width_);
    textureDesc_.Height = static_cast<UINT>(height_);
    textureDesc_.MipLevels = 1;
    textureDesc_.ArraySize = 1;
    textureDesc_.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    textureDesc_.SampleDesc.Count = 1;
    textureDesc_.Usage = D3D11_USAGE_IMMUTABLE;
    textureDesc_.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA subresourceData_{};
    subresourceData_.pSysMem = pixels_.data();
    subresourceData_.SysMemPitch = static_cast<UINT>(width_) * 4;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture_;
    if (FAILED(device_->CreateTexture2D(&textureDesc_, &subresourceData_, texture_.GetAddressOf())))
    {
        return false;
    }

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shaderResourceView_;
    if (FAILED(device_->CreateShaderResourceView(texture_.Get(), nullptr, shaderResourceView_.GetAddressOf())))
    {
        return false;
    }

    if (const std::shared_ptr<SpriteRenderer> spriteRenderer_ = spriteRenderer.lock())
    {
        spriteRenderer_->SetTextureResource(shaderResourceView_, textureDesc_);
        spriteRenderer_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
    }
    return true;
}

std::wstring UITextComponent::ConvertUtf8ToWide(const std::string& text_) const
{
    if (text_.empty()) return {};

    const int length_ = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text_.c_str(), -1, nullptr, 0);
    if (length_ <= 0)
    {
        return std::wstring(text_.begin(), text_.end());
    }

    std::wstring result_(static_cast<size_t>(length_), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text_.c_str(), -1, result_.data(), length_);
    result_.pop_back();
    return result_;
}

std::wstring UITextComponent::ResolveFontPath() const
{
    if (fontPath.empty()) return {};
    return WindowsDialogHelper::Utf8ToPath(fontPath).wstring();
}

void UITextComponent::NormalizeSettings()
{
    fontSize = std::clamp(fontSize, 4.0f, 256.0f);
    maxWidth = (std::max)(0.0f, maxWidth);
    padding = std::clamp(padding, 0.0f, 256.0f);
}

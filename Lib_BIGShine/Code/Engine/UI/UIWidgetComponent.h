#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"

#include <DirectXMath.h>
#include <memory>
#include <string>

class WidgetBoardComponent;

struct UIRectTransform
{
    DirectX::XMFLOAT2 anchorMin = { 0.5f, 0.5f };
    DirectX::XMFLOAT2 anchorMax = { 0.5f, 0.5f };
    DirectX::XMFLOAT2 pivot = { 0.5f, 0.5f };
    DirectX::XMFLOAT2 anchoredPosition = { 0.0f, 0.0f };
    DirectX::XMFLOAT2 sizeDelta = { 100.0f, 100.0f };
    DirectX::XMFLOAT2 localScale = { 1.0f, 1.0f };
    float localRotation = 0.0f;
};

struct UIScreenRect
{
    DirectX::XMFLOAT2 min = { 0.0f, 0.0f };
    DirectX::XMFLOAT2 max = { 100.0f, 100.0f };

    DirectX::XMFLOAT2 GetSize() const
    {
        return { max.x - min.x, max.y - min.y };
    }
};

class UIWidgetComponent : public Component
{
public:
    void Start() override;
    void Update(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    const std::string& GetName() const override
    {
        static std::string name = "UIWidgetComponent";
        return name;
    }

    void MarkLayoutDirty();
    void ResolveParentWidget();
    void CalculateLayout(
        WidgetBoardComponent* board_,
        UIWidgetComponent* parentWidget_,
        const DirectX::XMFLOAT2& screenSize_);

    const std::string& GetWidgetId() const { return widgetId; }
    const std::string& GetParentWidgetId() const { return parentWidgetId; }
    UIRectTransform& GetRectTransform() { return rectTransform; }
    const UIRectTransform& GetRectTransform() const { return rectTransform; }
    const UIScreenRect& GetScreenRect() const { return screenRect; }
    int GetZOrder() const { return zOrder; }
    bool IsVisible() const { return visible; }
    bool IsInputEnabled() const { return inputEnabled; }
    bool IsRaycastTarget() const { return raycastTarget; }
    bool IsLayoutDirty() const { return layoutDirty; }
    UIWidgetComponent* GetParentWidget() const { return parentWidget.lock().get(); }

private:
    std::string widgetId;
    std::string parentWidgetId;
    UIRectTransform rectTransform;
    UIScreenRect screenRect;
    int zOrder = 0;
    bool visible = true;
    bool inputEnabled = true;
    bool raycastTarget = true;
    bool layoutDirty = true;
    std::weak_ptr<UIWidgetComponent> parentWidget;
};

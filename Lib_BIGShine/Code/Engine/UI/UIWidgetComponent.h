#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"

#include <DirectXMath.h>
#include <memory>
#include <string>

class WidgetBoardComponent;

struct UIRectTransform
{
    // 親Rect内でのAnchor範囲。Min/Maxが同じなら固定点、広げるとStretchになる。
    DirectX::XMFLOAT2 anchorMin = { 0.5f, 0.5f };
    DirectX::XMFLOAT2 anchorMax = { 0.5f, 0.5f };
    DirectX::XMFLOAT2 pivot = { 0.5f, 0.5f };
    // Anchor基準点からのOffset。
    DirectX::XMFLOAT2 anchoredPosition = { 0.0f, 0.0f };
    // Anchor範囲に加算するWidgetのSize。
    DirectX::XMFLOAT2 sizeDelta = { 100.0f, 100.0f };
    // Widget単体のScaleと回転。
    DirectX::XMFLOAT2 localScale = { 1.0f, 1.0f };
    float localRotation = 0.0f;
};

struct UIScreenRect
{
    // Layout計算後のScreen座標Rect。
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

    // Layout再計算が必要な状態にする。
    void MarkLayoutDirty();
    // 保存対象のRectを変えず、メニュー演出の移動と拡縮だけをレイアウトへ重ねる。
    void SetRuntimeVisualTransform(const DirectX::XMFLOAT2& offset_, float scale_)
    {
        runtimeVisualOffset = offset_;
        runtimeVisualScale = scale_;
        MarkLayoutDirty();
    }
    // Transformの親をたどり、親Widgetを探す。
    void ResolveParentWidget();
    // Board/親Widget/画面Sizeから、このWidgetのScreenRectを計算する。
    void CalculateLayout(
        WidgetBoardComponent* board_,
        UIWidgetComponent* parentWidget_,
        const DirectX::XMFLOAT2& screenSize_);

    const std::string& GetWidgetId() const { return widgetId; }
    const std::string& GetParentWidgetId() const { return parentWidgetId; }
    UIRectTransform& GetRectTransform() { return rectTransform; }
    const UIRectTransform& GetRectTransform() const { return rectTransform; }
    const UIScreenRect& GetScreenRect() const { return screenRect; }
    // 保存された回転と独立したモーションの回転を合成する。
    float GetVisualRotation();
    int GetZOrder() const { return zOrder; }
    bool IsVisible() const { return visible; }
    bool IsInputEnabled() const { return inputEnabled; }
    bool IsRaycastTarget() const { return raycastTarget; }
    bool IsLayoutDirty() const { return layoutDirty; }
    UIWidgetComponent* GetParentWidget() const { return parentWidget.lock().get(); }

private:
    // UI内で参照するためのIDと、親Widgetを明示指定するID。
    std::string widgetId;
    std::string parentWidgetId;
    // Unity風のAnchor/Pivot/Layout情報。
    UIRectTransform rectTransform;
    DirectX::XMFLOAT2 runtimeVisualOffset{ 0.0f, 0.0f };
    float runtimeVisualScale = 1.0f;
    // 計算済みの画面座標。
    UIScreenRect screenRect;
    // 同じBoard内での描画/入力順。
    int zOrder = 0;
    // 表示、入力、Raycast対象の有効状態。
    bool visible = true;
    bool inputEnabled = true;
    bool raycastTarget = true;
    // trueならUI System側でCalculateLayoutを呼び直す必要がある。
    bool layoutDirty = true;
    // 親Widgetへの弱参照。所有は親GameObject側。
    std::weak_ptr<UIWidgetComponent> parentWidget;
};

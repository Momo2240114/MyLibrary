#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"

#include <DirectXMath.h>
#include <string>

enum class UI_SCALE_MODE
{
    CONSTANT_PIXEL_SIZE,
    SCALE_WITH_SCREEN_SIZE,
    EXPAND,
    SHRINK
};

class WidgetBoardComponent : public Component
{
public:
    void Start() override;
    void Update(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    const std::string& GetName() const override
    {
        static std::string name = "WidgetBoardComponent";
        return name;
    }

    void RebuildWidgetHierarchy();
    void MarkLayoutDirty();
    void SetVisible(bool visible_);
    bool IsVisible() const { return visible; }
    bool IsInputEnabled() const { return inputEnabled; }

    const std::string& GetBoardName() const { return boardName; }
    const std::string& GetBoardId() const { return boardId; }
    const DirectX::XMFLOAT2& GetReferenceResolution() const { return referenceResolution; }
    UI_SCALE_MODE GetScaleMode() const { return scaleMode; }
    int GetBoardLayer() const { return boardLayer; }
    int GetZOrder() const { return zOrder; }
    bool IsLayoutDirty() const { return layoutDirty; }

    DirectX::XMFLOAT2 CalculateBoardScale(const DirectX::XMFLOAT2& screenSize_) const;

private:
    std::string boardName = "WidgetBoard";
    std::string boardId = "widget_board";
    DirectX::XMFLOAT2 referenceResolution = { 1920.0f, 1080.0f };
    UI_SCALE_MODE scaleMode = UI_SCALE_MODE::SCALE_WITH_SCREEN_SIZE;
    int boardLayer = 0;
    int zOrder = 0;
    bool visible = true;
    bool inputEnabled = true;
    bool layoutDirty = true;
};

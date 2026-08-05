#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"

#include <DirectXMath.h>
#include <string>

enum class UI_SCALE_MODE
{
    // 画面解像度に対してWidgetBoardをどうScaleするか。
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

    // 子Widgetの並びやCanvas情報を作り直す入口。
    void RebuildWidgetHierarchy();
    // Layout計算が必要になったことをUI Systemへ知らせる。
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

    // 現在の画面SizeからBoard全体に掛けるScaleを計算する。
    DirectX::XMFLOAT2 CalculateBoardScale(const DirectX::XMFLOAT2& screenSize_) const;

private:
    // Editor表示名と保存用ID。
    std::string boardName = "WidgetBoard";
    std::string boardId = "widget_board";
    // Scale計算の基準解像度。
    DirectX::XMFLOAT2 referenceResolution = { 1920.0f, 1080.0f };
    // 解像度差をどう扱うか。
    UI_SCALE_MODE scaleMode = UI_SCALE_MODE::SCALE_WITH_SCREEN_SIZE;
    // Boardの大分類Layerと同Layer内の描画順。
    int boardLayer = 0;
    int zOrder = 0;
    bool visible = true;
    bool inputEnabled = true;
    // trueなら次回UI更新時にLayout再計算が必要。
    bool layoutDirty = true;
};

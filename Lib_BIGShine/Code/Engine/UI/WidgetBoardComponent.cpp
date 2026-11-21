#include "PrecompiledHeader.h"

#include "WidgetBoardComponent.h"

#include "Engine/Debug/TracyProfiler.h"
#include "imgui.h"

#include <algorithm>
#include <cstring>

namespace
{
    // Serialize文字列とEditor Combo表示をEnum順に合わせる。
    const char* UI_SCALE_MODE_NAMES[] =
    {
        "CONSTANT_PIXEL_SIZE",
        "SCALE_WITH_SCREEN_SIZE",
        "EXPAND",
        "SHRINK"
    };
    constexpr int UI_SCALE_MODE_COUNT =
        static_cast<int>(sizeof(UI_SCALE_MODE_NAMES) / sizeof(UI_SCALE_MODE_NAMES[0]));

    const char* ToString(UI_SCALE_MODE mode_)
    {
        // 不正値は標準のSCALE_WITH_SCREEN_SIZEとして扱う。
        const int index_ = static_cast<int>(mode_);
        if (index_ < 0 || index_ >= UI_SCALE_MODE_COUNT)
        {
            return UI_SCALE_MODE_NAMES[1];
        }
        return UI_SCALE_MODE_NAMES[index_];
    }

    UI_SCALE_MODE ScaleModeFromString(const std::string& name_)
    {
        // 保存文字列からEnumへ戻す。未知の文字列は標準Scaleへフォールバックする。
        for (int index_ = 0; index_ < UI_SCALE_MODE_COUNT; ++index_)
        {
            if (name_ == UI_SCALE_MODE_NAMES[index_])
            {
                return static_cast<UI_SCALE_MODE>(index_);
            }
        }
        return UI_SCALE_MODE::SCALE_WITH_SCREEN_SIZE;
    }
}

void WidgetBoardComponent::Start()
{
    BS_PROFILE_SCOPE("WidgetBoardComponent::Start");
    // 起動直後は子Widgetの並びを必ず計算させる。
    MarkLayoutDirty();
}

void WidgetBoardComponent::Update(float deltaTime_)
{
    BS_PROFILE_SCOPE("WidgetBoardComponent::Update");
    (void)deltaTime_;
}

void WidgetBoardComponent::Gui()
{
    BS_PROFILE_SCOPE("WidgetBoardComponent::Gui");

    char boardNameBuffer_[128]{};
    strncpy_s(boardNameBuffer_, sizeof(boardNameBuffer_), boardName.c_str(), _TRUNCATE);
    if (ImGui::InputText("Board Name", boardNameBuffer_, sizeof(boardNameBuffer_)))
    {
        boardName = boardNameBuffer_;
    }

    char boardIdBuffer_[128]{};
    strncpy_s(boardIdBuffer_, sizeof(boardIdBuffer_), boardId.c_str(), _TRUNCATE);
    if (ImGui::InputText("Board Id", boardIdBuffer_, sizeof(boardIdBuffer_)))
    {
        boardId = boardIdBuffer_;
    }

    if (ImGui::DragFloat2("Reference Resolution", &referenceResolution.x, 1.0f, 1.0f, 8192.0f))
    {
        // 0以下の基準解像度はScale計算を壊すため、最低1に制限する。
        referenceResolution.x = (std::max)(1.0f, referenceResolution.x);
        referenceResolution.y = (std::max)(1.0f, referenceResolution.y);
        MarkLayoutDirty();
    }

    int currentScaleMode_ = static_cast<int>(scaleMode);
    if (ImGui::Combo("Scale Mode", &currentScaleMode_, UI_SCALE_MODE_NAMES, UI_SCALE_MODE_COUNT))
    {
        scaleMode = static_cast<UI_SCALE_MODE>(currentScaleMode_);
        MarkLayoutDirty();
    }

    if (ImGui::InputInt("Board Layer", &boardLayer))
    {
        MarkLayoutDirty();
    }
    if (ImGui::InputInt("Z Order", &zOrder))
    {
        MarkLayoutDirty();
    }
    ImGui::Checkbox("Visible", &visible);
    ImGui::Checkbox("Input Enabled", &inputEnabled);

    if (ImGui::Button("Rebuild Widget Hierarchy"))
    {
        RebuildWidgetHierarchy();
    }
}

void WidgetBoardComponent::Serialize(nlohmann::json& json_) const
{
    BS_PROFILE_SCOPE("WidgetBoardComponent::Serialize");

    json_["Version"] = 1;
    json_["BoardName"] = boardName;
    json_["BoardId"] = boardId;
    json_["ReferenceResolution"] = { referenceResolution.x, referenceResolution.y };
    json_["ScaleMode"] = ToString(scaleMode);
    json_["BoardLayer"] = boardLayer;
    json_["ZOrder"] = zOrder;
    json_["Visible"] = visible;
    json_["InputEnabled"] = inputEnabled;
}

void WidgetBoardComponent::Deserialize(const nlohmann::json& json_)
{
    BS_PROFILE_SCOPE("WidgetBoardComponent::Deserialize");

    boardName = json_.value("BoardName", boardName);
    boardId = json_.value("BoardId", boardId);

    if (json_.contains("ReferenceResolution") &&
        json_["ReferenceResolution"].is_array() &&
        json_["ReferenceResolution"].size() >= 2)
    {
        // 古い/手編集データでもScale計算が壊れないよう最低値を保証する。
        referenceResolution.x = json_["ReferenceResolution"][0].get<float>();
        referenceResolution.y = json_["ReferenceResolution"][1].get<float>();
        referenceResolution.x = (std::max)(1.0f, referenceResolution.x);
        referenceResolution.y = (std::max)(1.0f, referenceResolution.y);
    }

    scaleMode = ScaleModeFromString(json_.value("ScaleMode", std::string(ToString(scaleMode))));
    boardLayer = json_.value("BoardLayer", boardLayer);
    zOrder = json_.value("ZOrder", zOrder);
    visible = json_.value("Visible", visible);
    inputEnabled = json_.value("InputEnabled", inputEnabled);
    MarkLayoutDirty();
}

void WidgetBoardComponent::RebuildWidgetHierarchy()
{
    BS_PROFILE_SCOPE("WidgetBoardComponent::RebuildWidgetHierarchy");
    // 現状はDirty化だけ行い、実際の並び替え/再計算はUI System側に任せる。
    MarkLayoutDirty();
}

void WidgetBoardComponent::MarkLayoutDirty()
{
    layoutDirty = true;
}

void WidgetBoardComponent::SetVisible(bool visible_)
{
    visible = visible_;
}

DirectX::XMFLOAT2 WidgetBoardComponent::CalculateBoardScale(const DirectX::XMFLOAT2& screenSize_) const
{
    // 基準解像度に対する現在画面の倍率をX/Y別に求める。
    const float scaleX_ = screenSize_.x / (std::max)(1.0f, referenceResolution.x);
    const float scaleY_ = screenSize_.y / (std::max)(1.0f, referenceResolution.y);

    switch (scaleMode)
    {
    case UI_SCALE_MODE::CONSTANT_PIXEL_SIZE:
        return { 1.0f, 1.0f };
    case UI_SCALE_MODE::EXPAND:
    {
        // 小さい方へ合わせるため、基準全体が画面内に収まる。
        const float scale_ = (std::min)(scaleX_, scaleY_);
        return { scale_, scale_ };
    }
    case UI_SCALE_MODE::SHRINK:
    {
        // 大きい方へ合わせるため、画面を埋めるが一部がはみ出す可能性がある。
        const float scale_ = (std::max)(scaleX_, scaleY_);
        return { scale_, scale_ };
    }
    case UI_SCALE_MODE::SCALE_WITH_SCREEN_SIZE:
    default:
        return { scaleX_, scaleY_ };
    }
}

#include "PrecompiledHeader.h"

#include "UISpriteSheetComponent.h"

#include "Engine/Debug/TracyProfiler.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/Graphics/Resource/Sprite.h"
#include "imgui.h"

#include <algorithm>

void UISpriteSheetComponent::Start()
{
    BS_PROFILE_SCOPE("UISpriteSheetComponent::Start");
    // 起動時にRendererを解決し、設定を正規化してから初回UVを反映する。
    ResolveSpriteRenderer();
    NormalizeSettings();
    ApplySlice();
}

void UISpriteSheetComponent::Update(float deltaTime_)
{
    BS_PROFILE_SCOPE("UISpriteSheetComponent::Update");
    (void)deltaTime_;

    if (spriteRenderer.expired())
    {
        // Component追加順やRenderer差し替えに対応するため、失効時は取り直す。
        ResolveSpriteRenderer();
    }

    if (applyEveryFrame)
    {
        // Editor操作や外部処理でTexture/UVが変わった場合でも、指定Sliceへ戻す。
        ApplySlice();
    }
}

void UISpriteSheetComponent::Gui()
{
    BS_PROFILE_SCOPE("UISpriteSheetComponent::Gui");

    bool changed_ = false;
    // SpriteSheetの分割数、表示Index、Atlas余白をEditorから調整する。
    changed_ |= ImGui::DragInt("Column Count", &columnCount, 1.0f, 1, 256);
    changed_ |= ImGui::DragInt("Row Count", &rowCount, 1.0f, 1, 256);
    changed_ |= ImGui::DragInt("Column Index", &columnIndex, 1.0f, 0, 255);
    changed_ |= ImGui::DragInt("Row Index", &rowIndex, 1.0f, 0, 255);
    changed_ |= ImGui::DragFloat("Padding Left", &paddingLeft, 0.25f, 0.0f, 4096.0f);
    changed_ |= ImGui::DragFloat("Padding Top", &paddingTop, 0.25f, 0.0f, 4096.0f);
    changed_ |= ImGui::DragFloat("Padding Right", &paddingRight, 0.25f, 0.0f, 4096.0f);
    changed_ |= ImGui::DragFloat("Padding Bottom", &paddingBottom, 0.25f, 0.0f, 4096.0f);
    ImGui::Checkbox("Apply Every Frame", &applyEveryFrame);

    if (changed_)
    {
        // 値変更時は範囲を整えてすぐ見た目へ反映する。
        NormalizeSettings();
        ApplySlice();
    }

    if (ImGui::Button("Apply Slice"))
    {
        ResolveSpriteRenderer();
        NormalizeSettings();
        ApplySlice();
    }
}

void UISpriteSheetComponent::Serialize(nlohmann::json& json_) const
{
    BS_PROFILE_SCOPE("UISpriteSheetComponent::Serialize");

    json_["Version"] = 1;
    json_["ColumnCount"] = columnCount;
    json_["RowCount"] = rowCount;
    json_["ColumnIndex"] = columnIndex;
    json_["RowIndex"] = rowIndex;
    json_["PaddingLeft"] = paddingLeft;
    json_["PaddingTop"] = paddingTop;
    json_["PaddingRight"] = paddingRight;
    json_["PaddingBottom"] = paddingBottom;
    json_["ApplyEveryFrame"] = applyEveryFrame;
}

void UISpriteSheetComponent::Deserialize(const nlohmann::json& json_)
{
    BS_PROFILE_SCOPE("UISpriteSheetComponent::Deserialize");

    columnCount = json_.value("ColumnCount", columnCount);
    rowCount = json_.value("RowCount", rowCount);
    columnIndex = json_.value("ColumnIndex", columnIndex);
    rowIndex = json_.value("RowIndex", rowIndex);
    paddingLeft = json_.value("PaddingLeft", paddingLeft);
    paddingTop = json_.value("PaddingTop", paddingTop);
    paddingRight = json_.value("PaddingRight", paddingRight);
    paddingBottom = json_.value("PaddingBottom", paddingBottom);
    applyEveryFrame = json_.value("ApplyEveryFrame", applyEveryFrame);
    NormalizeSettings();
}

// HUDからの一時的なセル変更はScene保存値へ反映しない。
void UISpriteSheetComponent::SetRuntimeCell(int columnIndex_, int rowIndex_)
{
    const int column_ = std::clamp(columnIndex_, 0, (std::max)(1, columnCount) - 1);
    const int row_ = std::clamp(rowIndex_, 0, (std::max)(1, rowCount) - 1);
    if (runtimeColumnIndex == column_ && runtimeRowIndex == row_) return;
    runtimeColumnIndex = column_;
    runtimeRowIndex = row_;
    if (spriteRenderer.expired()) ResolveSpriteRenderer();
    ApplySlice();
}

// 表示をInspectorで設定したセルへ戻す。
void UISpriteSheetComponent::ClearRuntimeCell()
{
    runtimeColumnIndex = runtimeRowIndex = -1;
    ApplySlice();
}

void UISpriteSheetComponent::ApplySlice()
{
    BS_PROFILE_SCOPE("UISpriteSheetComponent::ApplySlice");

    const std::shared_ptr<SpriteRenderer> spriteRenderer_ = spriteRenderer.lock();
    if (!spriteRenderer_) return;

    const DirectX::XMFLOAT2 textureSize_ = spriteRenderer_->GetTextureSize();
    // Paddingを除いた有効領域を分割し、指定CellのPixel Rectを計算する。
    const float usableWidth_ = (std::max)(1.0f, textureSize_.x - paddingLeft - paddingRight);
    const float usableHeight_ = (std::max)(1.0f, textureSize_.y - paddingTop - paddingBottom);
    const float cellWidth_ = usableWidth_ / static_cast<float>((std::max)(1, columnCount));
    const float cellHeight_ = usableHeight_ / static_cast<float>((std::max)(1, rowCount));
    const float left_ = paddingLeft + cellWidth_ * static_cast<float>(runtimeColumnIndex >= 0 ? std::clamp(runtimeColumnIndex, 0, columnCount - 1) : columnIndex);
    const float top_ = paddingTop + cellHeight_ * static_cast<float>(runtimeRowIndex >= 0 ? std::clamp(runtimeRowIndex, 0, rowCount - 1) : rowIndex);

    // SpriteRenderer側はPixel単位のUV Rectを受け取り、描画時にTexture座標へ変換する。
    spriteRenderer_->SetUVRect(left_, top_, cellWidth_, cellHeight_);
}

void UISpriteSheetComponent::ResolveSpriteRenderer()
{
    BS_PROFILE_SCOPE("UISpriteSheetComponent::ResolveSpriteRenderer");

    if (const std::shared_ptr<GameObject> gameObject_ = GetGameObject())
    {
        spriteRenderer = gameObject_->GetComponent<SpriteRenderer>();
    }
}

void UISpriteSheetComponent::NormalizeSettings()
{
    // 0分割や範囲外IndexでUV計算が壊れないよう、ここでまとめて制限する。
    columnCount = (std::max)(1, columnCount);
    rowCount = (std::max)(1, rowCount);
    columnIndex = std::clamp(columnIndex, 0, columnCount - 1);
    rowIndex = std::clamp(rowIndex, 0, rowCount - 1);
    paddingLeft = (std::max)(0.0f, paddingLeft);
    paddingTop = (std::max)(0.0f, paddingTop);
    paddingRight = (std::max)(0.0f, paddingRight);
    paddingBottom = (std::max)(0.0f, paddingBottom);
}

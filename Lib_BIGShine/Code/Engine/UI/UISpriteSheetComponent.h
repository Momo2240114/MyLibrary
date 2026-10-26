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

    // SpriteSheet上の指定CellをSpriteRendererのUV Rectへ反映する。
    void ApplySlice();
    // 保存済みの分割設定を保ち、実行中の表情などに使うセルだけを切り替える。
    void SetRuntimeCell(int columnIndex_, int rowIndex_ = 0);
    // 一時セルの指定を解除し、保存済みのセルを表示する。
    void ClearRuntimeCell();

private:
    // 同じGameObject上のSpriteRendererを探して保持する。
    void ResolveSpriteRenderer();
    // 分割数やIndex、Paddingが不正値にならないよう補正する。
    void NormalizeSettings();

private:
    // Textureを何列/何行に分割するか。
    int columnCount = 1;
    int rowCount = 1;
    // 表示するCellのIndex。
    int columnIndex = 0;
    int rowIndex = 0;
    int runtimeColumnIndex = -1;
    int runtimeRowIndex = -1;
    // Texture端から除外する余白。Atlas内のにじみ対策に使う。
    float paddingLeft = 0.0f;
    float paddingTop = 0.0f;
    float paddingRight = 0.0f;
    float paddingBottom = 0.0f;
    // trueなら毎Frame UVを再適用し、外部変更されたSpriteRendererにも追従する。
    bool applyEveryFrame = true;

    // UVを実際に持つSpriteRenderer。所有権はGameObject側にある。
    std::weak_ptr<SpriteRenderer> spriteRenderer;
};

#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"

#include <DirectXMath.h>
#include <array>
#include <string>

enum class UI_VERTEX_INDEX
{
    // Sprite/UI Quadの4頂点。描画側の頂点順と合わせて使う。
    LEFT_TOP,
    RIGHT_TOP,
    LEFT_BOTTOM,
    RIGHT_BOTTOM,
    MAX
};

struct UIVertexModification
{
    // 頂点位置を個別にずらし、台形変形や揺れ表現に使う。
    DirectX::XMFLOAT2 positionOffset = { 0.0f, 0.0f };
    // 頂点ごとのUVをずらし、局所的なTexture見え方を調整する。
    DirectX::XMFLOAT2 uvOffset = { 0.0f, 0.0f };
    // 頂点色へ掛ける倍率。GradientやAlpha調整に使う。
    DirectX::XMFLOAT4 colorMultiplier = { 1.0f, 1.0f, 1.0f, 1.0f };
};

class UIVertexModifierComponent : public Component
{
public:
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    const std::string& GetName() const override
    {
        static std::string name = "UIVertexModifierComponent";
        return name;
    }

    // 指定頂点の変更量を取得する。範囲外Enumは左上扱いに丸める。
    const UIVertexModification& GetVertexModification(UI_VERTEX_INDEX vertexIndex_) const;
    // EditorやAnimationから変更するための可変参照を返す。
    UIVertexModification& GetMutableVertexModification(UI_VERTEX_INDEX vertexIndex_);
    // 指定頂点のPosition Offsetだけを更新する。
    void SetVertexPositionOffset(UI_VERTEX_INDEX vertexIndex_, const DirectX::XMFLOAT2& positionOffset_);
    // 4頂点の補正を全て初期値へ戻す。
    void ResetAllModifications();

private:
    // Quad 4頂点それぞれの位置/UV/色補正。
    std::array<UIVertexModification, static_cast<size_t>(UI_VERTEX_INDEX::MAX)> vertexModifications;
};

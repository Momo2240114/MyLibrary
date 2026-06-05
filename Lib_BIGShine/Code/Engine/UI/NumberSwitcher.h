#pragma once
#include "Engine/GamePlay/Core/Component/ComponentBase.h"

class SpriteRenderer; // 前方宣言

class NumberSwitcher : public Component {
public:
    const std::string& GetName() const override {
        static std::string name = "NumberSwitcher";
        return name;
    }

    void Start() override;

    // 外部から表示したいグリッド番号をセットする
    void SetDigit(int num_);

    void Gui() override;
    void Serialize(nlohmann::json& j) const override;
    void Deserialize(const nlohmann::json& j) override;

private:
    // --- 追加：設定用メンバ ---
    int maxColumns = 10;          // 横に何数字並んでいるか（通常10）
    int maxRows = 1;              // 縦に何行並んでいるか
    DirectX::XMFLOAT2 digitSize = { 150.0f, 300.0f }; // 1文字の表示サイズ

    int currentDigit = 0;
    std::weak_ptr<SpriteRenderer> spriteRenderer; // 操作対象
};    
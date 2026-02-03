#pragma once
#include "ComponentBase.h"

class SpriteRenderer; // 前方宣言

class NumberSwitcher : public Component {
public:
    const std::string& GetName() const override {
        static std::string name = "NumberSwitcher";
        return name;
    }

    void Start() override;

    // 外部から数字（0-9）をセットする
    void SetDigit(int num);

    void Gui() override;
    void Serialize(nlohmann::json& j) const override;
    void Deserialize(const nlohmann::json& j) override;

private:
    // --- 追加：設定用メンバ ---
    int m_maxColumns = 10;          // 横に何数字並んでいるか（通常10）
    DirectX::XMFLOAT2 m_digitSize = { 150.0f, 300.0f }; // 1文字の表示サイズ

    int m_currentDigit = 0;
    std::weak_ptr<SpriteRenderer> m_spriteRenderer; // 操作対象
};    
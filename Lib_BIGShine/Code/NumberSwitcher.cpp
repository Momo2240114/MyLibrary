#include "NumberSwitcher.h"
#include "Sprite.h"
#include "GameObject.h"
#include "GameObjectManager.h"

void NumberSwitcher::Start() {
    // 自分のオブジェクトにある SpriteRenderer を取得
    m_spriteRenderer = GetGameObject()->GetComponent<SpriteRenderer>();
}

void NumberSwitcher::SetDigit(int num) {
    m_currentDigit = std::clamp(num, 0, 9);

    m_currentDigit = num; // 範囲制限は画像に合わせて変えられるよう、ここではあえて緩くします

    auto sr = m_spriteRenderer.lock();
    if (!sr) return;



    // 2. 表示サイズ（ピクセル）とUV切り抜き範囲を更新
    sr->SetUVRect(m_digitSize.x * m_currentDigit, 0.0f, m_digitSize.x, m_digitSize.y);
}

void NumberSwitcher::Gui() 
{
    if (ImGui::InputInt("Current Digit", &m_currentDigit)) {
        SetDigit(m_currentDigit);
    }

    ImGui::Separator();
    ImGui::Text("Settings");

    if (ImGui::InputInt("Max Columns", &m_maxColumns)) {
        if (m_maxColumns < 1) m_maxColumns = 1;
        SetDigit(m_currentDigit);
    }

    if (ImGui::DragFloat2("Digit Size", &m_digitSize.x, 1.0f)) {
        SetDigit(m_currentDigit);
    }
}

void NumberSwitcher::Serialize(nlohmann::json& j) const {
    j["CurrentDigit"] = m_currentDigit;
    j["MaxColumns"] = m_maxColumns;
    j["DigitSize"] = { m_digitSize.x, m_digitSize.y };
}

void NumberSwitcher::Deserialize(const nlohmann::json& j) {
    if (j.contains("CurrentDigit")) m_currentDigit = j["CurrentDigit"];
    if (j.contains("MaxColumns")) m_maxColumns = j["MaxColumns"];
    if (j.contains("DigitSize")) {
        m_digitSize.x = j["DigitSize"][0];
        m_digitSize.y = j["DigitSize"][1];
    }
}
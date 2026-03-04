#include "KeyConfig.h"
#include "KeyInput.h"
#include "MouseInput.h"
#include "ControllerInput.h"
#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// --- バインド ---
void KeyConfig::BindAction(const std::string& action, const InputCombo& combo) {
    if (combo.empty()) return;
    m_actionMap[action].push_back(combo);
}

void KeyConfig::ClearBinds(const std::string& action) {
    m_actionMap.erase(action);
}

// --- 内部判定ロジック ---
bool KeyConfig::CheckUnit(const InputUnit& unit, int type) const {
    if (unit.device == DeviceType::KBM) {
        // マウスコード判定 (VK_LBUTTON:0x01, VK_RBUTTON:0x02等)
        if (unit.code >= 0x01 && unit.code <= 0x06) {
            return MouseInput::Instance().GetButtonState(unit.code, type);
        }
        // キーボード判定
        if (type == 0) return KeyInput::KeyPressed(unit.code);
        if (type == 1) return KeyInput::KeyPressedIn(unit.code);
        return KeyInput::KeyPressedOut(unit.code);
    }
    else { // Gamepad
        auto& pad = ControllerInput::Instance();
        if (type == 0) return pad.ButtonPressed((PadCode)unit.code);
        if (type == 1) return pad.ButtonPressedIn((PadCode)unit.code);
        return pad.ButtonPressedOut((PadCode)unit.code);
    }
}

bool KeyConfig::CheckCombo(const InputCombo& combo, int type) const {
    if (combo.empty()) return false;

    // 瞬間入力(In/Out)判定：
    // 「どれか一つがトリガー(type)を引き、かつ全員が押されている(Hold)」場合に成立
    if (type != 0) {
        bool triggered = false;
        for (const auto& unit : combo) {
            if (CheckUnit(unit, type)) { triggered = true; break; }
        }
        if (!triggered) return false;
        for (const auto& unit : combo) {
            if (!CheckUnit(unit, 0)) return false;
        }
        return true;
    }

    // 継続入力(Hold)判定
    for (const auto& unit : combo) {
        if (!CheckUnit(unit, 0)) return false;
    }
    return true;
}

// --- 公開判定関数 ---
bool KeyConfig::ActionPressed(const std::string& action) const {
    auto it = m_actionMap.find(action);
    if (it == m_actionMap.end()) return false;
    for (const auto& combo : it->second) if (CheckCombo(combo, 0)) return true;
    return false;
}

bool KeyConfig::ActionPressedIn(const std::string& action) const {
    auto it = m_actionMap.find(action);
    if (it == m_actionMap.end()) return false;
    for (const auto& combo : it->second) if (CheckCombo(combo, 1)) return true;
    return false;
}

bool KeyConfig::ActionPressedOut(const std::string& action) const {
    auto it = m_actionMap.find(action);
    if (it == m_actionMap.end()) return false;
    for (const auto& combo : it->second) if (CheckCombo(combo, 2)) return true;
    return false;
}

// --- JSON保存 ---
bool KeyConfig::Save(const std::string& filename) {
    json root = json::object();
    for (auto& [name, combos] : m_actionMap) {
        json actionArr = json::array();
        for (auto& combo : combos) {
            json comboArr = json::array();
            for (auto& unit : combo) {
                comboArr.push_back({ {"dev", (int)unit.device}, {"code", unit.code} });
            }
            actionArr.push_back(comboArr);
        }
        root[name] = actionArr;
    }
    std::ofstream ofs(filename);
    if (!ofs) return false;
    ofs << root.dump(4);
    return true;
}

// --- JSON読み込み ---
bool KeyConfig::Load(const std::string& filename) {
    std::ifstream ifs(filename);
    if (!ifs) return false;
    json root;
    try { ifs >> root; }
    catch (...) { return false; }

    m_actionMap.clear();
    for (auto& [name, actionVal] : root.items()) {
        for (auto& comboVal : actionVal) {
            InputCombo combo;
            for (auto& unitVal : comboVal) {
                combo.push_back({ (DeviceType)unitVal["dev"].get<int>(), unitVal["code"].get<int>() });
            }
            m_actionMap[name].push_back(combo);
        }
    }
    return true;
}

bool KeyConfig::Save() { return Save(m_configFile); }
bool KeyConfig::Load() { return Load(m_configFile); }
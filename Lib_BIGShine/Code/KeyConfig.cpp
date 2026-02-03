#include <string>
#include <fstream>
#include <sstream>

#include "KeyConfig.h"

#include <nlohmann/json.hpp>
using json = nlohmann::json;


//
// -------------------- バインド処理 --------------------
//
void KeyConfig::BindAction(const std::string& action, int key, bool allowDuplicate)
{
    auto& vec = keyMap[action];

    // すでに割り当て済みなら追加しない
    if (!allowDuplicate) {
        for (int k : vec) {
            if (k == key) return;
        }
    }

    vec.push_back(key);
}

//
// -------------------- キー取得 --------------------
//
const std::vector<int>& KeyConfig::GetBindedKeys(const std::string& action) const
{
    static const std::vector<int> empty;

    auto it = keyMap.find(action);
    if (it != keyMap.end()) return it->second;
    return empty;
}

int KeyConfig::GetPrimaryKey(const std::string& action) const
{
    auto it = keyMap.find(action);
    if (it != keyMap.end() && !it->second.empty())
        return it->second[0];
    return -1;
}

//
// -------------------- 入力判定 --------------------
//
bool KeyConfig::ActionPressed(const std::string& action) const
{
    auto it = keyMap.find(action);
    if (it == keyMap.end()) return false;

    for (int key : it->second)
        if (KeyInput::KeyPressed(key))
            return true;

    return false;
}

bool KeyConfig::ActionPressedIn(const std::string& action) const
{
    auto it = keyMap.find(action);
    if (it == keyMap.end()) return false;

    for (int key : it->second)
        if (KeyInput::KeyPressedIn(key))
            return true;

    return false;
}

bool KeyConfig::ActionPressedOut(const std::string& action) const
{
    auto it = keyMap.find(action);
    if (it == keyMap.end()) return false;

    for (int key : it->second)
        if (KeyInput::KeyPressedOut(key))
            return true;

    return false;
}

//
// -------------------- JSON 保存 --------------------
//
bool KeyConfig::SaveText(const std::string& filename) const
{
    json j;

    for (auto& it : keyMap)
        j[it.first] = it.second;

    std::ofstream ofs(filename);
    if (!ofs) return false;

    ofs << j.dump(4);
    return true;
}

bool KeyConfig::SaveText() const
{
    if (m_configFile.empty()) return false;
    return SaveText(m_configFile);
}

//
// -------------------- JSON 読み込み --------------------
//
bool KeyConfig::LoadText(const std::string& filename)
{
    std::ifstream ifs(filename);
    if (!ifs) return false;

    json j;
    try {
        ifs >> j;
    }
    catch (...) {
        return false;
    }

    keyMap.clear();

    for (auto& [action, value] : j.items()) {
        if (value.is_array()) {
            keyMap[action] = value.get<std::vector<int>>();
        }
        else if (value.is_number_integer()) {
            // 旧フォーマット互換：単一キー
            keyMap[action] = { value.get<int>() };
        }
    }
    return true;
}

bool KeyConfig::LoadText()
{
    if (m_configFile.empty()) return false;
    return LoadText(m_configFile);
}
#pragma once

#include "KeyInput.h"
#include <unordered_map>
#include <vector>
#include <string>

class KeyConfig
{
public:
    // --- シングルトン取得 ---
    static KeyConfig& Instance() {
        static KeyConfig instance;
        return instance;
    }

    // 保存ファイルパスを設定
    void SetConfigFile(const std::string& filename) {
        m_configFile = filename;
    }

    const std::string& GetConfigFile() const {
        return m_configFile;
    }

    // ---- バインド ----
    void BindAction(const std::string& action, int key, bool allowDuplicate = false);


    // ---- 取得 ----
    const std::vector<int>& GetBindedKeys(const std::string& action) const;
    int GetPrimaryKey(const std::string& action) const;

    // ---- 入力 ----
    bool ActionPressed(const std::string& action) const;
    bool ActionPressedIn(const std::string& action) const;
    bool ActionPressedOut(const std::string& action) const;

    // ---- 保存/読み込み ----
    bool SaveText(const std::string& filename) const;
    bool SaveText() const;
    bool LoadText(const std::string& filename);
    bool LoadText();

private:
    KeyConfig() = default;

    std::unordered_map<std::string, std::vector<int>> keyMap;
    std::string m_configFile;
};

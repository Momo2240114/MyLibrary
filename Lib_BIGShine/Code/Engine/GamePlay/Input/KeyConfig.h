#pragma once
#include "ControllerInput.h"
#include "MouseInput.h"
#include "KeyInput.h"
#include <unordered_map>
#include <vector>
#include <string>
#include "DirectXMath.h"

// デバイス種別（KBM: Keyboard & Mouse）
enum class DeviceType {
    KBM = 0,
    Gamepad = 1
};

// 単一入力ユニット
struct InputUnit {
    DeviceType device;
    int code; // VK_コード、MouseCode、または PadCode
};

// 同時押し（AND）の集合
using InputCombo = std::vector<InputUnit>;

class KeyConfig {
public:
    static KeyConfig& Instance() {
        static KeyConfig instance;
        return instance;
    }

    // 保存ファイルパス設定
    void SetConfigFile(const std::string& filename) { m_configFile = filename; }

    // ---- バインド操作 ----
    // 同時押しセット(combo)をアクションに登録（呼ぶたびにOR条件として追加）
    void BindAction(const std::string& action, const InputCombo& combo);
    void ClearBinds(const std::string& action);

    // ---- 入力判定 ----
    bool ActionPressed(const std::string& action) const;   // 押しっぱなし
    bool ActionPressedIn(const std::string& action) const; // 押した瞬間
    bool ActionPressedOut(const std::string& action) const;// 離した瞬間

    // ---- 保存/読み込み (JSON) ----
    bool Save();
    bool Load();
    bool Save(const std::string& filename);
    bool Load(const std::string& filename);

private:
    KeyConfig() = default;

    // 内部判定用
    bool CheckUnit(const InputUnit& unit, int type) const; // type 0:Hold, 1:In, 2:Out
    bool CheckCombo(const InputCombo& combo, int type) const;

    // Map構造: Action名 -> [Combo1(AND) OR Combo2(AND) ...]
    std::unordered_map<std::string, std::vector<InputCombo>> m_actionMap;
    std::string m_configFile;
};
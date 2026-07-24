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

enum class AxisSourceType
{
    Button = 0,
    MouseMove = 1,
    GamepadStick = 2
};

enum class AxisCode
{
    MouseX = 0,
    MouseY = 1,

    LeftStickX = 2,
    LeftStickY = 3,
    RightStickX = 4,
    RightStickY = 5,

    MouseWheel = 100
};

struct AxisUnit
{
    AxisSourceType sourceType = AxisSourceType::Button;
    DeviceType device = DeviceType::KBM;
    int code = 0;
    float scale = 1.0f;
    float deadZone = 0.15f;
};


using AxisBind = std::vector<AxisUnit>;

// 同時押し（AND）の集合
using InputCombo = std::vector<InputUnit>;

class KeyConfig {
public:
    static KeyConfig& Instance() {
        static KeyConfig instance;
        return instance;
    }

    // 保存ファイルパス設定
    void SetConfigFile(const std::string& filename) { configFile = filename; }
    const std::string& GetConfigFile() const { return configFile; }

    // ---- バインド操作 ----
    // 同時押しセット(combo)をアクションに登録（呼ぶたびにOR条件として追加）
    void BindAction(const std::string& action, const InputCombo& combo);
    void SetBinds(const std::string& action, const std::vector<InputCombo>& combos);
    void RemoveAction(const std::string& action);
    bool RenameAction(const std::string& oldAction, const std::string& newAction);
    void ClearBinds(const std::string& action);
    const std::unordered_map<std::string, std::vector<InputCombo>>& GetActionMap() const { return actionMap; }

    // ---- 入力判定 ----
    bool ActionPressed(const std::string& action) const;   // 押しっぱなし
    bool ActionPressedIn(const std::string& action) const; // 押した瞬間
    bool ActionPressedOut(const std::string& action) const;// 離した瞬間

    // ---- 保存/読み込み (JSON) ----
    bool Save();
    bool Load();
    bool Save(const std::string& filename);
    bool Load(const std::string& filename);

    //-----Axis関係------
    void BindAxis(const std::string& axisName_, const AxisUnit& unit_);
    void SetAxisBinds(const std::string& axisName_, const AxisBind& binds_);
    void RemoveAxis(const std::string& axisName_);

    float GetAxis(const std::string& axisName_) const;
    float GetRawAxis(const std::string& axisName_) const;
    const std::unordered_map<std::string, AxisBind>& GetAxisMap() const { return axisMap; }

private:
    KeyConfig() = default;

    // 内部判定用
    bool CheckUnit(const InputUnit& unit, int type) const; // type 0:Hold, 1:In, 2:Out
    bool CheckCombo(const InputCombo& combo, int type) const;

    // Map構造: Action名 -> [Combo1(AND) OR Combo2(AND) ...]
    std::unordered_map<std::string, std::vector<InputCombo>> actionMap;
    std::string configFile;

    //-----Axis関係------
    float GetAxisUnitValue(const AxisUnit& unit_) const;

    std::unordered_map<std::string, AxisBind> axisMap;
};

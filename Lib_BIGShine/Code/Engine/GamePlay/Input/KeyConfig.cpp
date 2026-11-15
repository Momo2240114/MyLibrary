#include "PrecompiledHeader.h"

#include "KeyConfig.h"
#include "Engine/Debug/TracyProfiler.h"
#include "KeyInput.h"
#include "Engine/UI/ScreenTransitionManager.h"
#include "MouseInput.h"
#include "ControllerInput.h"
#include <fstream>
#include <iomanip>
#include <utility>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// --- バインド ---
void KeyConfig::BindAction(const std::string& action_, const InputCombo& combo_) {
    if (combo_.empty()) return;
    actionMap[action_].push_back(combo_);
    ++bindingsRevision;
}

void KeyConfig::SetBinds(const std::string& action_, const std::vector<InputCombo>& combos_) {
    if (action_.empty()) return;
    actionMap[action_] = combos_;
    ++bindingsRevision;
}

void KeyConfig::RemoveAction(const std::string& action_) {
    actionMap.erase(action_);
    ++bindingsRevision;
}

bool KeyConfig::RenameAction(const std::string& oldAction_, const std::string& newAction_) {
    if (oldAction_.empty() || newAction_.empty() || oldAction_ == newAction_) return false;

    auto it = actionMap.find(oldAction_);
    if (it == actionMap.end() || actionMap.find(newAction_) != actionMap.end()) return false;

    auto combos = std::move(it->second);
    actionMap.erase(it);
    actionMap[newAction_] = std::move(combos);
    ++bindingsRevision;
    return true;
}

void KeyConfig::ClearBinds(const std::string& action_) {
    actionMap.erase(action_);
    ++bindingsRevision;
}

// --- 内部判定ロジック ---
bool KeyConfig::CheckUnit(const InputUnit& unit_, int type_) const
{
    if (unit_.device == DeviceType::KBM) {
        // マウスコード判定 (VK_LBUTTON:0x01, VK_RBUTTON:0x02等)
        if (unit_.code >= 0x01 && unit_.code <= 0x06) {
            return MouseInput::Instance().GetButtonState(unit_.code, type_);
        }
        // キーボード判定
        if (type_ == 0) return KeyInput::KeyPressed(unit_.code);
        if (type_ == 1) return KeyInput::KeyPressedIn(unit_.code);
        return KeyInput::KeyPressedOut(unit_.code);
    }
    else { // Gamepad
        auto& pad_ = ControllerInput::Instance();
        if (type_ == 0) return pad_.ButtonPressed((PadCode)unit_.code);
        if (type_ == 1) return pad_.ButtonPressedIn((PadCode)unit_.code);
        return pad_.ButtonPressedOut((PadCode)unit_.code);
    }
}

bool KeyConfig::CheckCombo(const InputCombo& combo_, int type_) const {
    if (combo_.empty()) return false;

    // 瞬間入力(In/Out)判定：
    // 「どれか一つがトリガー(type)を引き、かつ全員が押されている(Hold)」場合に成立
    if (type_ != 0) {
        bool triggered_ = false;
        for (const auto& unit : combo_) {
            if (CheckUnit(unit, type_)) { triggered_ = true; break; }
        }
        if (!triggered_) return false;
        for (const auto& unit : combo_) {
            if (!CheckUnit(unit, 0)) return false;
        }
        return true;
    }

    // 継続入力(Hold)判定
    for (const auto& unit : combo_) {
        if (!CheckUnit(unit, 0)) return false;
    }
    return true;
}

// --- 公開判定関数 ---
bool KeyConfig::ActionPressed(const std::string& action_) const {
    // 遷移中もデバイス状態は更新し、ゲーム向けの入力結果だけを抑止する。
    if (ScreenTransitionManager::Instance().IsInputBlocked()) return false;
    auto it = actionMap.find(action_);
    if (it == actionMap.end()) return false;
    for (const auto& combo_ : it->second) if (CheckCombo(combo_, 0)) return true;
    return false;
}

bool KeyConfig::ActionPressedIn(const std::string& action_) const {
    // 遷移中もデバイス状態は更新し、ゲーム向けの入力結果だけを抑止する。
    if (ScreenTransitionManager::Instance().IsInputBlocked()) return false;
    auto it = actionMap.find(action_);
    if (it == actionMap.end()) return false;
    for (const auto& combo_ : it->second) if (CheckCombo(combo_, 1)) return true;
    return false;
}

bool KeyConfig::ActionPressedOut(const std::string& action_) const {
    // 遷移中もデバイス状態は更新し、ゲーム向けの入力結果だけを抑止する。
    if (ScreenTransitionManager::Instance().IsInputBlocked()) return false;
    auto it = actionMap.find(action_);
    if (it == actionMap.end()) return false;
    for (const auto& combo_ : it->second) if (CheckCombo(combo_, 2)) return true;
    return false;
}

// --- JSON保存 ---
bool KeyConfig::Save(const std::string& filename)
{
    if (filename.empty()) return false;

    json root = json::object();
    root["actions"] = json::object();
    root["axes"] = json::object();

    for (auto& [name_, combos_] : actionMap)
    {
        json actionArr_ = json::array();

        for (auto& combo_ : combos_)
        {
            json comboArr_ = json::array();

            for (auto& unit_ : combo_)
            {
                comboArr_.push_back({
                    {"dev", static_cast<int>(unit_.device)},
                    {"code", unit_.code}
                    });
            }

            actionArr_.push_back(comboArr_);
        }

        root["actions"][name_] = actionArr_;
    }

    for (auto& [name_, binds_] : axisMap)
    {
        json axisArr_ = json::array();

        for (auto& unit_ : binds_)
        {
            axisArr_.push_back({
                {"sourceType", static_cast<int>(unit_.sourceType)},
                {"dev", static_cast<int>(unit_.device)},
                {"code", unit_.code},
                {"scale", unit_.scale},
                {"deadZone", unit_.deadZone}
                });
        }

        root["axes"][name_] = axisArr_;
    }

    std::ofstream ofs_(filename);
    if (!ofs_) return false;

    ofs_ << root.dump(4);
    return true;
}

// --- JSON読み込み ---
bool KeyConfig::Load(const std::string& filename)
{
    if (filename.empty()) return false;

    std::ifstream ifs_(filename);
    if (!ifs_) return false;

    json root_;

    try
    {
        ifs_ >> root_;
    }
    catch (...)
    {
        return false;
    }

    actionMap.clear();
    ++bindingsRevision;
    axisMap.clear();

    json actions_ = root_.contains("actions") ? root_["actions"] : root_;

    for (auto& [name_, actionVal_] : actions_.items())
    {
        if (name_ == "axes") continue;

        for (auto& comboVal_ : actionVal_)
        {
            InputCombo combo_;

            for (auto& unitVal_ : comboVal_)
            {
                combo_.push_back({
                    static_cast<DeviceType>(unitVal_["dev"].get<int>()),
                    unitVal_["code"].get<int>()
                    });
            }

            actionMap[name_].push_back(combo_);
        }
    }

    if (root_.contains("axes"))
    {
        for (auto& [name_, axisVal_] : root_["axes"].items())
        {
            for (auto& unitVal_ : axisVal_)
            {
                AxisUnit unit_{};

                unit_.sourceType = static_cast<AxisSourceType>(unitVal_.value("sourceType", 0));
                unit_.device = static_cast<DeviceType>(unitVal_.value("dev", 0));
                unit_.code = unitVal_.value("code", 0);
                unit_.scale = unitVal_.value("scale", 1.0f);
                unit_.deadZone = unitVal_.value("deadZone", 0.15f);

                axisMap[name_].push_back(unit_);
            }
        }
    }

    return true;
}
//-----Axis-----
void KeyConfig::BindAxis(const std::string& axisName_, const AxisUnit& unit_)
{
    if (axisName_.empty()) return;
    axisMap[axisName_].push_back(unit_);
}

void KeyConfig::SetAxisBinds(const std::string& axisName_, const AxisBind& binds_)
{
    if (axisName_.empty()) return;
    axisMap[axisName_] = binds_;
}

void KeyConfig::RemoveAxis(const std::string& axisName_)
{
    axisMap.erase(axisName_);
}

float KeyConfig::GetAxis(const std::string& axisName_) const
{
    float value_ = GetRawAxis(axisName_);

    if (value_ > 1.0f) value_ = 1.0f;
    if (value_ < -1.0f) value_ = -1.0f;

    return value_;
}

float KeyConfig::GetRawAxis(const std::string& axisName_) const
{
    if (ScreenTransitionManager::Instance().IsInputBlocked()) return 0.0f;
    auto it_ = axisMap.find(axisName_);
    if (it_ == axisMap.end()) return 0.0f;

    float value_ = 0.0f;

    for (const auto& unit_ : it_->second)
    {
        value_ += GetAxisUnitValue(unit_);
    }

    return value_;
}

float KeyConfig::GetAxisUnitValue(const AxisUnit& unit_) const
{
    if (unit_.sourceType == AxisSourceType::Button)
    {
        InputUnit inputUnit_{ unit_.device, unit_.code };
        return CheckUnit(inputUnit_, 0) ? unit_.scale : 0.0f;
    }

    if (unit_.sourceType == AxisSourceType::MouseMove)
    {
        const auto& mouse_ = MouseInput::Instance();
        const auto delta_ = mouse_.IsCursorLocked() ? mouse_.GetSubMousePos() : Vector2{};
        float value_ = 0.0f;

        if (unit_.code == static_cast<int>(AxisCode::MouseX))
        {
            value_ = delta_.x;
        }
        else if (unit_.code == static_cast<int>(AxisCode::MouseY))
        {
            value_ = delta_.y;
        }
        else if (unit_.code == static_cast<int>(AxisCode::MouseWheel))
        {
            value_ = static_cast<float>(MouseInput::Instance().GetWheelDeltaThisFrame());
        }

        return value_ * unit_.scale;
    }

    if (unit_.sourceType == AxisSourceType::GamepadStick)
    {
        auto& pad_ = ControllerInput::Instance();
        float value_ = 0.0f;

        if (unit_.code == static_cast<int>(AxisCode::LeftStickX))
        {
            value_ = pad_.GetLeftStick().x;
        }
        else if (unit_.code == static_cast<int>(AxisCode::LeftStickY))
        {
            value_ = pad_.GetLeftStick().y;
        }
        else if (unit_.code == static_cast<int>(AxisCode::RightStickX))
        {
            value_ = pad_.GetRightStick().x;
        }
        else if (unit_.code == static_cast<int>(AxisCode::RightStickY))
        {
            value_ = pad_.GetRightStick().y;
        }

        if (std::abs(value_) < unit_.deadZone)
        {
            return 0.0f;
        }

        return value_ * unit_.scale;
    }

    return 0.0f;
}



bool KeyConfig::Save() { return Save(configFile); }
bool KeyConfig::Load() { return Load(configFile); }

// 新規操作だけを観測し、KeyPressedInの共有状態やゲーム操作のタイミングを変更しない。
void KeyConfig::UpdateActiveDevice(bool focused_)
{
    BS_PROFILE_SCOPE("KeyConfig::UpdateActiveDevice");
    auto& mouse_ = MouseInput::Instance();
    auto& pad_ = ControllerInput::Instance();
    bool keyboard_ = false;
    for (int code_ = 8; code_ < 256; ++code_)
    {
        const bool down_ = KeyInput::KeyPressed(code_);
        keyboard_ |= down_ && !observedKeys[code_];
        observedKeys[code_] = down_;
    }
    bool click_ = false;
    for (const int code_ : { VK_LBUTTON, VK_RBUTTON, VK_MBUTTON, VK_XBUTTON1, VK_XBUTTON2 })
        click_ |= mouse_.GetButtonState(code_,1);
    const auto delta_ = mouse_.GetSubMousePos();
    const bool wheel_ = observedWheelRevision != mouse_.GetWheelRevision();
    observedWheelRevision = mouse_.GetWheelRevision();
    if (focused_ && observationInitialized)
    {
        // 明確なボタン操作を移動より優先し、4ピクセル未満のマウス振動を無視する。
        if (keyboard_ || click_ || wheel_) activeDevice = InputDisplayDevice::KEYBOARD_MOUSE;
        else if (pad_.HasMeaningfulInput()) activeDevice = pad_.IsPlayStation() ? InputDisplayDevice::PLAYSTATION_PAD : InputDisplayDevice::XBOX_PAD;
        else if (delta_.x * delta_.x + delta_.y * delta_.y >= 16.0f) activeDevice = InputDisplayDevice::KEYBOARD_MOUSE;
    }
    if (!pad_.IsConnected() && activeDevice != InputDisplayDevice::KEYBOARD_MOUSE)
        activeDevice = InputDisplayDevice::KEYBOARD_MOUSE;
    observationInitialized = focused_;
}

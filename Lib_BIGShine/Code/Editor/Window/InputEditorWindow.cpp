#include "InputEditorWindow.h"

#include <Windows.h>
#include <algorithm>
#include <filesystem>
#include <imgui_stdlib.h>

InputEditorWindow::InputEditorWindow(GameObjectManager* manager_)
    : EditorWindow("Input Editor", manager_)
{
    defaultWindowSize = ImVec2(900.0f, 640.0f);
    isVisible = false;

    keyboardOptions = {
        {"A", DeviceType::KBM, 'A'}, {"B", DeviceType::KBM, 'B'}, {"C", DeviceType::KBM, 'C'}, {"D", DeviceType::KBM, 'D'},
        {"E", DeviceType::KBM, 'E'}, {"F", DeviceType::KBM, 'F'}, {"G", DeviceType::KBM, 'G'}, {"H", DeviceType::KBM, 'H'},
        {"I", DeviceType::KBM, 'I'}, {"J", DeviceType::KBM, 'J'}, {"K", DeviceType::KBM, 'K'}, {"L", DeviceType::KBM, 'L'},
        {"M", DeviceType::KBM, 'M'}, {"N", DeviceType::KBM, 'N'}, {"O", DeviceType::KBM, 'O'}, {"P", DeviceType::KBM, 'P'},
        {"Q", DeviceType::KBM, 'Q'}, {"R", DeviceType::KBM, 'R'}, {"S", DeviceType::KBM, 'S'}, {"T", DeviceType::KBM, 'T'},
        {"U", DeviceType::KBM, 'U'}, {"V", DeviceType::KBM, 'V'}, {"W", DeviceType::KBM, 'W'}, {"X", DeviceType::KBM, 'X'},
        {"Y", DeviceType::KBM, 'Y'}, {"Z", DeviceType::KBM, 'Z'},
        {"0", DeviceType::KBM, '0'}, {"1", DeviceType::KBM, '1'}, {"2", DeviceType::KBM, '2'}, {"3", DeviceType::KBM, '3'},
        {"4", DeviceType::KBM, '4'}, {"5", DeviceType::KBM, '5'}, {"6", DeviceType::KBM, '6'}, {"7", DeviceType::KBM, '7'},
        {"8", DeviceType::KBM, '8'}, {"9", DeviceType::KBM, '9'},
        {"Space", DeviceType::KBM, VK_SPACE}, {"Enter", DeviceType::KBM, VK_RETURN}, {"Tab", DeviceType::KBM, VK_TAB},
        {"Backspace", DeviceType::KBM, VK_BACK}, {"Escape", DeviceType::KBM, VK_ESCAPE}, {"Shift", DeviceType::KBM, VK_SHIFT},
        {"Ctrl", DeviceType::KBM, VK_CONTROL}, {"Alt", DeviceType::KBM, VK_MENU},
        {"Left", DeviceType::KBM, VK_LEFT}, {"Right", DeviceType::KBM, VK_RIGHT}, {"Up", DeviceType::KBM, VK_UP}, {"Down", DeviceType::KBM, VK_DOWN},
        {"Delete", DeviceType::KBM, VK_DELETE}, {"Insert", DeviceType::KBM, VK_INSERT}, {"Home", DeviceType::KBM, VK_HOME}, {"End", DeviceType::KBM, VK_END},
        {"PageUp", DeviceType::KBM, VK_PRIOR}, {"PageDown", DeviceType::KBM, VK_NEXT},
        {"F1", DeviceType::KBM, VK_F1}, {"F2", DeviceType::KBM, VK_F2}, {"F3", DeviceType::KBM, VK_F3}, {"F4", DeviceType::KBM, VK_F4},
        {"F5", DeviceType::KBM, VK_F5}, {"F6", DeviceType::KBM, VK_F6}, {"F7", DeviceType::KBM, VK_F7}, {"F8", DeviceType::KBM, VK_F8},
        {"F9", DeviceType::KBM, VK_F9}, {"F10", DeviceType::KBM, VK_F10}, {"F11", DeviceType::KBM, VK_F11}, {"F12", DeviceType::KBM, VK_F12},
        {"Num0", DeviceType::KBM, VK_NUMPAD0}, {"Num1", DeviceType::KBM, VK_NUMPAD1}, {"Num2", DeviceType::KBM, VK_NUMPAD2},
        {"Num3", DeviceType::KBM, VK_NUMPAD3}, {"Num4", DeviceType::KBM, VK_NUMPAD4}, {"Num5", DeviceType::KBM, VK_NUMPAD5},
        {"Num6", DeviceType::KBM, VK_NUMPAD6}, {"Num7", DeviceType::KBM, VK_NUMPAD7}, {"Num8", DeviceType::KBM, VK_NUMPAD8}, {"Num9", DeviceType::KBM, VK_NUMPAD9}
    };

    mouseOptions = {
        {"Mouse Left", DeviceType::KBM, VK_LBUTTON},
        {"Mouse Right", DeviceType::KBM, VK_RBUTTON},
        {"Mouse Middle", DeviceType::KBM, VK_MBUTTON}
    };

    padOptions = {
        {"Pad A", DeviceType::Gamepad, static_cast<int>(PadCode::A)}, {"Pad B", DeviceType::Gamepad, static_cast<int>(PadCode::B)},
        {"Pad X", DeviceType::Gamepad, static_cast<int>(PadCode::X)}, {"Pad Y", DeviceType::Gamepad, static_cast<int>(PadCode::Y)},
        {"Pad L1", DeviceType::Gamepad, static_cast<int>(PadCode::L1)}, {"Pad R1", DeviceType::Gamepad, static_cast<int>(PadCode::R1)},
        {"Pad L2", DeviceType::Gamepad, static_cast<int>(PadCode::L2)}, {"Pad R2", DeviceType::Gamepad, static_cast<int>(PadCode::R2)},
        {"Pad L3", DeviceType::Gamepad, static_cast<int>(PadCode::L3)}, {"Pad R3", DeviceType::Gamepad, static_cast<int>(PadCode::R3)},
        {"Pad Start", DeviceType::Gamepad, static_cast<int>(PadCode::Start)}, {"Pad Back", DeviceType::Gamepad, static_cast<int>(PadCode::Back)},
        {"Pad Up", DeviceType::Gamepad, static_cast<int>(PadCode::Up)}, {"Pad Down", DeviceType::Gamepad, static_cast<int>(PadCode::Down)},
        {"Pad Left", DeviceType::Gamepad, static_cast<int>(PadCode::Left)}, {"Pad Right", DeviceType::Gamepad, static_cast<int>(PadCode::Right)},
        {"L Stick Up", DeviceType::Gamepad, static_cast<int>(PadCode::StickL_Up)}, {"L Stick Down", DeviceType::Gamepad, static_cast<int>(PadCode::StickL_Down)},
        {"L Stick Left", DeviceType::Gamepad, static_cast<int>(PadCode::StickL_Left)}, {"L Stick Right", DeviceType::Gamepad, static_cast<int>(PadCode::StickL_Right)},
        {"R Stick Up", DeviceType::Gamepad, static_cast<int>(PadCode::StickR_Up)}, {"R Stick Down", DeviceType::Gamepad, static_cast<int>(PadCode::StickR_Down)},
        {"R Stick Left", DeviceType::Gamepad, static_cast<int>(PadCode::StickR_Left)}, {"R Stick Right", DeviceType::Gamepad, static_cast<int>(PadCode::StickR_Right)}
    };

    mouseAxisOptions = {
    {"Mouse X", AxisCode::MouseX},
    {"Mouse Y", AxisCode::MouseY}
    };

    stickAxisOptions = {
        {"Left Stick X", AxisCode::LeftStickX},
        {"Left Stick Y", AxisCode::LeftStickY},
        {"Right Stick X", AxisCode::RightStickX},
        {"Right Stick Y", AxisCode::RightStickY}
    };

    auto& keyConfig_ = KeyConfig::Instance();
    if (keyConfig_.GetConfigFile().empty())
    {
        keyConfig_.SetConfigFile(configFilePath);
    }
    else
    {
        configFilePath = keyConfig_.GetConfigFile();
    }

    keyConfig_.Load(configFilePath);
    RefreshDraftFromConfig();
}

void InputEditorWindow::Draw()
{
    if (!BeginEditorWindow(nullptr, ImGuiWindowFlags_NoCollapse))
    {
        EndEditorWindow();
        return;
    }

    DrawToolbar();
    ImGui::Separator();

    DrawModeTabs();
    ImGui::Separator();

    const float leftWidth_ = 250.0f;

    ImGui::BeginChild("InputList", ImVec2(leftWidth_, 0.0f), true);

    if (editMode == InputEditMode::Action)
    {
        DrawActionList();
    }
    else
    {
        DrawAxisList();
    }

    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("InputEditor", ImVec2(0.0f, 0.0f), true);

    if (editMode == InputEditMode::Action)
    {
        DrawActionEditor();
    }
    else
    {
        DrawAxisEditor();
    }

    ImGui::EndChild();

    DrawCapturePopup();
    DrawStylePopup();

    EndEditorWindow();
}

std::vector<std::string> InputEditorWindow::BuildActionNames() const
{
    std::vector<std::string> names_;
    for (const auto& pair_ : draftActionMap)
    {
        names_.push_back(pair_.first);
    }
    std::sort(names_.begin(), names_.end());
    return names_;
}

std::vector<InputCombo>* InputEditorWindow::GetSelectedCombos()
{
    auto it_ = draftActionMap.find(selectedActionName);
    return it_ == draftActionMap.end() ? nullptr : &it_->second;
}

const std::vector<InputCombo>* InputEditorWindow::GetSelectedCombos() const
{
    auto it_ = draftActionMap.find(selectedActionName);
    return it_ == draftActionMap.end() ? nullptr : &it_->second;
}

void InputEditorWindow::DrawToolbar()
{
    ImGui::TextUnformatted("Config");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-220.0f);
    if (ImGui::InputText("##ConfigPath", &configFilePath))
    {
        KeyConfig::Instance().SetConfigFile(configFilePath);
    }

    ImGui::SameLine();
    if (ImGui::Button("Load"))
    {
        KeyConfig::Instance().SetConfigFile(configFilePath);
        if (KeyConfig::Instance().Load(configFilePath))
        {
            RefreshDraftFromConfig();
            statusMessage = "Loaded.";
        }
        else
        {
            statusMessage = "Load failed.";
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Save"))
    {
        ApplyDraftToConfig();
        std::filesystem::path path_(configFilePath);
        if (path_.has_parent_path())
        {
            std::filesystem::create_directories(path_.parent_path());
        }

        if (KeyConfig::Instance().Save(configFilePath))
        {
            dirty = false;
            statusMessage = "Saved.";
        }
        else
        {
            statusMessage = "Save failed.";
        }
    }

    DrawStatusLine();
}

void InputEditorWindow::DrawActionList()
{
    ImGui::InputText("New Action", &newActionName);
    if (ImGui::Button("Add Action", ImVec2(-1.0f, 0.0f)))
    {
        if (!newActionName.empty() && draftActionMap.find(newActionName) == draftActionMap.end())
        {
            draftActionMap[newActionName] = {};
            SelectAction(newActionName);
            dirty = true;
        }
    }

    ImGui::Separator();
    auto names_ = BuildActionNames();
    for (const auto& name_ : names_)
    {
        if (ImGui::Selectable(name_.c_str(), selectedActionName == name_))
        {
            SelectAction(name_);
        }
    }
}

void InputEditorWindow::DrawActionEditor()
{
    auto* combos_ = GetSelectedCombos();
    if (!combos_)
    {
        ImGui::TextDisabled("No action selected.");
        return;
    }

    ImGui::Text("Action: %s", selectedActionName.c_str());
    ImGui::SetNextItemWidth(280.0f);
    ImGui::InputText("Name", &renameBuffer);
    ImGui::SameLine();
    if (ImGui::Button("Rename"))
    {
        if (!renameBuffer.empty() && draftActionMap.find(renameBuffer) == draftActionMap.end())
        {
            auto movedCombos_ = std::move(*combos_);
            draftActionMap.erase(selectedActionName);
            draftActionMap[renameBuffer] = std::move(movedCombos_);
            SelectAction(renameBuffer);
            dirty = true;
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Delete Action"))
    {
        draftActionMap.erase(selectedActionName);
        selectedActionName.clear();
        renameBuffer.clear();
        dirty = true;
        return;
    }

    ImGui::Separator();
    if (ImGui::Button("Add Combo"))
    {
        combos_->push_back(InputCombo{ InputUnit{ DeviceType::KBM, 'A' } });
        dirty = true;
    }

    for (int comboIndex_ = 0; comboIndex_ < static_cast<int>(combos_->size()); ++comboIndex_)
    {
        ImGui::PushID(comboIndex_);
        DrawComboEditor((*combos_)[comboIndex_], comboIndex_);
        ImGui::PopID();
    }
}

void InputEditorWindow::DrawComboEditor(InputCombo& combo_, int comboIndex_)
{
    ImGui::Separator();
    ImGui::Text("Combo %d", comboIndex_ + 1);
    ImGui::SameLine();
    if (ImGui::SmallButton("Add Unit"))
    {
        combo_.push_back(InputUnit{ DeviceType::KBM, 'A' });
        dirty = true;
    }

    ImGui::SameLine();
    if (ImGui::SmallButton("Remove Combo"))
    {
        auto* combos_ = GetSelectedCombos();
        if (combos_ && comboIndex_ >= 0 && comboIndex_ < static_cast<int>(combos_->size()))
        {
            combos_->erase(combos_->begin() + comboIndex_);
            dirty = true;
        }
        return;
    }

    for (int unitIndex_ = 0; unitIndex_ < static_cast<int>(combo_.size()); ++unitIndex_)
    {
        ImGui::PushID(unitIndex_);
        DrawUnitEditor(combo_, combo_[unitIndex_], unitIndex_);
        ImGui::PopID();
    }
}

void InputEditorWindow::DrawUnitEditor(InputCombo& combo_, InputUnit& unit_, int unitIndex_)
{
    ImGui::AlignTextToFramePadding();
    ImGui::Text("%d", unitIndex_ + 1);
    ImGui::SameLine();
    DrawInputPicker(unit_);
    ImGui::SameLine();
    if (ImGui::SmallButton("Capture"))
    {
        captureTarget = &unit_;
        capturePopupOpen = true;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Remove"))
    {
        combo_.erase(combo_.begin() + unitIndex_);
        dirty = true;
    }
}

void InputEditorWindow::DrawInputPicker(InputUnit& unit_)
{
    const std::string preview_ = BuildUnitLabel(unit_);
    ImGui::SetNextItemWidth(260.0f);
    if (!ImGui::BeginCombo("##Input", preview_.c_str())) return;

    auto drawOptions_ = [&](const char* header_, const std::vector<InputOption>& options_)
        {
            if (ImGui::Selectable(header_, false, ImGuiSelectableFlags_Disabled)) {}
            for (const auto& option_ : options_)
            {
                const bool selected_ = unit_.device == option_.device && unit_.code == option_.code;
                if (ImGui::Selectable(option_.name, selected_))
                {
                    unit_.device = option_.device;
                    unit_.code = option_.code;
                    dirty = true;
                }
            }
        };

    drawOptions_("Keyboard", keyboardOptions);
    ImGui::Separator();
    drawOptions_("Mouse", mouseOptions);
    ImGui::Separator();
    drawOptions_("Gamepad", padOptions);
    ImGui::EndCombo();
}

void InputEditorWindow::DrawCapturePopup()
{
    if (capturePopupOpen)
    {
        ImGui::OpenPopup("Capture Keyboard Input");
        capturePopupOpen = false;
    }

    if (ImGui::BeginPopupModal("Capture Keyboard Input", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("Press any keyboard key.");
        const int code_ = CaptureKeyboardCode();
        if (code_ > 0 && captureTarget)
        {
            captureTarget->device = DeviceType::KBM;
            captureTarget->code = code_;
            captureTarget = nullptr;
            dirty = true;
            ImGui::CloseCurrentPopup();
        }

        if (ImGui::Button("Cancel"))
        {
            captureTarget = nullptr;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void InputEditorWindow::DrawStatusLine()
{
    ImGui::SameLine();
    if (dirty)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.78f, 0.25f, 1.0f), "Modified");
    }
    if (!statusMessage.empty())
    {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", statusMessage.c_str());
    }
}
void InputEditorWindow::RefreshDraftFromConfig()
{
    draftActionMap = KeyConfig::Instance().GetActionMap();
    draftAxisMap = KeyConfig::Instance().GetAxisMap();

    auto actionNames_ = BuildActionNames();

    if (!actionNames_.empty())
    {
        SelectAction(actionNames_.front());
    }
    else
    {
        selectedActionName.clear();
        renameBuffer.clear();
    }

    auto axisNames_ = BuildAxisNames();

    if (!axisNames_.empty())
    {
        SelectAxis(axisNames_.front());
    }
    else
    {
        selectedAxisName.clear();
        axisRenameBuffer.clear();
    }

    dirty = false;
}

void InputEditorWindow::ApplyDraftToConfig()
{
    auto& keyConfig_ = KeyConfig::Instance();
    keyConfig_.SetConfigFile(configFilePath);

    std::vector<std::string> oldActionNames_;
    for (const auto& pair_ : keyConfig_.GetActionMap())
    {
        oldActionNames_.push_back(pair_.first);
    }

    for (const auto& name_ : oldActionNames_)
    {
        keyConfig_.RemoveAction(name_);
    }

    for (const auto& pair_ : draftActionMap)
    {
        keyConfig_.SetBinds(pair_.first, pair_.second);
    }

    std::vector<std::string> oldAxisNames_;
    for (const auto& pair_ : keyConfig_.GetAxisMap())
    {
        oldAxisNames_.push_back(pair_.first);
    }

    for (const auto& name_ : oldAxisNames_)
    {
        keyConfig_.RemoveAxis(name_);
    }

    for (const auto& pair_ : draftAxisMap)
    {
        keyConfig_.SetAxisBinds(pair_.first, pair_.second);
    }
}


void InputEditorWindow::SelectAction(const std::string& action_)
{
    selectedActionName = action_;
    renameBuffer = action_;
}

std::string InputEditorWindow::BuildUnitLabel(const InputUnit& unit_) const
{
    const auto findName_ = [&](const std::vector<InputOption>& options_) -> const char*
        {
            for (const auto& option_ : options_)
            {
                if (unit_.device == option_.device && unit_.code == option_.code)
                {
                    return option_.name;
                }
            }
            return nullptr;
        };

    if (const char* name_ = findName_(keyboardOptions)) return name_;
    if (const char* name_ = findName_(mouseOptions)) return name_;
    if (const char* name_ = findName_(padOptions)) return name_;

    return (unit_.device == DeviceType::Gamepad ? "Pad Code " : "Key Code ") + std::to_string(unit_.code);
}

int InputEditorWindow::CaptureKeyboardCode() const
{
    for (int code_ = 7; code_ < 256; ++code_)
    {
        if ((GetAsyncKeyState(code_) & 0x8000) != 0)
        {
            return code_;
        }
    }
    return 0;
}

void InputEditorWindow::DrawModeTabs()
{
    if (ImGui::Button(editMode == InputEditMode::Action ? "[Action]" : "Action"))
    {
        editMode = InputEditMode::Action;
    }

    ImGui::SameLine();

    if (ImGui::Button(editMode == InputEditMode::Axis ? "[Axis]" : "Axis"))
    {
        editMode = InputEditMode::Axis;
    }
}



std::vector<std::string> InputEditorWindow::BuildAxisNames() const
{
    std::vector<std::string> names_;

    for (const auto& pair_ : draftAxisMap)
    {
        names_.push_back(pair_.first);
    }

    std::sort(names_.begin(), names_.end());
    return names_;
}

AxisBind* InputEditorWindow::GetSelectedAxisBinds()
{
    auto it_ = draftAxisMap.find(selectedAxisName);
    return it_ == draftAxisMap.end() ? nullptr : &it_->second;
}

const AxisBind* InputEditorWindow::GetSelectedAxisBinds() const
{
    auto it_ = draftAxisMap.find(selectedAxisName);
    return it_ == draftAxisMap.end() ? nullptr : &it_->second;
}

void InputEditorWindow::SelectAxis(const std::string& axisName_)
{
    selectedAxisName = axisName_;
    axisRenameBuffer = axisName_;
}

void InputEditorWindow::DrawAxisList()
{
    ImGui::InputText("New Axis", &newAxisName);

    if (ImGui::Button("Add Axis", ImVec2(-1.0f, 0.0f)))
    {
        if (!newAxisName.empty() && draftAxisMap.find(newAxisName) == draftAxisMap.end())
        {
            draftAxisMap[newAxisName] = {};
            SelectAxis(newAxisName);
            dirty = true;
        }
    }

    ImGui::Separator();

    auto names_ = BuildAxisNames();

    for (const auto& name_ : names_)
    {
        if (ImGui::Selectable(name_.c_str(), selectedAxisName == name_))
        {
            SelectAxis(name_);
        }
    }
}

void InputEditorWindow::DrawAxisEditor()
{
    auto* binds_ = GetSelectedAxisBinds();

    if (!binds_)
    {
        ImGui::TextDisabled("No axis selected.");
        return;
    }

    ImGui::Text("Axis: %s", selectedAxisName.c_str());

    ImGui::SetNextItemWidth(280.0f);
    ImGui::InputText("Name", &axisRenameBuffer);

    ImGui::SameLine();

    if (ImGui::Button("Rename"))
    {
        if (!axisRenameBuffer.empty() && draftAxisMap.find(axisRenameBuffer) == draftAxisMap.end())
        {
            auto movedBinds_ = std::move(*binds_);
            draftAxisMap.erase(selectedAxisName);
            draftAxisMap[axisRenameBuffer] = std::move(movedBinds_);
            SelectAxis(axisRenameBuffer);
            dirty = true;
        }
    }

    ImGui::SameLine();

    if (ImGui::Button("Delete Axis"))
    {
        draftAxisMap.erase(selectedAxisName);
        selectedAxisName.clear();
        axisRenameBuffer.clear();
        dirty = true;
        return;
    }

    ImGui::Separator();

    if (ImGui::Button("Add Axis Unit"))
    {
        AxisUnit unit_{};
        unit_.sourceType = AxisSourceType::Button;
        unit_.device = DeviceType::KBM;
        unit_.code = 'W';
        unit_.scale = 1.0f;
        unit_.deadZone = 0.15f;

        binds_->push_back(unit_);
        dirty = true;
    }

    ImGui::Separator();

    for (int unitIndex_ = 0; unitIndex_ < static_cast<int>(binds_->size()); ++unitIndex_)
    {
        ImGui::PushID(unitIndex_);
        DrawAxisUnitEditor((*binds_)[unitIndex_], unitIndex_);
        ImGui::PopID();
    }
}

void InputEditorWindow::DrawAxisUnitEditor(AxisUnit& unit_, int unitIndex_)
{
    ImGui::Separator();

    ImGui::Text("Axis Unit %d", unitIndex_ + 1);

    ImGui::SameLine();

    if (ImGui::SmallButton("Remove"))
    {
        auto* binds_ = GetSelectedAxisBinds();

        if (binds_ && unitIndex_ >= 0 && unitIndex_ < static_cast<int>(binds_->size()))
        {
            binds_->erase(binds_->begin() + unitIndex_);
            dirty = true;
        }

        return;
    }

    DrawAxisSourcePicker(unit_);
    DrawAxisCodePicker(unit_);

    ImGui::SetNextItemWidth(120.0f);
    if (ImGui::DragFloat("Scale", &unit_.scale, 0.01f, -10.0f, 10.0f))
    {
        dirty = true;
    }

    if (unit_.sourceType == AxisSourceType::GamepadStick)
    {
        ImGui::SetNextItemWidth(120.0f);

        if (ImGui::DragFloat("Dead Zone", &unit_.deadZone, 0.01f, 0.0f, 1.0f))
        {
            dirty = true;
        }
    }

    ImGui::TextDisabled("Preview: %s", BuildAxisUnitLabel(unit_).c_str());
}

void InputEditorWindow::DrawAxisSourcePicker(AxisUnit& unit_)
{
    const char* sourceNames_[] =
    {
        "Button",
        "Mouse Move",
        "Gamepad Stick"
    };

    int sourceIndex_ = static_cast<int>(unit_.sourceType);

    ImGui::SetNextItemWidth(180.0f);

    if (ImGui::Combo(
        "Source Type",
        &sourceIndex_,
        sourceNames_,
        IM_ARRAYSIZE(sourceNames_)))
    {
        unit_.sourceType =
            static_cast<AxisSourceType>(sourceIndex_);

        switch (unit_.sourceType)
        {
        case AxisSourceType::Button:
            unit_.device = DeviceType::KBM;
            unit_.code = 'W';
            unit_.scale = 1.0f;
            break;

        case AxisSourceType::MouseMove:
            unit_.device = DeviceType::KBM;
            unit_.code = static_cast<int>(AxisCode::MouseX);
            unit_.scale = 1.0f;
            break;

        case AxisSourceType::GamepadStick:
            unit_.device = DeviceType::Gamepad;
            unit_.code = static_cast<int>(AxisCode::LeftStickX);
            unit_.scale = 1.0f;
            unit_.deadZone = 0.15f;
            break;
        }

        dirty = true;
    }
}

void InputEditorWindow::DrawAxisCodePicker(AxisUnit& unit_)
{
    if (unit_.sourceType == AxisSourceType::Button)
    {
        InputUnit inputUnit_{ unit_.device, unit_.code };

        DrawInputPicker(inputUnit_);

        if (inputUnit_.device != unit_.device || inputUnit_.code != unit_.code)
        {
            unit_.device = inputUnit_.device;
            unit_.code = inputUnit_.code;
            dirty = true;
        }

        return;
    }

    if (unit_.sourceType == AxisSourceType::MouseMove)
    {
        const std::string preview_ = BuildAxisUnitLabel(unit_);

        ImGui::SetNextItemWidth(220.0f);

        if (ImGui::BeginCombo("Axis Code", preview_.c_str()))
        {
            for (const auto& option_ : mouseAxisOptions)
            {
                const bool selected_ = unit_.code == static_cast<int>(option_.code);

                if (ImGui::Selectable(option_.name, selected_))
                {
                    unit_.device = DeviceType::KBM;
                    unit_.code = static_cast<int>(option_.code);
                    dirty = true;
                }
            }

            ImGui::EndCombo();
        }

        return;
    }

    if (unit_.sourceType == AxisSourceType::GamepadStick)
    {
        const std::string preview_ = BuildAxisUnitLabel(unit_);

        ImGui::SetNextItemWidth(220.0f);

        if (ImGui::BeginCombo("Axis Code", preview_.c_str()))
        {
            for (const auto& option_ : stickAxisOptions)
            {
                const bool selected_ = unit_.code == static_cast<int>(option_.code);

                if (ImGui::Selectable(option_.name, selected_))
                {
                    unit_.device = DeviceType::Gamepad;
                    unit_.code = static_cast<int>(option_.code);
                    dirty = true;
                }
            }

            ImGui::EndCombo();
        }

        return;
    }
}



std::string InputEditorWindow::BuildAxisUnitLabel(const AxisUnit& unit_) const
{
    if (unit_.sourceType == AxisSourceType::Button)
    {
        InputUnit inputUnit_{ unit_.device, unit_.code };
        return BuildUnitLabel(inputUnit_);
    }

    if (unit_.sourceType == AxisSourceType::MouseMove)
    {
        for (const auto& option_ : mouseAxisOptions)
        {
            if (unit_.code == static_cast<int>(option_.code))
            {
                return option_.name;
            }
        }

        return "Mouse Axis " + std::to_string(unit_.code);
    }

    if (unit_.sourceType == AxisSourceType::GamepadStick)
    {
        for (const auto& option_ : stickAxisOptions)
        {
            if (unit_.code == static_cast<int>(option_.code))
            {
                return option_.name;
            }
        }

        return "Stick Axis " + std::to_string(unit_.code);
    }

    return "Unknown Axis";
}
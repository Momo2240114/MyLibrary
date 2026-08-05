#pragma once

#include "EditorWindow.h"
#include "Engine/GamePlay/Input/KeyConfig.h"

#include <string>
#include <unordered_map>
#include <vector>

class InputEditorWindow : public EditorWindow
{
public:
    explicit InputEditorWindow(GameObjectManager* manager_);
    void Draw() override;

private:
    enum class InputEditMode
    {
        Action = 0,
        Axis
    };

    struct InputOption
    {
        // UI上の表示名と、実際に保存するDeviceType/codeの対応。
        const char* name;
        DeviceType device;
        int code;
    };

    struct AxisOption
    {
        // マウス移動やスティック軸のような連続値入力の表示名とコード。
        const char* name;
        AxisCode code;
    };

private:
    // ===== Common =====
    void DrawToolbar();
    void DrawModeTabs();
    void DrawStatusLine();
    void RefreshDraftFromConfig();
    void ApplyDraftToConfig();

    // ===== Action =====
    std::vector<std::string> BuildActionNames() const;

    std::vector<InputCombo>* GetSelectedCombos();
    const std::vector<InputCombo>* GetSelectedCombos() const;

    void DrawActionList();
    void DrawActionEditor();
    void DrawComboEditor(InputCombo& combo_, int comboIndex_);
    void DrawUnitEditor(InputCombo& combo_, InputUnit& unit_, int unitIndex_);
    void DrawInputPicker(InputUnit& unit_);

    void SelectAction(const std::string& action_);
    std::string BuildUnitLabel(const InputUnit& unit_) const;

    // ===== Axis =====
    std::vector<std::string> BuildAxisNames() const;

    AxisBind* GetSelectedAxisBinds();
    const AxisBind* GetSelectedAxisBinds() const;

    void DrawAxisList();
    void DrawAxisEditor();
    void DrawAxisUnitEditor(AxisUnit& unit_, int unitIndex_);
    void DrawAxisSourcePicker(AxisUnit& unit_);
    void DrawAxisCodePicker(AxisUnit& unit_);

    void SelectAxis(const std::string& axisName_);
    std::string BuildAxisUnitLabel(const AxisUnit& unit_) const;

    // ===== Capture =====
    void DrawCapturePopup();
    int CaptureKeyboardCode() const;

private:
    InputEditMode editMode = InputEditMode::Action;

    std::vector<InputOption> keyboardOptions;
    std::vector<InputOption> mouseOptions;
    std::vector<InputOption> padOptions;

    std::vector<AxisOption> mouseAxisOptions;
    std::vector<AxisOption> stickAxisOptions;

    // KeyConfigを直接変更せず、保存ボタンまで編集内容を保持する作業用マップ。
    std::unordered_map<std::string, std::vector<InputCombo>> draftActionMap;
    std::unordered_map<std::string, AxisBind> draftAxisMap;

    std::string selectedActionName;
    std::string renameBuffer;
    std::string newActionName = "NewAction";

    std::string selectedAxisName;
    std::string axisRenameBuffer;
    std::string newAxisName = "NewAxis";

    std::string configFilePath = "Data/Input/KeyConfig.json";
    std::string statusMessage;

    // 未保存変更があるかを示し、ToolbarのModified表示に使う。
    bool dirty = false;

    // Capture中に書き戻すInputUnitを一時的に保持する。Popup終了時に必ずnullptrへ戻す。
    bool capturePopupOpen = false;
    InputUnit* captureTarget = nullptr;
};

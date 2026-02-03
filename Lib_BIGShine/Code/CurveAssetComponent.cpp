#include "CurveAssetComponent.h"
#include "CurveEditor.h"

const std::string& CurveAssetComponent::GetName() const
{
    static std::string n = "CurveAssetComponent";
    return n;
}

void CurveAssetComponent::SaveCurve()
{
    if (filePath.empty())
    {
        // デフォルト保存パス：プロジェクトの "Curves/" フォルダに name.json
        filePath = "Curves/" + curveData.name + ".json";
    }

    CurveSerializer::Save(filePath, curveData);
}

void CurveAssetComponent::LoadCurve(const std::string& filename)
{
    filePath = "Curves/" + filename;
    CurveSerializer::Load(filePath, curveData);
}

void CurveAssetComponent::Gui()
{
    ImGui::Text("Curve Asset Component");

    // ----- ファイル名 / curveData.name の編集 -----
    char buffer[128];
    strncpy_s(buffer, curveData.name.c_str(), sizeof(buffer));
    if (ImGui::InputText("Curve Name", buffer, sizeof(buffer)))
    {
        curveData.name = buffer;
    }

    ImGui::Text("File Path: %s", filePath.c_str());

    if (ImGui::Button("Save Curve"))
        SaveCurve();

    ImGui::SameLine();
    if (ImGui::Button("Load Curve"))
    {
        // 実際はファイルダイアログで選択させる方が良い
        std::string fileToLoad = curveData.name + ".json";
        LoadCurve(fileToLoad);
    }

    // ----- CurveEditor を呼び出す -----
    static CurveEditor editor;
    ImGui::Separator();
    editor.Draw(curveData);
}
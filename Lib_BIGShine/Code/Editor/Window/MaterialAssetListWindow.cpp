#include "PrecompiledHeader.h"

#include "MaterialAssetListWindow.h"

#include "Editor/AssetBrowserPayload.h"
#include "Engine/Graphics/VFXMaterial/VFXMaterialSerializer.h"
#include "Engine/Platform/WindowsDialogHelper.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <imgui.h>
#include <unordered_map>
#include <utility>

MaterialAssetListWindow::MaterialAssetListWindow(GameObjectManager* manager_)
    : EditorWindow("Material Asset Keys", manager_)
{
    defaultWindowSize = ImVec2(920.0f, 560.0f);
    SetVisible(false);
    Refresh();
}

void MaterialAssetListWindow::Draw()
{
    BS_PROFILE_SCOPE("MaterialAssetListWindow::Draw");

    if (!BeginEditorWindow(nullptr, ImGuiWindowFlags_NoCollapse))
    {
        EndEditorWindow();
        return;
    }

    DrawToolbar();
    DrawMaterialTable();
    EndEditorWindow();
}

void MaterialAssetListWindow::Refresh()
{
    BS_PROFILE_SCOPE("MaterialAssetListWindow::Refresh");

    materialAssets.clear();
    LoadMaterialAssets("Data/VFXMaterials");

    std::unordered_map<std::string, int> keyCount_;
    for (const MaterialAssetEntry& entry_ : materialAssets)
    {
        ++keyCount_[entry_.key];
    }

    for (MaterialAssetEntry& entry_ : materialAssets)
    {
        if (keyCount_[entry_.key] <= 1) continue;
        entry_.key = BuildAssetKey(entry_.fullPath, "Data/VFXMaterials");
    }

    std::sort(
        materialAssets.begin(),
        materialAssets.end(),
        [](const MaterialAssetEntry& left_, const MaterialAssetEntry& right_)
        {
            return left_.key < right_.key;
        });
}

void MaterialAssetListWindow::DrawToolbar()
{
    BS_PROFILE_SCOPE("MaterialAssetListWindow::DrawToolbar");

    ImGui::InputTextWithHint("##MaterialKeySearch", "Search key, name, file, path...", searchText, sizeof(searchText));
    ImGui::SameLine();
    if (ImGui::Button("Refresh")) Refresh();
    ImGui::SameLine();
    ImGui::Checkbox("Texture Columns", &showTextureColumns);
    ImGui::Text("Count: %d", static_cast<int>(materialAssets.size()));
}

void MaterialAssetListWindow::DrawMaterialTable()
{
    BS_PROFILE_SCOPE("MaterialAssetListWindow::DrawMaterialTable");

    const int columnCount_ = showTextureColumns ? 7 : 4;
    if (!ImGui::BeginTable(
        "MaterialAssetKeyTable",
        columnCount_,
        ImGuiTableFlags_Borders |
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_Resizable |
        ImGuiTableFlags_ScrollY))
    {
        return;
    }

    ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthFixed, 180.0f);
    ImGui::TableSetupColumn("Material", ImGuiTableColumnFlags_WidthFixed, 180.0f);
    ImGui::TableSetupColumn("File", ImGuiTableColumnFlags_WidthFixed, 180.0f);
    ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthStretch);
    if (showTextureColumns)
    {
        ImGui::TableSetupColumn("Base", ImGuiTableColumnFlags_WidthFixed, 160.0f);
        ImGui::TableSetupColumn("Mask", ImGuiTableColumnFlags_WidthFixed, 120.0f);
        ImGui::TableSetupColumn("Noise", ImGuiTableColumnFlags_WidthFixed, 120.0f);
    }
    ImGui::TableHeadersRow();

    for (const MaterialAssetEntry& entry_ : materialAssets)
    {
        if (!MatchesSearch(entry_)) continue;

        ImGui::TableNextRow();
        ImGui::PushID(entry_.fullPath.c_str());

        ImGui::TableSetColumnIndex(0);
        if (ImGui::Selectable(entry_.key.c_str(), false, ImGuiSelectableFlags_SpanAllColumns))
        {
            ImGui::SetClipboardText(entry_.key.c_str());
        }
        BeginMaterialDrag(entry_);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Click: copy key\nDrag: material path");

        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(entry_.materialName.c_str());

        ImGui::TableSetColumnIndex(2);
        ImGui::TextUnformatted(entry_.fileName.c_str());

        ImGui::TableSetColumnIndex(3);
        ImGui::TextUnformatted(entry_.fullPath.c_str());
        if (ImGui::IsItemClicked()) ImGui::SetClipboardText(entry_.fullPath.c_str());
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Click: copy path");

        if (showTextureColumns)
        {
            ImGui::TableSetColumnIndex(4);
            ImGui::TextUnformatted(entry_.baseTexturePath.empty() ? "-" : entry_.baseTexturePath.c_str());
            ImGui::TableSetColumnIndex(5);
            ImGui::TextUnformatted(entry_.maskTexturePath.empty() ? "-" : entry_.maskTexturePath.c_str());
            ImGui::TableSetColumnIndex(6);
            ImGui::TextUnformatted(entry_.noiseTexturePath.empty() ? "-" : entry_.noiseTexturePath.c_str());
        }

        ImGui::PopID();
    }

    ImGui::EndTable();
}

void MaterialAssetListWindow::LoadMaterialAssets(const std::string& rootPath_)
{
    BS_PROFILE_SCOPE("MaterialAssetListWindow::LoadMaterialAssets");

    std::error_code error_;
    const std::filesystem::path root_ = WindowsDialogHelper::Utf8ToPath(rootPath_);
    if (!std::filesystem::is_directory(root_, error_)) return;

    for (const std::filesystem::directory_entry& entry_ : std::filesystem::recursive_directory_iterator(root_, error_))
    {
        if (error_) break;
        if (entry_.is_directory(error_)) continue;

        const std::filesystem::path path_ = entry_.path();
        std::string extension_ = path_.extension().string();
        std::transform(
            extension_.begin(),
            extension_.end(),
            extension_.begin(),
            [](unsigned char character_) { return static_cast<char>(std::tolower(character_)); });

        if (extension_ != ".vfxmat" && extension_ != ".json") continue;

        MaterialAssetEntry materialEntry_{};
        if (!TryReadMaterialEntry(WindowsDialogHelper::PathToUtf8(path_.lexically_normal()), materialEntry_)) continue;
        materialEntry_.key = WindowsDialogHelper::PathToUtf8(path_.stem());
        materialEntry_.fileName = WindowsDialogHelper::PathToUtf8(path_.filename());
        materialAssets.push_back(std::move(materialEntry_));
    }
}

std::string MaterialAssetListWindow::BuildAssetKey(
    const std::string& fullPath_,
    const std::string& rootPath_) const
{
    BS_PROFILE_SCOPE("MaterialAssetListWindow::BuildAssetKey");

    const std::filesystem::path fullPathValue_ = WindowsDialogHelper::Utf8ToPath(fullPath_);
    const std::filesystem::path rootPathValue_ = WindowsDialogHelper::Utf8ToPath(rootPath_);
    std::error_code error_;
    std::filesystem::path relativePath_ = std::filesystem::relative(fullPathValue_, rootPathValue_, error_);
    if (error_) relativePath_ = fullPathValue_.filename();
    relativePath_.replace_extension();
    return WindowsDialogHelper::PathToUtf8(relativePath_.lexically_normal());
}

bool MaterialAssetListWindow::MatchesSearch(const MaterialAssetEntry& entry_) const
{
    BS_PROFILE_SCOPE("MaterialAssetListWindow::MatchesSearch");

    if (searchText[0] == '\0') return true;

    std::string search_ = searchText;
    std::string target_ = entry_.key + " " +
        entry_.materialName + " " +
        entry_.fileName + " " +
        entry_.fullPath + " " +
        entry_.baseTexturePath + " " +
        entry_.maskTexturePath + " " +
        entry_.noiseTexturePath;

    std::transform(
        search_.begin(),
        search_.end(),
        search_.begin(),
        [](unsigned char character_) { return static_cast<char>(std::tolower(character_)); });
    std::transform(
        target_.begin(),
        target_.end(),
        target_.begin(),
        [](unsigned char character_) { return static_cast<char>(std::tolower(character_)); });
    return target_.find(search_) != std::string::npos;
}

bool MaterialAssetListWindow::TryReadMaterialEntry(
    const std::string& fullPath_,
    MaterialAssetEntry& entry_) const
{
    BS_PROFILE_SCOPE("MaterialAssetListWindow::TryReadMaterialEntry");

    VFXMaterialData materialData_{};
    if (!VFXMaterialSerializer::Load(fullPath_, materialData_)) return false;

    entry_.fullPath = fullPath_;
    entry_.materialName = materialData_.materialName;
    entry_.baseTexturePath = materialData_.baseTexturePath;
    entry_.maskTexturePath = materialData_.maskTexturePath;
    entry_.noiseTexturePath = materialData_.noiseTexturePath;
    return true;
}

void MaterialAssetListWindow::BeginMaterialDrag(const MaterialAssetEntry& entry_) const
{
    BS_PROFILE_SCOPE("MaterialAssetListWindow::BeginMaterialDrag");

    if (!ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) return;
    ImGui::SetDragDropPayload(
        AssetBrowserPayload::MATERIAL,
        entry_.fullPath.c_str(),
        entry_.fullPath.size() + 1);
    ImGui::TextUnformatted(entry_.key.c_str());
    ImGui::TextDisabled(entry_.fullPath.c_str());
    ImGui::EndDragDropSource();
}
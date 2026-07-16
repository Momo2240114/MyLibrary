#include "PrecompiledHeader.h"

#include "AssetBrowserWindow.h"

#include "Editor/AssetBrowserPayload.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Graphics/Resource/TextureResourceManager.h"
#include "Engine/Platform/WindowsDialogHelper.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <imgui_internal.h>
#include <nlohmann/json.hpp>

AssetBrowserWindow::AssetBrowserWindow(GameObjectManager* manager_)
    : EditorWindow("Asset Browser", manager_)
{
    isVisible = false;
    defaultWindowSize = ImVec2(840.0f, 560.0f);
    Refresh();
}

void AssetBrowserWindow::Draw()
{
    if (!BeginEditorWindow())
    {
        EndEditorWindow();
        return;
    }

    DrawContent();
    DrawStylePopup();
    EndEditorWindow();
}

void AssetBrowserWindow::DrawContent()
{
    ImGui::PushID(this);
    ImGui::InputTextWithHint("##AssetSearch", "Search current folder...", searchText, sizeof(searchText));
    ImGui::SameLine();
    if (ImGui::Button("Refresh")) Refresh();
    ImGui::SameLine();
    ImGui::SliderFloat("Icon Size", &iconSize, 40.0f, 128.0f, "%.0f");

    const float splitterWidth_ = 5.0f;
    const float contentWidth_ = ImGui::GetContentRegionAvail().x;
    const float minimumPaneWidth_ = (std::max)(80.0f, (contentWidth_ - splitterWidth_) * 0.25f);
    folderTreeWidth = std::clamp(
        folderTreeWidth,
        minimumPaneWidth_,
        (std::max)(minimumPaneWidth_, contentWidth_ - splitterWidth_ - minimumPaneWidth_));
    float gridWidth_ = (std::max)(minimumPaneWidth_, contentWidth_ - folderTreeWidth - splitterWidth_);

    ImGui::BeginChild("AssetFolderTree", ImVec2(folderTreeWidth, 0.0f), true);
    DrawFolderTree(rootNode);
    ImGui::EndChild();

    ImGui::SameLine();
    const ImVec2 splitterPosition_ = ImGui::GetCursorScreenPos();
    const ImRect splitterBounds_(
        splitterPosition_,
        ImVec2(splitterPosition_.x + splitterWidth_, splitterPosition_.y + ImGui::GetContentRegionAvail().y));
    ImGui::SplitterBehavior(
        splitterBounds_,
        ImGui::GetID("##AssetBrowserSplitter"),
        ImGuiAxis_X,
        &folderTreeWidth,
        &gridWidth_,
        minimumPaneWidth_,
        minimumPaneWidth_,
        4.0f);

    ImGui::SameLine();
    ImGui::BeginChild("AssetGrid", ImVec2(0.0f, 0.0f), true);
    DrawGrid();
    ImGui::EndChild();
    ImGui::PopID();
}

void AssetBrowserWindow::Refresh()
{
    rootNode = {};
    rootNode.name = "Data";
    rootNode.path = "Data";
    rootNode.isDirectory = true;
    LoadDirectory("Data", rootNode);
    selectedFolder = selectedFolderPath.empty() ? &rootNode : FindNode(selectedFolderPath, rootNode);
    if (!selectedFolder)
    {
        selectedFolder = &rootNode;
        selectedFolderPath = rootNode.path;
    }
}

void AssetBrowserWindow::LoadDirectory(const std::filesystem::path& path_, AssetNode& node_)
{
    std::error_code error_;
    if (!std::filesystem::is_directory(path_, error_)) return;

    for (const auto& entry_ : std::filesystem::directory_iterator(path_, error_))
    {
        if (error_) break;
        AssetNode child_;
        child_.name = WindowsDialogHelper::PathToUtf8(entry_.path().filename());
        child_.path = WindowsDialogHelper::PathToUtf8(entry_.path().lexically_normal());
        child_.isDirectory = entry_.is_directory(error_);
        child_.type = child_.isDirectory ? AssetType::FILE : ClassifyAsset(entry_.path());
        if (child_.isDirectory) LoadDirectory(entry_.path(), child_);
        node_.children.push_back(std::move(child_));
    }

    std::sort(node_.children.begin(), node_.children.end(),
        [](const AssetNode& left_, const AssetNode& right_)
        {
            if (left_.isDirectory != right_.isDirectory) return left_.isDirectory > right_.isDirectory;
            return left_.name < right_.name;
        });
}

void AssetBrowserWindow::DrawFolderTree(AssetNode& node_)
{
    if (!node_.isDirectory) return;
    ImGui::PushID(node_.path.c_str());
    ImGuiTreeNodeFlags flags_ = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (selectedFolder == &node_) flags_ |= ImGuiTreeNodeFlags_Selected;
    const bool open_ = ImGui::TreeNodeEx("##Folder", flags_, "%s", node_.name.c_str());
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
    {
        selectedFolder = &node_;
        selectedFolderPath = node_.path;
    }
    if (open_)
    {
        for (AssetNode& child_ : node_.children) DrawFolderTree(child_);
        ImGui::TreePop();
    }
    ImGui::PopID();
}

void AssetBrowserWindow::DrawGrid()
{
    AssetNode* folder_ = selectedFolder ? selectedFolder : &rootNode;
    ImGui::TextUnformatted(folder_->path.c_str());
    ImGui::Separator();
    const float cellWidth_ = iconSize + 24.0f;
    const int columnCount_ = (std::max)(1, static_cast<int>(ImGui::GetContentRegionAvail().x / cellWidth_));
    if (!ImGui::BeginTable("AssetGridTable", columnCount_, ImGuiTableFlags_SizingStretchSame)) return;
    for (AssetNode& child_ : folder_->children)
    {
        if (!MatchesSearch(child_)) continue;
        ImGui::TableNextColumn();
        DrawAssetCard(child_);
    }
    ImGui::EndTable();
}

void AssetBrowserWindow::DrawAssetCard(AssetNode& node_)
{
    ImGui::PushID(node_.path.c_str());
    ID3D11ShaderResourceView* texture_ = node_.type == AssetType::TEXTURE
        ? TextureResourceManager::Instance().GetTexture(node_.path)
        : nullptr;
    if (texture_)
    {
        ImGui::Image(reinterpret_cast<void*>(texture_), ImVec2(iconSize, iconSize));
    }
    else
    {
        const char* icon_ = node_.isDirectory ? "[Folder]" :
            (node_.type == AssetType::TEXTURE ? "[Texture]" :
                (node_.type == AssetType::MODEL ? "[Model]" :
                    (node_.type == AssetType::CURVE ? "[Curve]" :
                        (node_.type == AssetType::JSON ? "[JSON]" : "[File]"))));
        ImGui::Button(icon_, ImVec2(iconSize, iconSize));
    }

    if (ImGui::IsItemClicked())
    {
        if (node_.isDirectory)
        {
            selectedFolder = &node_;
            selectedFolderPath = node_.path;
        }
        else
        {
            selectedFilePath = node_.path;
        }
    }
    if (!node_.isDirectory) BeginAssetDrag(node_);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", node_.path.c_str());
    ImGui::TextWrapped("%s", node_.name.c_str());
    ImGui::PopID();
}

void AssetBrowserWindow::BeginAssetDrag(AssetNode& node_)
{
    if (!ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) return;

    const char* payloadType_ = AssetBrowserPayload::FILE;
    if (node_.type == AssetType::TEXTURE)
    {
        payloadType_ = AssetBrowserPayload::TEXTURE;
        ID3D11ShaderResourceView* texture_ = TextureResourceManager::Instance().LoadTexture(
            GraphicsDeviceManager::Instance().GetDevice(), node_.path);
        if (texture_) ImGui::Image(reinterpret_cast<void*>(texture_), ImVec2(64.0f, 64.0f));
    }
    else if (node_.type == AssetType::MODEL)
    {
        payloadType_ = AssetBrowserPayload::MODEL;
    }
    else if (node_.type == AssetType::JSON)
    {
        payloadType_ = AssetBrowserPayload::JSON;
    }
    else if (node_.type == AssetType::CURVE)
    {
        payloadType_ = AssetBrowserPayload::CURVE;
    }
    ImGui::SetDragDropPayload(payloadType_, node_.path.c_str(), node_.path.size() + 1);
    ImGui::TextUnformatted(node_.name.c_str());
    ImGui::EndDragDropSource();
}

AssetBrowserWindow::AssetNode* AssetBrowserWindow::FindNode(const std::string& path_, AssetNode& node_)
{
    if (node_.path == path_) return &node_;
    for (AssetNode& child_ : node_.children)
    {
        if (AssetNode* found_ = FindNode(path_, child_)) return found_;
    }
    return nullptr;
}

bool AssetBrowserWindow::MatchesSearch(const AssetNode& node_) const
{
    if (searchText[0] == '\0') return true;
    std::string name_ = node_.name;
    std::string search_ = searchText;
    std::transform(name_.begin(), name_.end(), name_.begin(),
        [](unsigned char character_) { return static_cast<char>(std::tolower(character_)); });
    std::transform(search_.begin(), search_.end(), search_.begin(),
        [](unsigned char character_) { return static_cast<char>(std::tolower(character_)); });
    return name_.find(search_) != std::string::npos;
}

AssetBrowserWindow::AssetType AssetBrowserWindow::ClassifyAsset(const std::filesystem::path& path_) const
{
    std::string extension_ = path_.extension().string();
    std::transform(extension_.begin(), extension_.end(), extension_.begin(),
        [](unsigned char character_) { return static_cast<char>(std::tolower(character_)); });
    if (extension_ == ".png" || extension_ == ".jpg" || extension_ == ".jpeg" ||
        extension_ == ".bmp" || extension_ == ".tga" || extension_ == ".dds" || extension_ == ".hdr")
    {
        return AssetType::TEXTURE;
    }
    if (extension_ == ".gltf" || extension_ == ".glb" || extension_ == ".fbx" || extension_ == ".obj")
    {
        return AssetType::MODEL;
    }
    if (extension_ == ".json")
    {
        std::ifstream file_(path_);
        if (file_.is_open())
        {
            try
            {
                nlohmann::json json_;
                file_ >> json_;
                if (json_.value("AssetType", "") == "CurveTrack") return AssetType::CURVE;
            }
            catch (const nlohmann::json::exception&)
            {
            }
        }
        return AssetType::JSON;
    }
    return AssetType::FILE;
}

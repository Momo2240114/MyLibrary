#include "PrecompiledHeader.h"

#include "AssetBrowserWindow.h"

#include "Editor/AssetBrowserPayload.h"
#include "Engine/Debug/TracyProfiler.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Graphics/Resource/TextureResourceManager.h"
#include "Engine/Platform/WindowsDialogHelper.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <imgui_internal.h>
#include <nlohmann/json.hpp>

namespace
{
    constexpr const char* ASSET_BROWSER_ICON_DIRECTORY = "Data/Editor/AssetBrowserIcons";
    constexpr float ASSET_DRAG_PREVIEW_SIZE = 64.0f;
    constexpr float ASSET_SELECTION_ROUNDING = 4.0f;
    constexpr float ASSET_SELECTION_THICKNESS = 2.0f;

    std::string ToLowerString(std::string text_)
    {
        // 検索と拡張子判定を大文字小文字に左右されないよう小文字へそろえる。
        std::transform(text_.begin(), text_.end(), text_.begin(),
            [](unsigned char character_) { return static_cast<char>(std::tolower(character_)); });
        return text_;
    }

    std::string FindExistingIconPath(const std::initializer_list<const char*>& fileNames_)
    {
        // アイコン名は大文字小文字の揺れを許容し、最初に存在したものを採用する。
        for (const char* fileName_ : fileNames_)
        {
            const std::filesystem::path iconPath_ =
                std::filesystem::path(ASSET_BROWSER_ICON_DIRECTORY) / fileName_;
            std::error_code error_;
            if (std::filesystem::exists(iconPath_, error_))
            {
                return WindowsDialogHelper::PathToUtf8(iconPath_.lexically_normal());
            }
        }
        return {};
    }

    void RetainDrawTexture(ID3D11ShaderResourceView* texture_)
    {
        // ImGuiの描画コマンドが実行されるまでSRVを保持し、描画前に解放される事故を避ける。
        if (texture_) texture_->AddRef();
    }

    void ReleaseAssetBrowserDrawTexture(const ImDrawList* parentList_, const ImDrawCmd* command_)
    {
        (void)parentList_;
        ID3D11ShaderResourceView* texture_ =
            static_cast<ID3D11ShaderResourceView*>(command_->UserCallbackData);
        if (texture_) texture_->Release();
    }

    bool DrawRetainedImageButton(
        const char* id_,
        ID3D11ShaderResourceView* texture_,
        const ImVec2& size_)
    {
        RetainDrawTexture(texture_);
        const bool pressed_ = ImGui::ImageButton(
            id_,
            reinterpret_cast<ImTextureID>(texture_),
            size_);
        ImGui::GetWindowDrawList()->AddCallback(
            ReleaseAssetBrowserDrawTexture,
            texture_);
        return pressed_;
    }

    void DrawRetainedImage(ID3D11ShaderResourceView* texture_, const ImVec2& size_)
    {
        RetainDrawTexture(texture_);
        ImGui::Image(
            reinterpret_cast<ImTextureID>(texture_),
            size_);
        ImGui::GetWindowDrawList()->AddCallback(
            ReleaseAssetBrowserDrawTexture,
            texture_);
    }
}

AssetBrowserWindow::AssetBrowserWindow(GameObjectManager* manager_)
    : EditorWindow("Asset Browser", manager_)
{
    BS_PROFILE_SCOPE("AssetBrowserWindow::AssetBrowserWindow");
    isVisible = false;
    defaultWindowSize = ImVec2(840.0f, 560.0f);
    LoadAssetBrowserIcons();
    Refresh();
}

void AssetBrowserWindow::Draw()
{
    BS_PROFILE_SCOPE("AssetBrowserWindow::Draw");
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
    BS_PROFILE_SCOPE("AssetBrowserWindow::DrawContent");

    // 左にフォルダツリー、右に選択フォルダ内のアセットグリッドを並べる基本レイアウト。
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
    BS_PROFILE_SCOPE("AssetBrowserWindow::Refresh");

    // Data配下を再走査し、選択中フォルダが残っていれば同じパスへ復元する。
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
    BS_PROFILE_SCOPE("AssetBrowserWindow::LoadDirectory");

    // 実ファイル階層を再帰的にAssetNodeへ変換し、ファイルは拡張子やjson内容から種類を分類する。
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
    // ディレクトリだけをツリー表示し、クリックされたフォルダを右側グリッドの表示対象にする。
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
    BS_PROFILE_SCOPE("AssetBrowserWindow::DrawGrid");

    // 選択フォルダの子要素をアイコンサイズに合わせて列数計算し、検索に合うものだけカード表示する。
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
    BS_PROFILE_SCOPE("AssetBrowserWindow::DrawAssetCard");

    // アセット1件分のアイコン、選択枠、ドラッグ開始、ツールチップをまとめて描く。
    ImGui::PushID(node_.path.c_str());

    ID3D11ShaderResourceView* texture_ = GetAssetIcon(node_);
    bool pressed_ = false;
    if (texture_)
    {
        pressed_ = DrawRetainedImageButton("##AssetIcon", texture_, ImVec2(iconSize, iconSize));
    }
    else
    {
        pressed_ = ImGui::Button("Asset", ImVec2(iconSize, iconSize));
    }

    const ImVec2 itemMinimum_ = ImGui::GetItemRectMin();
    const ImVec2 itemMaximum_ = ImGui::GetItemRectMax();
    const bool selected_ =
        (node_.isDirectory && selectedFolderPath == node_.path) ||
        (!node_.isDirectory && selectedFilePath == node_.path);
    if (selected_)
    {
        const ImU32 selectionColor_ = ImGui::GetColorU32(ImGuiCol_HeaderActive);
        ImGui::GetWindowDrawList()->AddRect(
            itemMinimum_,
            itemMaximum_,
            selectionColor_,
            ASSET_SELECTION_ROUNDING,
            0,
            ASSET_SELECTION_THICKNESS);
    }

    if (pressed_)
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
    // 他のEditor UIへ渡せるよう、種類別のPayload名とパス文字列をDragDropへ積む。
    if (!ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) return;
    BS_PROFILE_SCOPE("AssetBrowserWindow::BeginAssetDrag");

    const char* payloadType_ = GetPayloadType(node_);
    if (ID3D11ShaderResourceView* texture_ = GetAssetIcon(node_))
    {
        DrawRetainedImage(texture_, ImVec2(ASSET_DRAG_PREVIEW_SIZE, ASSET_DRAG_PREVIEW_SIZE));
    }
    ImGui::SetDragDropPayload(payloadType_, node_.path.c_str(), node_.path.size() + 1);
    ImGui::TextUnformatted(node_.name.c_str());
    ImGui::EndDragDropSource();
}

void AssetBrowserWindow::LoadAssetBrowserIcons()
{
    BS_PROFILE_SCOPE("AssetBrowserWindow::LoadAssetBrowserIcons");

    // 種類別の標準アイコンを起動時に探し、存在しない場合はFallbackで表示できるようにする。
    assetBrowserIcons.folderPath = FindExistingIconPath({ "Folder.png", "folder.png" });
    assetBrowserIcons.modelPath = FindExistingIconPath({ "Model.png", "model.png" });
    assetBrowserIcons.texturePath = FindExistingIconPath({ "Texture.png", "texture.png" });
    assetBrowserIcons.jsonPath = FindExistingIconPath({ "Json.png", "JSON.png", "json.png" });
    assetBrowserIcons.materialPath = FindExistingIconPath({ "Material.png", "material.png" });
    assetBrowserIcons.curvePath = FindExistingIconPath({ "Curve.png", "curve.png" });
    assetBrowserIcons.filePath = FindExistingIconPath({ "File.png", "file.png" });
    assetBrowserIcons.missingPath = FindExistingIconPath({ "Missing.png", "missing.png" });
}

ID3D11ShaderResourceView* AssetBrowserWindow::GetAssetIcon(const AssetNode& node_) const
{
    // テクスチャは実画像をサムネイルにし、それ以外は種類別アイコンを使う。
    if (node_.isDirectory)
    {
        return GetIconTexture(GetFallbackIconPath(assetBrowserIcons.folderPath));
    }

    switch (node_.type)
    {
    case AssetType::TEXTURE:
        if (ID3D11ShaderResourceView* thumbnail_ = GetTextureThumbnail(node_))
        {
            return thumbnail_;
        }
        return GetIconTexture(GetFallbackIconPath(assetBrowserIcons.texturePath));
    case AssetType::MODEL:
        return GetIconTexture(GetFallbackIconPath(assetBrowserIcons.modelPath));
    case AssetType::MATERIAL:
        return GetIconTexture(GetFallbackIconPath(assetBrowserIcons.materialPath));
    case AssetType::CURVE:
        return GetIconTexture(GetFallbackIconPath(assetBrowserIcons.curvePath));
    case AssetType::JSON:
        return GetIconTexture(GetFallbackIconPath(assetBrowserIcons.jsonPath));
    case AssetType::FILE:
    default:
        return GetIconTexture(GetFallbackIconPath(assetBrowserIcons.filePath));
    }
}

ID3D11ShaderResourceView* AssetBrowserWindow::GetIconTexture(const std::string& path_) const
{
    if (path_.empty()) return nullptr;
    TextureResourceManager& manager_ = TextureResourceManager::Instance();
    if (ID3D11ShaderResourceView* texture_ = manager_.GetTexture(path_))
    {
        return texture_;
    }
    return manager_.LoadTexture(GraphicsDeviceManager::Instance().GetDevice(), path_);
}

ID3D11ShaderResourceView* AssetBrowserWindow::GetTextureThumbnail(const AssetNode& node_) const
{
    TextureResourceManager& manager_ = TextureResourceManager::Instance();
    if (ID3D11ShaderResourceView* texture_ = manager_.GetTexture(node_.path))
    {
        return texture_;
    }
    return manager_.LoadTexture(GraphicsDeviceManager::Instance().GetDevice(), node_.path);
}

const std::string& AssetBrowserWindow::GetFallbackIconPath(const std::string& preferredPath_) const
{
    if (!preferredPath_.empty()) return preferredPath_;
    if (!assetBrowserIcons.jsonPath.empty()) return assetBrowserIcons.jsonPath;
    if (!assetBrowserIcons.filePath.empty()) return assetBrowserIcons.filePath;
    if (!assetBrowserIcons.missingPath.empty()) return assetBrowserIcons.missingPath;
    return assetBrowserIcons.texturePath;
}

const char* AssetBrowserWindow::GetPayloadType(const AssetNode& node_) const
{
    // ドロップ先が受け取り種類を判別できるよう、AssetTypeをPayload文字列へ変換する。
    switch (node_.type)
    {
    case AssetType::TEXTURE:
        return AssetBrowserPayload::TEXTURE;
    case AssetType::MODEL:
        return AssetBrowserPayload::MODEL;
    case AssetType::JSON:
        return AssetBrowserPayload::JSON;
    case AssetType::MATERIAL:
        return AssetBrowserPayload::MATERIAL;
    case AssetType::CURVE:
        return AssetBrowserPayload::CURVE;
    case AssetType::FILE:
    default:
        return AssetBrowserPayload::FILE;
    }
}

AssetBrowserWindow::AssetNode* AssetBrowserWindow::FindNode(const std::string& path_, AssetNode& node_)
{
    // Refresh後も選択フォルダを保てるよう、保存していたパスから新しいツリー内のノードを探す。
    if (node_.path == path_) return &node_;
    for (AssetNode& child_ : node_.children)
    {
        if (AssetNode* found_ = FindNode(path_, child_)) return found_;
    }
    return nullptr;
}

bool AssetBrowserWindow::MatchesSearch(const AssetNode& node_) const
{
    // 検索は現在フォルダ内の名前だけを対象にし、フォルダ移動なしで一覧を素早く絞る。
    if (searchText[0] == '\0') return true;
    std::string name_ = ToLowerString(node_.name);
    std::string search_ = ToLowerString(searchText);
    return name_.find(search_) != std::string::npos;
}

bool AssetBrowserWindow::HasPathComponent(
    const std::filesystem::path& path_,
    const std::string& componentName_) const
{
    const std::filesystem::path normalizedPath_ = path_.lexically_normal();
    const std::string targetName_ = ToLowerString(componentName_);
    for (const std::filesystem::path& component_ : normalizedPath_)
    {
        if (ToLowerString(component_.string()) == targetName_)
        {
            return true;
        }
    }
    return false;
}

AssetBrowserWindow::AssetType AssetBrowserWindow::ClassifyAsset(const std::filesystem::path& path_) const
{
    // 拡張子を基本にしつつ、jsonはフォルダ名やAssetTypeキーも見てCurve/VFXMaterialを判定する。
    std::string extension_ = ToLowerString(path_.extension().string());
    if (extension_ == ".png" || extension_ == ".jpg" || extension_ == ".jpeg" ||
        extension_ == ".bmp" || extension_ == ".tga" || extension_ == ".dds" || extension_ == ".hdr")
    {
        return AssetType::TEXTURE;
    }
    if (extension_ == ".gltf" || extension_ == ".glb" || extension_ == ".fbx" || extension_ == ".obj")
    {
        return AssetType::MODEL;
    }
    if (extension_ == ".vfxmat")
    {
        return AssetType::MATERIAL;
    }
    if (extension_ == ".json")
    {
        if (HasPathComponent(path_, "VFXMaterial"))
        {
            return AssetType::MATERIAL;
        }
        if (HasPathComponent(path_, "Curve"))
        {
            return AssetType::CURVE;
        }

        std::ifstream file_(path_);
        if (file_.is_open())
        {
            try
            {
                nlohmann::json json_;
                file_ >> json_;
                if (json_.value("AssetType", "") == "CurveTrack") return AssetType::CURVE;
                if (json_.value("AssetType", "") == "VFXMaterial") return AssetType::MATERIAL;
            }
            catch (const nlohmann::json::exception&)
            {
            }
        }
        return AssetType::JSON;
    }
    return AssetType::FILE;
}

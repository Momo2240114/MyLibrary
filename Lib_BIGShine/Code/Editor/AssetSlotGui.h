#pragma once

#include "Editor/AssetBrowserPayload.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Platform/WindowsDialogHelper.h"

#include <filesystem>
#include <imgui.h>
#include <string>

enum class AssetSlotType
{
    // AssetBrowserのPayload種別と、開くファイルダイアログの種類を合わせるための分類。
    TEXTURE,
    MODEL,
    JSON,
    MATERIAL,
};

namespace AssetSlotGui
{
    inline void ReleasePreviewTexture(
        const ImDrawList*,
        const ImDrawCmd* command_)
    {
        // ImGuiの描画コマンド完了後に、ImageButton用にAddRefしたSRVをReleaseする。
        ID3D11ShaderResourceView* texture_ =
            static_cast<ID3D11ShaderResourceView*>(command_->UserCallbackData);
        if (texture_)
        {
            texture_->Release();
        }
    }

    inline const char* GetPayloadType(AssetSlotType type_)
    {
        // AssetSlotの種類から、受け入れるDragDrop Payload名へ変換する。
        switch (type_)
        {
        case AssetSlotType::TEXTURE: return AssetBrowserPayload::TEXTURE;
        case AssetSlotType::MODEL: return AssetBrowserPayload::MODEL;
        case AssetSlotType::JSON: return AssetBrowserPayload::JSON;
        case AssetSlotType::MATERIAL: return AssetBrowserPayload::MATERIAL;
        }
        return AssetBrowserPayload::FILE;
    }

    inline const char* GetEmptyLabel(AssetSlotType type_)
    {
        // 未選択時のボタン表示をAsset種別ごとに変える。
        switch (type_)
        {
        case AssetSlotType::TEXTURE: return "[Texture]";
        case AssetSlotType::MODEL: return "[Model]";
        case AssetSlotType::JSON: return "[JSON]";
        case AssetSlotType::MATERIAL: return "[Material]";
        }
        return "[Asset]";
    }

    inline std::string OpenDialog(
        AssetSlotType type_,
        const std::string& path_,
        const std::string& initialDirectory_)
    {
        // 現在のパスがあればその親フォルダ、無ければ初期フォルダを開く。
        std::string directory_ = initialDirectory_;
        if (!path_.empty())
        {
            directory_ = WindowsDialogHelper::PathToUtf8(
                WindowsDialogHelper::Utf8ToPath(path_).parent_path());
        }
        HWND window_ = GraphicsDeviceManager::Instance().GetWindowHandle();
        switch (type_)
        {
        case AssetSlotType::TEXTURE:
            return WindowsDialogHelper::OpenTextureDialog(L"Select Texture", directory_, window_);
        case AssetSlotType::MODEL:
            return WindowsDialogHelper::OpenGltfDialog(L"Select Model", directory_, window_);
        case AssetSlotType::JSON:
            return WindowsDialogHelper::OpenJsonDialog(L"Select JSON Asset", directory_, window_);
        case AssetSlotType::MATERIAL:
            return WindowsDialogHelper::OpenVFXMaterialDialog(L"Select VFX Material", directory_, window_);
        }
        return {};
    }

    inline bool Draw(
        const char* label_,
        AssetSlotType type_,
        std::string& path_,
        ID3D11ShaderResourceView* previewTexture_ = nullptr,
        const char* initialDirectory_ = "Data")
    {
        // プレビュー画像、Drag&Drop、Browse、Clearを1つにまとめたAssetパス編集UI。
        bool changed_ = false;
        ImGui::PushID(label_);
        ImGui::TextUnformatted(label_);

        bool browseRequested_ = false;
        const ImVec2 previewSize_(72.0f, 72.0f);
        if (previewTexture_)
        {
            // ImGuiの描画コマンドには生SRVだけが残るため、同フレーム中のリソース再構築に備えて描画完了まで保持する。
            previewTexture_->AddRef();
            browseRequested_ = ImGui::ImageButton(
                "##AssetPreview",
                reinterpret_cast<ImTextureID>(previewTexture_),
                previewSize_);
            ImGui::GetWindowDrawList()->AddCallback(
                ReleasePreviewTexture,
                previewTexture_);
        }
        else
        {
            browseRequested_ = ImGui::Button(GetEmptyLabel(type_), previewSize_);
        }

        if (ImGui::BeginDragDropTarget())
        {
            // AssetBrowserから対応するPayloadを落とした場合は、即パスを差し替える。
            if (const ImGuiPayload* payload_ = ImGui::AcceptDragDropPayload(GetPayloadType(type_)))
            {
                path_ = static_cast<const char*>(payload_->Data);
                changed_ = true;
            }
            ImGui::EndDragDropTarget();
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Click to open a dialog or drop an asset from Asset Browser");
        }

        ImGui::SameLine();
        ImGui::BeginGroup();
        const std::string fileName_ = path_.empty()
            ? "Not selected"
            : WindowsDialogHelper::PathToUtf8(WindowsDialogHelper::Utf8ToPath(path_).filename());
        ImGui::TextWrapped("%s", fileName_.c_str());
        ImGui::TextDisabled("%s", path_.empty() ? "Drop an asset here" : path_.c_str());
        if (ImGui::SmallButton("Browse...")) browseRequested_ = true;
        ImGui::SameLine();
        if (ImGui::SmallButton("Clear") && !path_.empty())
        {
            path_.clear();
            changed_ = true;
        }
        ImGui::EndGroup();

        if (browseRequested_)
        {
            const std::string selectedPath_ = OpenDialog(type_, path_, initialDirectory_);
            if (!selectedPath_.empty())
            {
                path_ = selectedPath_;
                changed_ = true;
            }
        }
        ImGui::PopID();
        return changed_;
    }
}

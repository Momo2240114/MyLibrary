#pragma once

#include "Editor/AssetBrowserPayload.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Platform/WindowsDialogHelper.h"

#include <filesystem>
#include <imgui.h>
#include <string>

enum class AssetSlotType
{
    TEXTURE,
    MODEL,
    JSON,
};

namespace AssetSlotGui
{
    inline const char* GetPayloadType(AssetSlotType type_)
    {
        switch (type_)
        {
        case AssetSlotType::TEXTURE: return AssetBrowserPayload::TEXTURE;
        case AssetSlotType::MODEL: return AssetBrowserPayload::MODEL;
        case AssetSlotType::JSON: return AssetBrowserPayload::JSON;
        }
        return AssetBrowserPayload::FILE;
    }

    inline const char* GetEmptyLabel(AssetSlotType type_)
    {
        switch (type_)
        {
        case AssetSlotType::TEXTURE: return "[Texture]";
        case AssetSlotType::MODEL: return "[Model]";
        case AssetSlotType::JSON: return "[JSON]";
        }
        return "[Asset]";
    }

    inline std::string OpenDialog(
        AssetSlotType type_,
        const std::string& path_,
        const std::string& initialDirectory_)
    {
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
        bool changed_ = false;
        ImGui::PushID(label_);
        ImGui::TextUnformatted(label_);

        bool browseRequested_ = false;
        const ImVec2 previewSize_(72.0f, 72.0f);
        if (previewTexture_)
        {
            browseRequested_ = ImGui::ImageButton(
                "##AssetPreview",
                reinterpret_cast<ImTextureID>(previewTexture_),
                previewSize_);
        }
        else
        {
            browseRequested_ = ImGui::Button(GetEmptyLabel(type_), previewSize_);
        }

        if (ImGui::BeginDragDropTarget())
        {
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

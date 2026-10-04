#include "PrecompiledHeader.h"

#include "IBLSettingsWindow.h"

#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Platform/WindowsDialogHelper.h"

#include <algorithm>
#include <cstring>
#include <filesystem>

namespace
{
    constexpr const char* DEFAULT_HDR_PATH = "Data/Texture/IBL/qwantani_dusk_2_2k.hdr";
    constexpr float DEFAULT_HDR_INTENSITY = 0.002f;

    void CopyToBuffer(const std::string& text_, std::array<char, 260>& buffer_)
    {
        buffer_.fill('\0');
        strncpy_s(buffer_.data(), buffer_.size(), text_.c_str(), _TRUNCATE);
    }
}

IBLSettingsWindow::IBLSettingsWindow(GameObjectManager* manager_)
    : EditorWindow("IBL Settings", manager_)
{
    defaultWindowSize = ImVec2(460.0f, 210.0f);
}

void IBLSettingsWindow::OnVisibilityChanged(bool visible_)
{
    if (visible_)
    {
        SyncFromGraphicsSettings();
    }
}

void IBLSettingsWindow::SyncFromGraphicsSettings()
{
    auto& graphics_ = GraphicsDeviceManager::Instance();
    const std::string currentPath_ = WindowsDialogHelper::PathToUtf8(
        std::filesystem::path(graphics_.GetIBLSpecularHdrPath()));
    CopyToBuffer(currentPath_, hdrPathBuffer);
    hdrIntensity = graphics_.GetIBLHdrIntensity();
    hasSyncedFromGraphics = true;
}

void IBLSettingsWindow::Draw()
{
    if (!isVisible)
    {
        return;
    }

    if (!hasSyncedFromGraphics)
    {
        SyncFromGraphicsSettings();
    }

    if (BeginEditorWindow())
    {
        DrawStylePopup();

        ImGui::TextUnformatted("Specular HDRI");
        ImGui::SetNextItemWidth(-90.0f);
        ImGui::InputText("##IBLHdrPath", hdrPathBuffer.data(), hdrPathBuffer.size());
        ImGui::SameLine();
        if (ImGui::Button("Browse"))
        {
            const std::string selectedPath_ = WindowsDialogHelper::OpenTextureDialog(
                L"Select IBL HDRI",
                "Data/Texture/IBL",
                GraphicsDeviceManager::Instance().GetWindowHandle());
            if (!selectedPath_.empty())
            {
                CopyToBuffer(selectedPath_, hdrPathBuffer);
            }
        }

        ImGui::SetNextItemWidth(180.0f);
        ImGui::DragFloat("HDR IBL Intensity", &hdrIntensity, 0.0005f, 0.0f, 2.0f, "%.4f");

        if (ImGui::Button("Apply"))
        {
            const std::filesystem::path hdrPath_ = WindowsDialogHelper::Utf8ToPath(hdrPathBuffer.data());
            if (GraphicsDeviceManager::Instance().ReloadIBLTextures(hdrPath_.wstring(), hdrIntensity))
            {
                statusMessage = "IBL settings applied.";
            }
            else
            {
                statusMessage = GraphicsDeviceManager::Instance().GetLastIBLError();
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Reload Current"))
        {
            if (GraphicsDeviceManager::Instance().ReloadIBLTextures())
            {
                statusMessage = "Current IBL reloaded.";
                SyncFromGraphicsSettings();
            }
            else
            {
                statusMessage = GraphicsDeviceManager::Instance().GetLastIBLError();
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Reset"))
        {
            CopyToBuffer(DEFAULT_HDR_PATH, hdrPathBuffer);
            hdrIntensity = DEFAULT_HDR_INTENSITY;
        }

        const std::string& error_ = GraphicsDeviceManager::Instance().GetLastIBLError();
        if (!error_.empty())
        {
            ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.25f, 1.0f), "%s", error_.c_str());
        }
        else if (!statusMessage.empty())
        {
            ImGui::TextDisabled("%s", statusMessage.c_str());
        }
    }
    EndEditorWindow();
}

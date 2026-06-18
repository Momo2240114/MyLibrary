#pragma once

#include <Windows.h>

#include <filesystem>
#include <string>
#include <vector>

class WindowsDialogHelper
{
public:
    struct FilterSpec
    {
        const wchar_t* name;
        const wchar_t* pattern;
    };

    static std::string OpenFileDialog(
        const wchar_t* title_,
        const std::vector<FilterSpec>& filters_,
        const std::string& initialDir_ = "",
        HWND ownerWindow_ = nullptr);

    static std::string SaveFileDialog(
        const wchar_t* title_,
        const std::vector<FilterSpec>& filters_,
        const std::string& initialPath_ = "",
        const wchar_t* defaultExtension_ = nullptr,
        HWND ownerWindow_ = nullptr);

    static std::string OpenTextureDialog(
        const wchar_t* title_,
        const std::string& initialDir_ = "Data/Texture/Particle",
        HWND ownerWindow_ = nullptr);

    static std::string OpenJsonDialog(
        const wchar_t* title_,
        const std::string& initialDir_ = "Data",
        HWND ownerWindow_ = nullptr);

    static std::string SaveJsonDialog(
        const wchar_t* title_,
        const std::string& initialPath_ = "Data",
        HWND ownerWindow_ = nullptr);

    static std::string OpenGltfDialog(
        const wchar_t* title_,
        const std::string& initialDir_ = "Data/Model",
        HWND ownerWindow_ = nullptr);

    static std::string OpenAssetDialog(
        const wchar_t* title_,
        const std::string& initialDir_ = "Data",
        HWND ownerWindow_ = nullptr);

    static std::filesystem::path Utf8ToPath(const std::string& path_);
    static std::string PathToUtf8(const std::filesystem::path& path_);
};

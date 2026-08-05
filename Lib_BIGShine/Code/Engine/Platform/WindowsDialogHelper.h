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
        // ダイアログに表示する種類名。
        const wchar_t* name;

        // "*.json" などのフィルターパターン。
        const wchar_t* pattern;
    };

    // 任意フィルターでファイルを開く共通ダイアログ。
    static std::string OpenFileDialog(
        const wchar_t* title_,
        const std::vector<FilterSpec>& filters_,
            const std::string& initialDir_ = "",
            HWND ownerWindow_ = nullptr);

    // 任意フィルターで保存先を選ぶ共通ダイアログ。
    static std::string SaveFileDialog(
        const wchar_t* title_,
        const std::vector<FilterSpec>& filters_,
        const std::string& initialPath_ = "",
            const wchar_t* defaultExtension_ = nullptr,
            HWND ownerWindow_ = nullptr);

    // Texture選択向けの既定フィルターと初期フォルダを持つOpenDialog。
    static std::string OpenTextureDialog(
        const wchar_t* title_,
            const std::string& initialDir_ = "Data/Texture/Particle",
            HWND ownerWindow_ = nullptr);

    // JSON Asset選択用のOpenDialog。
    static std::string OpenJsonDialog(
        const wchar_t* title_,
            const std::string& initialDir_ = "Data",
            HWND ownerWindow_ = nullptr);

    // JSON Asset保存用のSaveDialog。
    static std::string SaveJsonDialog(
        const wchar_t* title_,
            const std::string& initialPath_ = "Data",
            HWND ownerWindow_ = nullptr);

    // VFXMaterial Asset選択用。
    static std::string OpenVFXMaterialDialog(
        const wchar_t* title_,
            const std::string& initialDir_ = "Data/VFXMaterials",
            HWND ownerWindow_ = nullptr);

    // VFXMaterial Asset保存用。
    static std::string SaveVFXMaterialDialog(
        const wchar_t* title_,
            const std::string& initialPath_ = "Data/VFXMaterials",
            HWND ownerWindow_ = nullptr);

    // GLTFモデル選択用。
    static std::string OpenGltfDialog(
        const wchar_t* title_,
            const std::string& initialDir_ = "Data/Model",
            HWND ownerWindow_ = nullptr);

    // 汎用Asset選択用。
    static std::string OpenAssetDialog(
        const wchar_t* title_,
            const std::string& initialDir_ = "Data",
            HWND ownerWindow_ = nullptr);

    // UTF-8文字列とfilesystem::pathの相互変換。日本語パス対応のために使う。
    static std::filesystem::path Utf8ToPath(const std::string& path_);
    static std::string PathToUtf8(const std::filesystem::path& path_);
};

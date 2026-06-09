#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <shobjidl.h>
#include <shlobj.h>
#include <wrl/client.h>


class WindowsDialogHelper
{
public:
    struct FilterSpec
    {
        const wchar_t* name;
        const wchar_t* pattern;
    };

    // 単一ファイル選択ダイアログ（基底）
    // 戻り値: 選択されたパス（スラッシュ区切り）、キャンセル時は空文字
    static std::string OpenFileDialog(
        const wchar_t* title,
        const std::vector<FilterSpec>& filters,
        const std::string& initialDir = "");

    // テクスチャ専用
    static std::string OpenTextureDialog(
        const wchar_t* title,
        const std::string& initialDir = "Data/Texture/Particle");

    // JSON専用
    static std::string OpenJsonDialog(
        const wchar_t* title,
        const std::string& initialDir = "Data");

    // glTF / GLB 専用
    static std::string OpenGltfDialog(
        const wchar_t* title,
        const std::string& initialDir = "Data/Model");

    // 全アセット汎用（テクスチャ・JSON・glTF）
    static std::string OpenAssetDialog(
        const wchar_t* title,
        const std::string& initialDir = "Data");
};

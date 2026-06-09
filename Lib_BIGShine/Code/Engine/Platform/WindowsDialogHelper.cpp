#include "WindowsDialogHelper.h"
#include <algorithm>
#include <string>


std::string WindowsDialogHelper::OpenFileDialog(
    const wchar_t* title,
    const std::vector<FilterSpec>& filters,
    const std::string& initialDir)
{
    Microsoft::WRL::ComPtr<IFileOpenDialog> dialog_;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL,
        IID_IFileOpenDialog, reinterpret_cast<void**>(dialog_.GetAddressOf()))))
        return "";

    // フィルター設定
    std::vector<COMDLG_FILTERSPEC> specs_;
    specs_.reserve(filters.size());
    for (const auto& f : filters)
        specs_.push_back({ f.name, f.pattern });
    dialog_->SetFileTypes(static_cast<UINT>(specs_.size()), specs_.data());

    // タイトル
    dialog_->SetTitle(title);

    // 初期ディレクトリ（SHCreateItemFromParsingName はフルパス必須）
    if (!initialDir.empty())
    {
        std::filesystem::path absPath_ = std::filesystem::absolute(initialDir);
        std::wstring wdir_ = absPath_.wstring();
        Microsoft::WRL::ComPtr<IShellItem> shellItem_;
        if (SUCCEEDED(SHCreateItemFromParsingName(wdir_.c_str(), nullptr,
            IID_IShellItem, reinterpret_cast<void**>(shellItem_.GetAddressOf()))))
        {
            dialog_->SetFolder(shellItem_.Get());
        }
    }

    // ダイアログ表示（キャンセル時は空文字を返す）
    if (FAILED(dialog_->Show(nullptr))) return "";

    Microsoft::WRL::ComPtr<IShellItem> result_;
    if (FAILED(dialog_->GetResult(result_.GetAddressOf()))) return "";

    PWSTR rawPath_ = nullptr;
    if (FAILED(result_->GetDisplayName(SIGDN_FILESYSPATH, &rawPath_))) return "";

    std::wstring wpath_(rawPath_);
    CoTaskMemFree(rawPath_);

    // フルパス → exe基準の相対パスに変換
    std::filesystem::path selectedPath_(wpath_);
    std::filesystem::path basePath_ = std::filesystem::current_path();
    std::string outPath_;
    std::error_code ec_;
    std::filesystem::path relPath_ = std::filesystem::relative(selectedPath_, basePath_, ec_);
    if (!ec_ && !relPath_.empty())
    {
        outPath_ = relPath_.generic_string(); // スラッシュ区切り
    }
    else
    {
        // 相対化できなかった場合はフルパスをスラッシュ区切りで返す
        outPath_ = selectedPath_.generic_string();
    }

    return outPath_;
}

std::string WindowsDialogHelper::OpenTextureDialog(
    const wchar_t* title,
    const std::string& initialDir)
{
    return OpenFileDialog(title,
        {
            { L"Texture Files", L"*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.dds;*.hdr" },
            { L"All Files",     L"*.*" }
        }, initialDir);
}

std::string WindowsDialogHelper::OpenJsonDialog(
    const wchar_t* title,
    const std::string& initialDir)
{
    return OpenFileDialog(title,
        {
            { L"JSON Files", L"*.json" },
            { L"All Files",  L"*.*"   }
        }, initialDir);
}

std::string WindowsDialogHelper::OpenGltfDialog(
    const wchar_t* title,
    const std::string& initialDir)
{
    return OpenFileDialog(title,
        {
            { L"glTF Files", L"*.gltf;*.glb" },
            { L"All Files",  L"*.*"          }
        }, initialDir);
}

std::string WindowsDialogHelper::OpenAssetDialog(
    const wchar_t* title,
    const std::string& initialDir)
{
    return OpenFileDialog(title,
        {
            { L"All Asset Files", L"*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.dds;*.hdr;*.json;*.gltf;*.glb" },
            { L"Texture Files",   L"*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.dds;*.hdr" },
            { L"JSON Files",      L"*.json"       },
            { L"glTF Files",      L"*.gltf;*.glb" },
            { L"All Files",       L"*.*"          }
        }, initialDir);
}
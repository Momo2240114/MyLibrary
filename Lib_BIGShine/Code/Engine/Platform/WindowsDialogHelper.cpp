#include "WindowsDialogHelper.h"

#include <commdlg.h>
#include <filesystem>
#include <system_error>

#pragma comment(lib, "Comdlg32.lib")

namespace
{
    std::wstring Utf8ToWide(const std::string& text_)
    {
        if (text_.empty()) return {};

        const int length_ = MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, text_.c_str(), -1, nullptr, 0);
        if (length_ <= 0)
        {
            return std::filesystem::path(text_).wstring();
        }

        std::wstring result_(static_cast<size_t>(length_), L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text_.c_str(), -1, result_.data(), length_);
        result_.pop_back();
        return result_;
    }

    std::string WideToUtf8(const std::wstring& text_)
    {
        if (text_.empty()) return {};

        const int length_ = WideCharToMultiByte(
            CP_UTF8, 0, text_.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (length_ <= 0) return {};

        std::string result_(static_cast<size_t>(length_), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text_.c_str(), -1, result_.data(), length_, nullptr, nullptr);
        result_.pop_back();
        return result_;
    }

    std::wstring BuildFilterString(const std::vector<WindowsDialogHelper::FilterSpec>& filters_)
    {
        std::wstring filterString_;
        for (const auto& filter_ : filters_)
        {
            if (!filter_.name || !filter_.pattern) continue;
            filterString_.append(filter_.name);
            filterString_.push_back(L'\0');
            filterString_.append(filter_.pattern);
            filterString_.push_back(L'\0');
        }
        filterString_.push_back(L'\0');
        return filterString_;
    }

    std::filesystem::path ResolveInitialDirectory(const std::string& initialDir_)
    {
        std::error_code error_;
        std::filesystem::path directory_ = initialDir_.empty()
            ? std::filesystem::current_path(error_)
            : WindowsDialogHelper::Utf8ToPath(initialDir_);

        if (std::filesystem::is_regular_file(directory_, error_))
        {
            directory_ = directory_.parent_path();
        }
        if (directory_.is_relative())
        {
            directory_ = std::filesystem::absolute(directory_, error_);
        }
        if (error_ || !std::filesystem::is_directory(directory_, error_))
        {
            directory_ = std::filesystem::current_path(error_);
        }
        return directory_;
    }

    std::string MakeProjectRelativePath(const std::filesystem::path& selectedPath_)
    {
        std::error_code error_;
        const std::filesystem::path basePath_ = std::filesystem::current_path(error_);
        if (!error_)
        {
            const std::filesystem::path relativePath_ =
                std::filesystem::relative(selectedPath_, basePath_, error_);
            if (!error_ && !relativePath_.empty())
            {
                const std::wstring relativeText_ = relativePath_.generic_wstring();
                if (relativeText_.rfind(L"..", 0) != 0)
                {
                    return WideToUtf8(relativeText_);
                }
            }
        }
        return WideToUtf8(selectedPath_.generic_wstring());
    }
}

std::string WindowsDialogHelper::OpenFileDialog(
    const wchar_t* title_,
    const std::vector<FilterSpec>& filters_,
    const std::string& initialDir_,
    HWND ownerWindow_)
{
    constexpr DWORD BUFFER_LENGTH = 32768;
    std::vector<wchar_t> fileBuffer_(BUFFER_LENGTH, L'\0');
    const std::wstring filterString_ = BuildFilterString(filters_);
    const std::filesystem::path initialDirectory_ = ResolveInitialDirectory(initialDir_);
    const std::wstring initialDirectoryText_ = initialDirectory_.wstring();

    OPENFILENAMEW dialog_{};
    dialog_.lStructSize = sizeof(dialog_);
    dialog_.hwndOwner = ownerWindow_ ? ownerWindow_ : GetActiveWindow();
    dialog_.lpstrFilter = filterString_.c_str();
    dialog_.nFilterIndex = 1;
    dialog_.lpstrFile = fileBuffer_.data();
    dialog_.nMaxFile = static_cast<DWORD>(fileBuffer_.size());
    dialog_.lpstrInitialDir = initialDirectoryText_.c_str();
    dialog_.lpstrTitle = title_;
    dialog_.Flags =
        OFN_EXPLORER |
        OFN_FILEMUSTEXIST |
        OFN_PATHMUSTEXIST |
        OFN_NOCHANGEDIR |
        OFN_DONTADDTORECENT;

    if (!GetOpenFileNameW(&dialog_)) return {};

    return MakeProjectRelativePath(std::filesystem::path(fileBuffer_.data()));
}

std::string WindowsDialogHelper::SaveFileDialog(
    const wchar_t* title_,
    const std::vector<FilterSpec>& filters_,
    const std::string& initialPath_,
    const wchar_t* defaultExtension_,
    HWND ownerWindow_)
{
    constexpr DWORD BUFFER_LENGTH = 32768;
    std::vector<wchar_t> fileBuffer_(BUFFER_LENGTH, L'\0');
    const std::filesystem::path requestedPath_ = Utf8ToPath(initialPath_);
    const std::filesystem::path initialDirectory_ = ResolveInitialDirectory(
        requestedPath_.has_extension()
            ? PathToUtf8(requestedPath_.parent_path())
            : initialPath_);
    const std::wstring initialDirectoryText_ = initialDirectory_.wstring();
    const std::wstring initialFileName_ = requestedPath_.has_filename()
        ? requestedPath_.filename().wstring()
        : std::wstring();
    if (!initialFileName_.empty())
    {
        wcsncpy_s(fileBuffer_.data(), fileBuffer_.size(), initialFileName_.c_str(), _TRUNCATE);
    }
    const std::wstring filterString_ = BuildFilterString(filters_);

    OPENFILENAMEW dialog_{};
    dialog_.lStructSize = sizeof(dialog_);
    dialog_.hwndOwner = ownerWindow_ ? ownerWindow_ : GetActiveWindow();
    dialog_.lpstrFilter = filterString_.c_str();
    dialog_.nFilterIndex = 1;
    dialog_.lpstrFile = fileBuffer_.data();
    dialog_.nMaxFile = static_cast<DWORD>(fileBuffer_.size());
    dialog_.lpstrInitialDir = initialDirectoryText_.c_str();
    dialog_.lpstrTitle = title_;
    dialog_.lpstrDefExt = defaultExtension_;
    dialog_.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
    if (!GetSaveFileNameW(&dialog_)) return {};
    return MakeProjectRelativePath(std::filesystem::path(fileBuffer_.data()));
}

std::string WindowsDialogHelper::OpenTextureDialog(
    const wchar_t* title_,
    const std::string& initialDir_,
    HWND ownerWindow_)
{
    return OpenFileDialog(title_,
        {
            { L"Texture Files", L"*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.dds;*.hdr" },
            { L"All Files", L"*.*" },
        },
        initialDir_,
        ownerWindow_);
}

std::string WindowsDialogHelper::OpenJsonDialog(
    const wchar_t* title_,
    const std::string& initialDir_,
    HWND ownerWindow_)
{
    return OpenFileDialog(title_,
        {
            { L"JSON Files", L"*.json" },
            { L"All Files", L"*.*" },
        },
        initialDir_,
        ownerWindow_);
}

std::string WindowsDialogHelper::SaveJsonDialog(
    const wchar_t* title_,
    const std::string& initialPath_,
    HWND ownerWindow_)
{
    return SaveFileDialog(title_,
        {
            { L"JSON Files", L"*.json" },
            { L"All Files", L"*.*" },
        },
        initialPath_,
        L"json",
        ownerWindow_);
}

std::string WindowsDialogHelper::OpenGltfDialog(
    const wchar_t* title_,
    const std::string& initialDir_,
    HWND ownerWindow_)
{
    return OpenFileDialog(title_,
        {
            { L"glTF Files", L"*.gltf;*.glb" },
            { L"All Files", L"*.*" },
        },
        initialDir_,
        ownerWindow_);
}

std::string WindowsDialogHelper::OpenAssetDialog(
    const wchar_t* title_,
    const std::string& initialDir_,
    HWND ownerWindow_)
{
    return OpenFileDialog(title_,
        {
            { L"All Asset Files", L"*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.dds;*.hdr;*.json;*.gltf;*.glb" },
            { L"Texture Files", L"*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.dds;*.hdr" },
            { L"JSON Files", L"*.json" },
            { L"glTF Files", L"*.gltf;*.glb" },
            { L"All Files", L"*.*" },
        },
        initialDir_,
        ownerWindow_);
}

std::filesystem::path WindowsDialogHelper::Utf8ToPath(const std::string& path_)
{
    return std::filesystem::path(Utf8ToWide(path_));
}

std::string WindowsDialogHelper::PathToUtf8(const std::filesystem::path& path_)
{
    return WideToUtf8(path_.generic_wstring());
}

#include "PrecompiledHeader.h"
#include "GamePackageExporter.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Input/KeyConfig.h"
#include "Engine/Debug/TracyProfiler.h"
#include "Engine/Platform/WindowsDialogHelper.h"

#include <Windows.h>
#include <utility>
#include <array>
#include <cstdio>
#include <fstream>
#include <map>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace
{
    // Scene名とObject名をファイル名として検証し、書き出し先の外へ出るパスを拒否する。
    bool IsSafeFileName(const std::string& name_)
    {
        if (name_.empty() || name_ == "." || name_ == ".." ||
            name_.find_first_of("<>:\"/\\|?*") != std::string::npos ||
            name_.back() == '.' || name_.back() == ' ')
        {
            return false;
        }
        for (const unsigned char character_ : name_)
        {
            if (character_ < 32)
            {
                return false;
            }
        }
        return true;
    }

    // 配布データはUTF-8 BOMなし・CRLFで保存し、書き込み失敗も呼び出し元へ通知する。
    void WriteJson(const std::filesystem::path& path_, const nlohmann::json& document_)
    {
        std::filesystem::create_directories(path_.parent_path());
        std::ofstream file_(path_, std::ios::binary);
        file_.exceptions(std::ios::badbit | std::ios::failbit);
        const auto text_ = document_.dump(4);
        for (const char character_ : text_)
        {
            if (character_ == '\n')
            {
                file_.put('\r');
            }
            file_.put(character_);
        }
        file_ << "\r\n";
        file_.flush();
        file_.close();
    }

    // 元のData構成を保ち、リンクから外部ファイルを取り込まないよう検査する。
    void CopyData(const std::filesystem::path& source_, const std::filesystem::path& destination_)
    {
        BS_PROFILE_SCOPE("GamePackageExporter::CopyData");
        for (const auto& entry_ : std::filesystem::recursive_directory_iterator(source_))
        {
            if (entry_.is_symlink())
            {
                throw std::runtime_error("Data contains a symbolic link. Export stopped.");
            }
            const auto target_ = destination_ / entry_.path().lexically_relative(source_);
            if (entry_.is_directory())
            {
                std::filesystem::create_directories(target_);
            }
            else if (entry_.is_regular_file())
            {
                std::filesystem::create_directories(target_.parent_path());
                std::filesystem::copy_file(entry_.path(), target_);
            }
        }
    }
}

// ICOの形式と画像領域を検証してから、Game専用リソースだけを更新する。
bool GamePackageExporter::SaveGameIcon(const std::filesystem::path& icon_, std::string& error_)
{
    BS_PROFILE_SCOPE("GamePackageExporter::SaveGameIcon");
    try
    {
        const auto source_ = std::filesystem::canonical(icon_);
        if (!std::filesystem::is_regular_file(source_))
        {
            throw std::runtime_error("Select an ICO file.");
        }
        const auto fileSize_ = std::filesystem::file_size(source_);
        std::ifstream file_(source_, std::ios::binary);
        file_.exceptions(std::ios::badbit | std::ios::failbit);
        std::array<unsigned char, 6> header_{};
        file_.read(reinterpret_cast<char*>(header_.data()), header_.size());
        const unsigned int imageCount_ = header_[4] | (static_cast<unsigned int>(header_[5]) << 8);
        const auto directorySize_ = 6ull + 16ull * imageCount_;
        if (header_[0] != 0 || header_[1] != 0 || header_[2] != 1 || header_[3] != 0 ||
            imageCount_ == 0 || directorySize_ > fileSize_)
        {
            throw std::runtime_error("The selected file is not a valid ICO file.");
        }

        // 複数解像度の各画像がファイル内に収まることを確認し、不正なデータ範囲を拒否する。
        for (unsigned int index_ = 0; index_ < imageCount_; ++index_)
        {
            std::array<unsigned char, 16> entry_{};
            file_.read(reinterpret_cast<char*>(entry_.data()), entry_.size());
            const auto readUInt32_ = [&entry_](std::size_t offset_) -> unsigned long long
            {
                return static_cast<unsigned long long>(entry_[offset_]) |
                    (static_cast<unsigned long long>(entry_[offset_ + 1]) << 8) |
                    (static_cast<unsigned long long>(entry_[offset_ + 2]) << 16) |
                    (static_cast<unsigned long long>(entry_[offset_ + 3]) << 24);
            };
            const auto imageSize_ = readUInt32_(8);
            const auto imageOffset_ = readUInt32_(12);
            if (imageSize_ == 0 || imageOffset_ < directorySize_ || imageOffset_ + imageSize_ > fileSize_)
            {
                throw std::runtime_error("The ICO file contains an invalid image range.");
            }
        }
        file_.close();
        const auto preview_ = static_cast<HICON>(LoadImageW(nullptr, source_.c_str(), IMAGE_ICON,
            0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE));
        if (!preview_)
        {
            throw std::runtime_error("Windows could not load the selected ICO file.");
        }
        DestroyIcon(preview_);

        // Icons.rcのGame分岐が参照する固定パスへ保存し、Engine用ICOには触れない。
        const auto destination_ = std::filesystem::current_path() / "Data" / "Icons" / "ANATHEMA" / "ANATHEMA.ico";
        if (!std::filesystem::is_regular_file("BIGShine.vcxproj"))
        {
            throw std::runtime_error("Save the icon from the project directory.");
        }
        std::filesystem::create_directories(destination_.parent_path());
        if (!std::filesystem::exists(destination_) || !std::filesystem::equivalent(source_, destination_))
        {
            std::filesystem::copy_file(source_, destination_, std::filesystem::copy_options::overwrite_existing);
        }
        error_.clear();
        return true;
    }
    catch (const std::exception& exception_)
    {
        error_ = exception_.what();
        return false;
    }
}

// 書き出し処理は新規フォルダー内だけで行い、既存の配布物を上書きしない。
bool GamePackageExporter::Export(GameObjectManager& manager_, const std::filesystem::path& executable_,
    std::string& outputDirectory_, std::string& error_)
{
    BS_PROFILE_SCOPE("GamePackageExporter::Export");
    outputDirectory_.clear();
    try
    {
        if (manager_.GetSceneRuntimeState() != SCENE_RUNTIME_STATE::RUNNING)
        {
            throw std::runtime_error("Wait for the scene to finish loading before exporting.");
        }
        const auto sourceRoot_ = std::filesystem::current_path();
        const auto executablePath_ = std::filesystem::canonical(executable_);
        if (!std::filesystem::is_regular_file(executablePath_) || executablePath_.extension() != ".exe")
        {
            throw std::runtime_error("Select a built Release or Shipping executable.");
        }
        const auto dataPath_ = sourceRoot_ / "Data";
        const auto readmePath_ = sourceRoot_ / "ReadMe.txt";
        if (!std::filesystem::is_directory(dataPath_) || !std::filesystem::is_regular_file(readmePath_))
        {
            throw std::runtime_error("Data or ReadMe.txt was not found in the project directory.");
        }

        auto snapshot_ = manager_.CaptureStandaloneScene();
        const auto sceneName_ = snapshot_.at("SceneName").get<std::string>();
        if (!IsSafeFileName(sceneName_) || !snapshot_.at("Objects").is_object() ||
            !snapshot_.at("ObjectList").is_array())
        {
            throw std::runtime_error("The startup scene snapshot is invalid.");
        }
        for (const auto& nameJson_ : snapshot_.at("ObjectList"))
        {
            const auto name_ = nameJson_.get<std::string>();
            if (!IsSafeFileName(name_) || !snapshot_.at("Objects").contains(name_))
            {
                throw std::runtime_error("The scene contains an invalid object name or missing object.");
            }
        }

        // EXEと同じ場所のDLLを優先し、PhysXなどプロジェクト直下の依存DLLを補完する。
        std::map<std::filesystem::path, std::filesystem::path> libraries_;
        for (const auto& entry_ : std::filesystem::directory_iterator(executablePath_.parent_path()))
        {
            if (entry_.is_regular_file() && entry_.path().extension() == ".dll")
            {
                libraries_[entry_.path().filename()] = entry_.path();
            }
        }
        constexpr std::array<const char*, 6> REQUIRED_LIBRARIES{
            "SDL3.dll", "fmod.dll", "PhysX_64.dll", "PhysXCommon_64.dll",
            "PhysXCooking_64.dll", "PhysXFoundation_64.dll"
        };
        for (const auto name_ : REQUIRED_LIBRARIES)
        {
            const auto fileName_ = std::filesystem::path(name_);
            if (libraries_.count(fileName_) == 0 && std::filesystem::is_regular_file(sourceRoot_ / fileName_))
            {
                libraries_[fileName_] = sourceRoot_ / fileName_;
            }
            if (libraries_.count(fileName_) == 0)
            {
                throw std::runtime_error(std::string("Required runtime DLL not found beside EXE or project: ") + name_);
            }
        }

        SYSTEMTIME time_{};
        GetLocalTime(&time_);
        char folderName_[96]{};
        sprintf_s(folderName_, "ReviewGame_%04u%02u%02u_%02u%02u%02u_%03u",
            time_.wYear, time_.wMonth, time_.wDay, time_.wHour, time_.wMinute, time_.wSecond, time_.wMilliseconds);
        const auto exportRoot_ = sourceRoot_ / "Exports";
        std::filesystem::create_directories(exportRoot_);
        const auto output_ = exportRoot_ / folderName_;
        if (!std::filesystem::create_directory(output_))
        {
            throw std::runtime_error("The export directory already exists. Please retry.");
        }
        outputDirectory_ = WindowsDialogHelper::PathToUtf8(output_);

        CopyData(dataPath_, output_ / "Data");
        std::filesystem::copy_file(executablePath_, output_ / "Game.exe");
        for (const auto& [name_, library_] : libraries_)
        {
            std::filesystem::copy_file(library_, output_ / name_);
        }
        std::filesystem::copy_file(readmePath_, output_ / "ReadMe.txt");

        // Play開始前の配置と描画設定を配布側のSceneへ反映し、編集カメラは含めない。
        const auto scenePath_ = output_ / "Data" / "Scenes" / WindowsDialogHelper::Utf8ToPath(sceneName_);
        for (const auto& nameJson_ : snapshot_.at("ObjectList"))
        {
            const auto name_ = nameJson_.get<std::string>();
            WriteJson(scenePath_ / "Objects" / WindowsDialogHelper::Utf8ToPath(name_ + ".json"),
                snapshot_.at("Objects").at(name_));
        }
        snapshot_.erase("Objects");
        snapshot_.erase("EditorSelection");
        snapshot_.erase("EditorCamera");
        WriteJson(scenePath_ / "SceneList.json", snapshot_);

        // 実行中に補完・編集した入力も配布物へ保存し、元のKeyConfigファイルは変更しない。
        nlohmann::json input_{{"actions", nlohmann::json::object()}, {"axes", nlohmann::json::object()}};
        for (const auto& [name_, combos_] : KeyConfig::Instance().GetActionMap())
        {
            input_["actions"][name_] = nlohmann::json::array();
            for (const auto& combo_ : combos_)
            {
                nlohmann::json units_ = nlohmann::json::array();
                for (const auto& unit_ : combo_)
                {
                    units_.push_back({{"dev", static_cast<int>(unit_.device)}, {"code", unit_.code}});
                }
                input_["actions"][name_].push_back(std::move(units_));
            }
        }
        for (const auto& [name_, binds_] : KeyConfig::Instance().GetAxisMap())
        {
            input_["axes"][name_] = nlohmann::json::array();
            for (const auto& unit_ : binds_)
            {
                input_["axes"][name_].push_back({{"sourceType", static_cast<int>(unit_.sourceType)},
                    {"dev", static_cast<int>(unit_.device)}, {"code", unit_.code},
                    {"scale", unit_.scale}, {"deadZone", unit_.deadZone}});
            }
        }
        WriteJson(output_ / "Data" / "Input" / "KeyConfig.json", input_);
        // 全コピーが成功した最後に起動設定を保存し、未完了フォルダーと区別する。
        WriteJson(output_ / "GamePackage.json",
            {{"FormatVersion", 1}, {"GameOnly", true}, {"StartupScene", sceneName_}});
        error_.clear();
        return true;
    }
    catch (const std::exception& exception_)
    {
        error_ = exception_.what();
        if (!outputDirectory_.empty())
        {
            error_ += "\nIncomplete export: " + outputDirectory_;
        }
        return false;
    }
}

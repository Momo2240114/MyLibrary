#include "PrecompiledHeader.h"
#include "GameRuntimeSettings.h"
#include "Engine/Core/BuildConfig.h"
#include "Engine/Debug/TracyProfiler.h"
#include "Engine/GamePlay/Input/KeyConfig.h"

#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <nlohmann/json.hpp>

bool GameRuntimeSettings::gameOnly = !BS_ENABLE_EDITOR;
std::string GameRuntimeSettings::startupScene = "MainScene";
GameRuntimeSettings::UserPreferences GameRuntimeSettings::userPreferences;

namespace
{
    std::filesystem::path GetUserPreferencesPath()
    {
        const DWORD required_ = GetEnvironmentVariableW(L"LOCALAPPDATA", nullptr, 0);
        if (required_ == 0)
        {
            return {};
        }
        std::wstring directory_(required_, L'\0');
        const DWORD length_ = GetEnvironmentVariableW(L"LOCALAPPDATA", directory_.data(), required_);
        if (length_ == 0 || length_ >= required_)
        {
            return {};
        }
        directory_.resize(length_);
        return std::filesystem::path(directory_) / L"ANATHEMA" / L"UserSettings.json";
    }
}

const GameRuntimeSettings::UserPreferences& GameRuntimeSettings::GetUserPreferences()
{
    return userPreferences;
}

void GameRuntimeSettings::SetUserPreferences(const UserPreferences& preferences_)
{
    const auto clamp_ = [](float value_, float min_, float max_)
    {
        return std::isfinite(value_) ? std::clamp(value_, min_, max_) : 1.0f;
    };
    userPreferences = {
        clamp_(preferences_.brightness, 0.5f, 1.5f),
        clamp_(preferences_.masterVolume, 0.0f, 1.0f),
        clamp_(preferences_.bgmVolume, 0.0f, 1.0f),
        clamp_(preferences_.seVolume, 0.0f, 1.0f),
        clamp_(preferences_.mouseSensitivity, 0.1f, 3.0f),
        clamp_(preferences_.controllerSensitivity, 0.1f, 3.0f),
        clamp_(preferences_.vibration, 0.0f, 1.0f)
    };
}

void GameRuntimeSettings::LoadUserPreferences()
{
    userPreferences = {};
    const auto path_ = GetUserPreferencesPath();
    if (path_.empty())
    {
        return;
    }
    std::ifstream file_(path_, std::ios::binary);
    if (!file_)
    {
        return;
    }
    const auto json_ = nlohmann::json::parse(file_, nullptr, false);
    if (!json_.is_object())
    {
        return;
    }
    const auto read_ = [&](const char* key_)
    {
        const auto value_ = json_.find(key_);
        return value_ != json_.end() && value_->is_number() ? value_->get<float>() : 1.0f;
    };
    SetUserPreferences({read_("Brightness"), read_("MasterVolume"), read_("BgmVolume"), read_("SeVolume"),
        read_("MouseSensitivity"), read_("ControllerSensitivity"), read_("Vibration")});
}

bool GameRuntimeSettings::SaveUserPreferences()
{
    const auto path_ = GetUserPreferencesPath();
    if (path_.empty())
    {
        return false;
    }
    std::error_code error_;
    std::filesystem::create_directories(path_.parent_path(), error_);
    if (error_)
    {
        return false;
    }
    auto temporary_ = path_;
    temporary_ += L".tmp";
    const nlohmann::json json_ = {{"Brightness", userPreferences.brightness}, {"MasterVolume", userPreferences.masterVolume},
        {"BgmVolume", userPreferences.bgmVolume}, {"SeVolume", userPreferences.seVolume},
        {"MouseSensitivity", userPreferences.mouseSensitivity}, {"ControllerSensitivity", userPreferences.controllerSensitivity},
        {"Vibration", userPreferences.vibration}};
    std::ofstream file_(temporary_);
    if (!file_)
    {
        return false;
    }
    file_ << json_.dump(4) << '\n';
    file_.close();
    if (file_ && MoveFileExW(temporary_.c_str(), path_.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        return true;
    }
    std::filesystem::remove(temporary_, error_);
    return false;
}

// 配布版と開発中のShipping起動で、それぞれのDataの配置先を解決する。
bool GameRuntimeSettings::Initialize(std::string& error_)
{
    BS_PROFILE_SCOPE("GameRuntimeSettings::Initialize");
    try
    {
        wchar_t executablePath_[32768]{};
        const DWORD length_ = GetModuleFileNameW(nullptr, executablePath_, 32768);
        if (length_ == 0 || length_ >= 32768)
        {
            throw std::runtime_error("Executable path could not be resolved.");
        }
        const auto directory_ = std::filesystem::path(executablePath_).parent_path();
        const auto settingsPath_ = directory_ / "GamePackage.json";
        const bool hasPackageSettings_ = std::filesystem::exists(settingsPath_);
        gameOnly = !BS_ENABLE_EDITOR;
        startupScene = "MainScene";
        if (hasPackageSettings_)
        {
            std::ifstream file_(settingsPath_, std::ios::binary);
            nlohmann::json settings_;
            file_ >> settings_;
            if (settings_.at("FormatVersion").get<int>() != 1 ||
                !settings_.at("GameOnly").get<bool>())
            {
                throw std::runtime_error("Unsupported game package settings.");
            }
            startupScene = settings_.at("StartupScene").get<std::string>();
            if (startupScene.empty() || startupScene == "." || startupScene == ".." ||
                startupScene.find_first_of("/\\:") != std::string::npos)
            {
                throw std::runtime_error("Invalid startup scene name.");
            }
#if defined(BS_BUILD_RELEASE) && !defined(BS_APP_GAME) && !defined(BS_APP_ENGINE)
            // 通常のReleaseだけは、書き出した配布版でもタイトルからF1でEditorへ入れるようにする。
            gameOnly = false;
#else
            gameOnly = true;
#endif
        }
        // Engineの出力先から直接起動した場合も、親階層の開発用Dataを解決する。
#if defined(BS_APP_ENGINE)
        constexpr bool RESOLVE_ENGINE_DATA = true;
#else
        constexpr bool RESOLVE_ENGINE_DATA = false;
#endif
        if (hasPackageSettings_ || gameOnly || RESOLVE_ENGINE_DATA)
        {
            auto dataDirectory_ = directory_;
            // 配布設定がある場合は、別のプロジェクトのDataで欠損を補わない。
            // 設定のないビルド出力からの起動に限り、プロジェクトのDataを探す。
            if (!hasPackageSettings_ && !std::filesystem::is_directory(directory_ / "Data"))
            {
                const auto workingDirectory_ = std::filesystem::current_path();
                if (std::filesystem::is_regular_file(workingDirectory_ / "BIGShine.vcxproj") &&
                    std::filesystem::is_directory(workingDirectory_ / "Data"))
                {
                    dataDirectory_ = workingDirectory_;
                }
                else
                {
                    auto candidateDirectory_ = directory_.parent_path();
                    while (!candidateDirectory_.empty())
                    {
                        if (std::filesystem::is_regular_file(candidateDirectory_ / "BIGShine.vcxproj") &&
                            std::filesystem::is_directory(candidateDirectory_ / "Data"))
                        {
                            dataDirectory_ = candidateDirectory_;
                            break;
                        }
                        const auto parentDirectory_ = candidateDirectory_.parent_path();
                        if (parentDirectory_ == candidateDirectory_)
                        {
                            break;
                        }
                        candidateDirectory_ = parentDirectory_;
                    }
                }
            }
            std::filesystem::current_path(dataDirectory_);
        }
        // タイトルからEditorを開かず本編へ進む起動でも、Scene生成前に入力設定を読み込む。
        const auto keyConfigPath_ = std::filesystem::current_path() / "Data/Input/KeyConfig.json";
        const auto keyConfigPathUtf8_ = keyConfigPath_.u8string();
        const std::string keyConfigPathText_(keyConfigPathUtf8_.begin(), keyConfigPathUtf8_.end());
        if (!std::filesystem::is_regular_file(keyConfigPath_))
        {
            throw std::runtime_error("Input configuration is missing. Check Data/Input/KeyConfig.json.\nPath: " + keyConfigPathText_);
        }
        auto& keys_ = KeyConfig::Instance();
        keys_.SetConfigFile("Data/Input/KeyConfig.json");
        if (!keys_.Load())
        {
            throw std::runtime_error("Input configuration could not be read. Check file access and JSON syntax.\nPath: " + keyConfigPathText_);
        }
        LoadUserPreferences();
        error_.clear();
        return true;
    }
    catch (const std::exception& exception_)
    {
        error_ = exception_.what();
        return false;
    }
}

bool GameRuntimeSettings::IsGameOnly()
{
    return gameOnly;
}

const std::string& GameRuntimeSettings::GetStartupScene()
{
    return startupScene;
}

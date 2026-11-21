#include "PrecompiledHeader.h"
#include "GameRuntimeSettings.h"
#include "Engine/Core/BuildConfig.h"
#include "Engine/Debug/TracyProfiler.h"
#include "Engine/GamePlay/Input/KeyConfig.h"

#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <nlohmann/json.hpp>

bool GameRuntimeSettings::gameOnly = !BS_ENABLE_EDITOR;
std::string GameRuntimeSettings::startupScene = "MainScene";

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

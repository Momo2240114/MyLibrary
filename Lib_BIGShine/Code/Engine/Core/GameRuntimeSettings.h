#pragma once

#include <string>

// 配布フォルダーの起動設定を保持し、エディターとゲーム専用起動を切り替える。
class GameRuntimeSettings
{
public:
    // 配布設定とDataの配置先を解決し、全起動モード共通で入力設定を読み込む。
    static bool Initialize(std::string& error_);
    static bool IsGameOnly();
    static const std::string& GetStartupScene();

    struct UserPreferences
    {
        float brightness = 1.0f;
        float masterVolume = 1.0f;
        float bgmVolume = 1.0f;
        float seVolume = 1.0f;
        float mouseSensitivity = 1.0f;
        float controllerSensitivity = 1.0f;
        float vibration = 1.0f;
    };

    static const UserPreferences& GetUserPreferences();
    static void SetUserPreferences(const UserPreferences& preferences_);
    static bool SaveUserPreferences();

private:
    static void LoadUserPreferences();
    static UserPreferences userPreferences;
    static bool gameOnly;
    static std::string startupScene;
};

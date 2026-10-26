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

private:
    static bool gameOnly;
    static std::string startupScene;
};

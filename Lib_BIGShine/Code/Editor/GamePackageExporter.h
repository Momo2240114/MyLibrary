#pragma once

#include <filesystem>
#include <string>

class GameObjectManager;

// Game用アイコンの保存と、ビルド済みEXE・開始Sceneの配布書き出しを担当する。
class GamePackageExporter
{
public:
    // 選択したICOを検証し、次回Gameビルドで埋め込む既存の保存先へ複製する。
    static bool SaveGameIcon(const std::filesystem::path& icon_, std::string& error_);

    // 元データを変更せず、依存DLL・入力設定・ReadMeを含むゲーム一式を書き出す。
    static bool Export(GameObjectManager& manager_, const std::filesystem::path& executable_,
        std::string& outputDirectory_, std::string& error_);
};

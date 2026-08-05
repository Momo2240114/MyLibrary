#pragma once

#include "Engine/Core/BuildConfig.h"

#include <cstddef>
#include <cstdint>
#include <chrono>
#include <mutex>
#include <string>
#include <vector>

enum class LOG_CATEGORY
{
    // Engine全体の基本ログ。
    CORE,
    COMPONENT,
    EDITOR,
    PHYSICS,
    RENDER,
    AUDIO,
    GAME,
    AI,
    COUNT
};

enum class LOG_LEVEL
{
    // 通常情報。
    INFO,
    SUCCESS,
    WARNING,
    ERROR_LOG
};

struct LogMessage
{
    // 追加順に増える通し番号。
    uint64_t sequence = 0u;
    LOG_CATEGORY category = LOG_CATEGORY::CORE;
    LOG_LEVEL level = LOG_LEVEL::INFO;
    std::string file;
    int line = 0;
    std::string text;
    std::string time;

    // DebugLog生成からの経過秒。時間範囲検索に使う。
    double elapsedSeconds = 0.0;
};

class DebugLog
{
public:
    static DebugLog& Instance();

    // ファイル名と行番号つきでログを追加する。
    void AddLog(LOG_CATEGORY category_, LOG_LEVEL level_, const char* file_, int line_, const std::string& text_);
    void Clear();

    std::vector<LogMessage> GetMessages() const;

    // UI仮想リスト用に、指定範囲だけ取り出す。
    std::vector<LogMessage> GetMessagesRange(size_t startIndex_, size_t count_) const;

    // 直近ログ表示用に、指定時刻以降のログだけ取り出す。
    std::vector<LogMessage> GetMessagesSince(double minElapsedSeconds_, size_t maxCount_) const;
    size_t GetMessageCount() const;
    double GetElapsedSeconds() const;

    bool SaveText(const std::string& path_) const;

    // DebugWindowや外部解析用にJSON形式で保存する。
    bool SaveJson(const std::string& path_) const;

    static const char* GetCategoryName(LOG_CATEGORY category_);
    static const char* GetLevelName(LOG_LEVEL level_);

private:
    DebugLog() = default;

    // ログ表示用の時刻文字列を作る。
    static std::string MakeTimeString();

    // JSON保存時に壊れないよう特殊文字をエスケープする。
    static std::string EscapeJsonString(const std::string& text_);

    // 複数スレッドからログが飛ぶため、messages操作を保護する。
    mutable std::mutex mutex;
    std::vector<LogMessage> messages;
    std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();
    uint64_t nextSequence = 0u;
};

#define BS_LOG(category_, level_, text_) \
    DebugLog::Instance().AddLog((category_), (level_), __FILE__, __LINE__, (text_))

#define BS_LOG_NOOP(category_, text_) \
    do \
    { \
    } while (false)

#if BS_ENABLE_DEBUG_LOG
#define BS_LOG_INFO(category_, text_) \
    BS_LOG((category_), LOG_LEVEL::INFO, (text_))
#else
#define BS_LOG_INFO(category_, text_) BS_LOG_NOOP((category_), (text_))
#endif

#if BS_ENABLE_DEBUG_LOG
#define BS_LOG_SUCCESS(category_, text_) \
    BS_LOG((category_), LOG_LEVEL::SUCCESS, (text_))
#else
#define BS_LOG_SUCCESS(category_, text_) BS_LOG_NOOP((category_), (text_))
#endif

#if BS_ENABLE_WARNING_LOG
#define BS_LOG_WARNING(category_, text_) \
    BS_LOG((category_), LOG_LEVEL::WARNING, (text_))
#else
#define BS_LOG_WARNING(category_, text_) BS_LOG_NOOP((category_), (text_))
#endif

#if BS_ENABLE_ERROR_LOG
#define BS_LOG_ERROR(category_, text_) \
    BS_LOG((category_), LOG_LEVEL::ERROR_LOG, (text_))
#else
#define BS_LOG_ERROR(category_, text_) BS_LOG_NOOP((category_), (text_))
#endif

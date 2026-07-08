#pragma once

#include <cstddef>
#include <mutex>
#include <string>
#include <vector>

enum class LOG_CATEGORY
{
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
    INFO,
    SUCCESS,
    WARNING,
    ERROR_LOG
};

struct LogMessage
{
    LOG_CATEGORY category = LOG_CATEGORY::CORE;
    LOG_LEVEL level = LOG_LEVEL::INFO;
    std::string file;
    int line = 0;
    std::string text;
    std::string time;
};

class DebugLog
{
public:
    static DebugLog& Instance();

    void AddLog(LOG_CATEGORY category_, LOG_LEVEL level_, const char* file_, int line_, const std::string& text_);
    void Clear();

    std::vector<LogMessage> GetMessages() const;
    size_t GetMessageCount() const;

    bool SaveText(const std::string& path_) const;
    bool SaveJson(const std::string& path_) const;

    static const char* GetCategoryName(LOG_CATEGORY category_);
    static const char* GetLevelName(LOG_LEVEL level_);

private:
    DebugLog() = default;

    static std::string MakeTimeString();
    static std::string EscapeJsonString(const std::string& text_);

    mutable std::mutex mutex;
    std::vector<LogMessage> messages;
};

#define BS_LOG(category_, level_, text_) \
    DebugLog::Instance().AddLog((category_), (level_), __FILE__, __LINE__, (text_))

#define BS_LOG_INFO(category_, text_) \
    BS_LOG((category_), LOG_LEVEL::INFO, (text_))

#define BS_LOG_SUCCESS(category_, text_) \
    BS_LOG((category_), LOG_LEVEL::SUCCESS, (text_))

#define BS_LOG_WARNING(category_, text_) \
    BS_LOG((category_), LOG_LEVEL::WARNING, (text_))

#define BS_LOG_ERROR(category_, text_) \
    BS_LOG((category_), LOG_LEVEL::ERROR_LOG, (text_))

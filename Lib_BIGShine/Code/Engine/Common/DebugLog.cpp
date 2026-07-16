#include "PrecompiledHeader.h"

#include "DebugLog.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <utility>

DebugLog& DebugLog::Instance()
{
    static DebugLog instance_;
    return instance_;
}

void DebugLog::AddLog(LOG_CATEGORY category_, LOG_LEVEL level_, const char* file_, int line_, const std::string& text_)
{
    const std::chrono::steady_clock::time_point now_ = std::chrono::steady_clock::now();
    LogMessage message_;
    message_.category = category_;
    message_.level = level_;
    message_.file = file_ ? file_ : "";
    message_.line = line_;
    message_.text = text_;
    message_.time = MakeTimeString();

    std::lock_guard<std::mutex> lock_(mutex);
    message_.sequence = nextSequence++;
    message_.elapsedSeconds = std::chrono::duration<double>(now_ - startTime).count();
    messages.emplace_back(std::move(message_));
}

void DebugLog::Clear()
{
    std::lock_guard<std::mutex> lock_(mutex);
    messages.clear();
    nextSequence = 0u;
    startTime = std::chrono::steady_clock::now();
}

std::vector<LogMessage> DebugLog::GetMessages() const
{
    std::lock_guard<std::mutex> lock_(mutex);
    return messages;
}

std::vector<LogMessage> DebugLog::GetMessagesRange(size_t startIndex_, size_t count_) const
{
    std::lock_guard<std::mutex> lock_(mutex);
    if (startIndex_ >= messages.size() || count_ == 0u) return {};

    const size_t endIndex_ = (std::min)(messages.size(), startIndex_ + count_);
    return std::vector<LogMessage>(messages.begin() + startIndex_, messages.begin() + endIndex_);
}

std::vector<LogMessage> DebugLog::GetMessagesSince(double minElapsedSeconds_, size_t maxCount_) const
{
    std::lock_guard<std::mutex> lock_(mutex);
    if (messages.empty() || maxCount_ == 0u) return {};

    size_t firstIndex_ = messages.size();
    size_t collectedCount_ = 0u;
    for (size_t index_ = messages.size(); index_ > 0u; --index_)
    {
        const LogMessage& message_ = messages[index_ - 1u];
        if (message_.elapsedSeconds < minElapsedSeconds_) break;
        firstIndex_ = index_ - 1u;
        ++collectedCount_;
        if (collectedCount_ >= maxCount_) break;
    }

    if (firstIndex_ >= messages.size()) return {};

    return std::vector<LogMessage>(messages.begin() + firstIndex_, messages.end());
}

size_t DebugLog::GetMessageCount() const
{
    std::lock_guard<std::mutex> lock_(mutex);
    return messages.size();
}

double DebugLog::GetElapsedSeconds() const
{
    std::lock_guard<std::mutex> lock_(mutex);
    const std::chrono::steady_clock::time_point now_ = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(now_ - startTime).count();
}

bool DebugLog::SaveText(const std::string& path_) const
{
    const std::filesystem::path outputPath_(path_);
    if (outputPath_.has_parent_path())
    {
        std::filesystem::create_directories(outputPath_.parent_path());
    }

    std::ofstream file_(outputPath_, std::ios::out | std::ios::trunc);
    if (!file_.is_open()) return false;

    const std::vector<LogMessage> copy_ = GetMessages();
    for (const LogMessage& message_ : copy_)
    {
        file_ << "[" << message_.time << "]"
            << "[" << GetCategoryName(message_.category) << "]"
            << "[" << GetLevelName(message_.level) << "] "
            << message_.text
            << " (" << message_.file << ":" << message_.line << ")"
            << "\n";
    }

    return true;
}

bool DebugLog::SaveJson(const std::string& path_) const
{
    const std::filesystem::path outputPath_(path_);
    if (outputPath_.has_parent_path())
    {
        std::filesystem::create_directories(outputPath_.parent_path());
    }

    std::ofstream file_(outputPath_, std::ios::out | std::ios::trunc);
    if (!file_.is_open()) return false;

    const std::vector<LogMessage> copy_ = GetMessages();
    file_ << "{\n";
    file_ << "  \"Logs\": [\n";
    for (size_t index_ = 0; index_ < copy_.size(); ++index_)
    {
        const LogMessage& message_ = copy_[index_];
        file_ << "    {\n";
        file_ << "      \"Time\": \"" << EscapeJsonString(message_.time) << "\",\n";
        file_ << "      \"Sequence\": " << message_.sequence << ",\n";
        file_ << "      \"ElapsedSeconds\": " << message_.elapsedSeconds << ",\n";
        file_ << "      \"Category\": \"" << GetCategoryName(message_.category) << "\",\n";
        file_ << "      \"Level\": \"" << GetLevelName(message_.level) << "\",\n";
        file_ << "      \"File\": \"" << EscapeJsonString(message_.file) << "\",\n";
        file_ << "      \"Line\": " << message_.line << ",\n";
        file_ << "      \"Text\": \"" << EscapeJsonString(message_.text) << "\"\n";
        file_ << "    }";
        if (index_ + 1 < copy_.size()) file_ << ",";
        file_ << "\n";
    }
    file_ << "  ]\n";
    file_ << "}\n";

    return true;
}

const char* DebugLog::GetCategoryName(LOG_CATEGORY category_)
{
    switch (category_)
    {
    case LOG_CATEGORY::CORE: return "Core";
    case LOG_CATEGORY::COMPONENT: return "Component";
    case LOG_CATEGORY::EDITOR: return "Editor";
    case LOG_CATEGORY::PHYSICS: return "Physics";
    case LOG_CATEGORY::RENDER: return "Render";
    case LOG_CATEGORY::AUDIO: return "Audio";
    case LOG_CATEGORY::GAME: return "Game";
    case LOG_CATEGORY::AI: return "AI";
    default: return "Unknown";
    }
}

const char* DebugLog::GetLevelName(LOG_LEVEL level_)
{
    switch (level_)
    {
    case LOG_LEVEL::INFO: return "Info";
    case LOG_LEVEL::SUCCESS: return "Success";
    case LOG_LEVEL::WARNING: return "Warning";
    case LOG_LEVEL::ERROR_LOG: return "Error";
    default: return "Unknown";
    }
}

std::string DebugLog::MakeTimeString()
{
    const auto now_ = std::chrono::system_clock::now();
    const std::time_t time_ = std::chrono::system_clock::to_time_t(now_);
    std::tm localTime_{};

#if defined(_WIN32)
    localtime_s(&localTime_, &time_);
#else
    localtime_r(&time_, &localTime_);
#endif

    std::ostringstream stream_;
    stream_ << std::put_time(&localTime_, "%Y-%m-%d %H:%M:%S");
    return stream_.str();
}

std::string DebugLog::EscapeJsonString(const std::string& text_)
{
    std::ostringstream stream_;
    for (const char char_ : text_)
    {
        switch (char_)
        {
        case '\\': stream_ << "\\\\"; break;
        case '"': stream_ << "\\\""; break;
        case '\n': stream_ << "\\n"; break;
        case '\r': stream_ << "\\r"; break;
        case '\t': stream_ << "\\t"; break;
        default:
            stream_ << char_;
            break;
        }
    }
    return stream_.str();
}

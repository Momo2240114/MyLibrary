#include "PrecompiledHeader.h"
#include "SceneLoadRequest.h"
#include "Engine/Debug/TracyProfiler.h"
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <unordered_set>

namespace
{
    // Scene名・Object名のパス区切りを拒否し、Data/Scenesの外へ読み出さない。
    void ValidateName(const std::string& name_)
    {
        if (name_.empty() || name_ == "." || name_ == ".." ||
            name_.find_first_of("/\\:") != std::string::npos)
        {
            throw std::runtime_error("Invalid scene or object name: " + name_);
        }
    }

    nlohmann::json ReadJson(const std::filesystem::path& path_)
    {
        std::ifstream file_(path_, std::ios::binary);
        if (!file_)
        {
            const auto pathText_ = path_.u8string();
            throw std::runtime_error("Scene file not found: " + std::string(pathText_.begin(), pathText_.end()));
        }
        nlohmann::json result_;
        file_ >> result_;
        return result_;
    }
}

SceneLoadRequest::SceneLoadRequest(const std::string& sceneName_)
    : shared(std::make_shared<SharedState>()),
      worker([state_ = shared, sceneName_](std::stop_token stop_)
{
    ReadScene(stop_, state_, sceneName_);
})
{
}

SceneLoadRequest::~SceneLoadRequest()
{
    // workerはsharedより先に破棄され、Device・Sceneの寿命に依存せず終了する。
    worker.request_stop();
}

void SceneLoadRequest::ReadScene(std::stop_token stop_, std::shared_ptr<SharedState> state_, std::string sceneName_)
{
    BS_PROFILE_SCOPE("SceneLoadRequest::ReadScene");
    try
    {
        ValidateName(sceneName_);
        const auto base_ = std::filesystem::path("Data/Scenes") / std::filesystem::path(std::u8string(sceneName_.begin(), sceneName_.end()));
        auto document_ = ReadJson(base_ / "SceneList.json");
        const auto& names_ = document_.at("ObjectList");
        if (!names_.is_array())
        {
            throw std::runtime_error("ObjectList must be an array.");
        }
        state_->total = names_.size() + 1;
        state_->completed = 1;
        document_["Objects"] = nlohmann::json::object();
        std::unordered_set<std::string> seen_;
        for (const auto& entry_ : names_)
        {
            if (stop_.stop_requested())
            {
                throw std::runtime_error("Scene loading cancelled.");
            }
            const auto name_ = entry_.get<std::string>();
            ValidateName(name_);
            if (seen_.insert(name_).second)
            {
                document_["Objects"][name_] = ReadJson(base_ / "Objects" / (std::filesystem::path(std::u8string(name_.begin(), name_.end())) += ".json"));
            }
            ++state_->completed;
        }
        std::lock_guard<std::mutex> lock_(state_->mutex);
        state_->document = std::move(document_);
    }
    catch (const std::exception& exception_)
    {
        std::lock_guard<std::mutex> lock_(state_->mutex);
        state_->error = exception_.what();
    }
    catch (...)
    {
        std::lock_guard<std::mutex> lock_(state_->mutex);
        state_->error = "Unknown scene read failure.";
    }
    state_->finished.store(true, std::memory_order_release);
}

bool SceneLoadRequest::IsFinished() const
{
    return shared->finished.load(std::memory_order_acquire);
}

float SceneLoadRequest::GetProgress() const
{
    const auto total_ = shared->total.load();
    return total_ > 0 ? (std::min)(1.0f, static_cast<float>(shared->completed.load()) / static_cast<float>(total_)) : 0.0f;
}

bool SceneLoadRequest::TakeResult(nlohmann::json& document_, std::string& error_)
{
    if (!IsFinished())
    {
        return false;
    }
    std::lock_guard<std::mutex> lock_(shared->mutex);
    error_ = shared->error;
    if (!error_.empty())
    {
        return false;
    }
    document_ = std::move(shared->document);
    return true;
}

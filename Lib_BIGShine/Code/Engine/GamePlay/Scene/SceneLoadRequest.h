#pragma once
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <nlohmann/json.hpp>

// ファイル読込とJSON解析だけを行い、Scene・Component・GPUへアクセスしない要求。
class SceneLoadRequest
{
public:
    explicit SceneLoadRequest(const std::string& sceneName_);
    ~SceneLoadRequest();
    SceneLoadRequest(const SceneLoadRequest&) = delete;
    SceneLoadRequest& operator=(const SceneLoadRequest&) = delete;
    bool IsFinished() const;
    float GetProgress() const;
    // 完了後だけ結果の所有権を呼び出し側へ渡す。失敗時はエラーを返す。
    bool TakeResult(nlohmann::json& document_, std::string& error_);

private:
    // mutexは結果の受け渡しだけを保護し、I/O待ち中やjoin中には保持しない。
    struct SharedState
    {
        std::atomic<bool> finished{false};
        std::atomic<std::size_t> completed{0};
        std::atomic<std::size_t> total{1};
        std::mutex mutex;
        nlohmann::json document;
        std::string error;
    };
    static void ReadScene(std::stop_token stop_, std::shared_ptr<SharedState> state_, std::string sceneName_);
    std::shared_ptr<SharedState> shared;
    // 破棄時は次のファイル境界で中断してjoinする。ワーカーへthisを渡さない。
    std::jthread worker;
};

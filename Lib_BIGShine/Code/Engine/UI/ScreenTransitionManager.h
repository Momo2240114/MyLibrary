#pragma once
#include "TransitionSequence.h"
#include "ITransitionEffect.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
class Component;
using TransitionHandle = std::uint64_t;
// Sceneに依存せず、最前面の演出要求とSceneをまたぐマスクの寿命を管理する。
class ScreenTransitionManager
{
public:
    static ScreenTransitionManager& Instance(); 
    void Initialize();
    void Shutdown();
    // 優先度が高い要求だけが既存演出を置換する。同順位は最初の要求を保持する。
    TransitionHandle Request(const TransitionParameters& parameters_, TransitionPlayback mode_,
        std::function<void()> onCovered_ = {}, std::function<void()> onCompleted_ = {},
        std::weak_ptr<Component> owner_ = {}, bool trackOwner_ = false, bool persistent_ = false, int priority_ = 0, bool preview_ = false);
    // フレーム開始時に一度呼び、前フレームの被覆通知をObject更新前に実行する。
    void Update(float rawDeltaTime_);
    // 主画面のImGui描画後、Present前に一度だけ合成する。
    void Render();
    void DrawGUI(bool editor_);
    void Hold(TransitionHandle handle_);
    void Reveal(TransitionHandle handle_);
    void CompleteWork(TransitionHandle handle_);
    void Cancel(TransitionHandle handle_, TransitionCancelMode mode_ = TransitionCancelMode::CLEAR);
    // 完全被覆中の演出を引き継ぎ、Scene破棄による所有Componentの消失から保護する。
    TransitionHandle RetainCovered();
    TransitionParameters GetProfile(const std::string& profile_,const std::string& context_) const;
    TransitionParameters GetPreset(const std::string& name_) const;
    bool LoadSettings();
    bool SavePreset(const std::string& name_,const TransitionParameters& parameters_);
    bool IsActive() const { return sequence.IsActive(); }
    bool IsPreview() const { return preview; }
    bool IsCovered(TransitionHandle handle_) const { return handle_ && handle_ == handle && sequence.IsCovered(); }
    bool Owns(TransitionHandle handle_) const { return handle_ && handle_ == handle; }
    bool IsInputBlocked() const { return IsActive() && !preview && parameters.blockInput; }
    TransitionHandle GetHandle() const { return handle; }
    TransitionState GetState() const { return sequence.GetState(); }
    static void DrawParameters(TransitionParameters& parameters_);
private:
    bool IsOwnerAlive() const;
    TransitionSequence sequence;
    TransitionParameters parameters;
    TransitionHandle handle = 0;
    TransitionHandle nextHandle = 1;
    std::unique_ptr<ITransitionEffect> effect;
    std::weak_ptr<Component> owner;
    std::function<void()> onCovered;
    std::function<void()> onCompleted;
    bool trackOwner = false;
    bool persistent = false;
    bool preview = false;
    bool fallbackQueued = false;
    int priority = 0;
    nlohmann::json settings = nlohmann::json::object();
    TransitionParameters editing;
    bool showSettings = false;
    bool previewCycle = false;
    float previewGap = .6f, previewWait = 0;
    char presetFilter[64]{};
    int editingProfile = 0;
    char presetName[128] = "DefaultFade";
    std::string status;
};

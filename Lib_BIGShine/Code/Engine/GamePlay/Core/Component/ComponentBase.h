#pragma once


#include <memory>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <d3d11.h>
#include <vector>

#include <ImGui.h>
#include <string>
#include <map>

#include "Engine/Math/Vector.h"
#include "Engine/Debug/TracyProfiler.h"
#include <nlohmann/json.hpp>


class GameObject; // 前方宣言
class GameObjectManager; // 前方宣言
class GlTFModel; // 前方宣言

/// GameObjectへ追加される機能単位の基底クラス。
/// 所有元はweak_ptrで参照し、GameObjectとの循環参照を防ぐ。
class Component
{
public:
    bool enabled = true; // Component全体の有効/無効。UpdateやGuiの前提判定に使う。

    virtual ~Component() = default;
    virtual const std::string& GetName() const = 0;
    // GameObjectのLifecycleから呼ばれる基本処理。必要なComponentだけoverrideする。
    virtual void Start() {}
    virtual void Update(float deltaTime_) {}
    virtual void LateUpdate(float elapsedTime_) {}
    virtual void Submit(ID3D11DeviceContext* context_) {}
    virtual void Gui() {}
    // 非同期LoadやRuntime準備が必要なComponent用の状態確認Hook。
    virtual void UpdateLoadState() {}
    virtual bool IsLoadReady() const { return true; }
    virtual bool IsRuntimeReady() const { return true; }
    virtual bool HasRuntimeFailure() const { return false; }
    void DrawGuiProfiled()
    {
        // ComponentごとのGui負荷をTracyで追えるようにしてからGuiを呼ぶ。
        const std::string profileName_ = "Component GUI::" + GetName();
        BS_PROFILE_SCOPE_DYNAMIC(profileName_.c_str());
        Gui();
    }

    // Save用: 引数のJSON Objectへ自分の変数を書き込む。
    virtual void Serialize(nlohmann::json& json_) const {}
    // Load用: 引数のJSON Objectから自分の変数を読み出す。
    virtual void Deserialize(const nlohmann::json& json_) {}
    void DeserializeProfiled(const nlohmann::json& json_)
    {
        // Load時の重いDeserializeをComponent名単位でProfileする。
        const std::string profileName_ = "Component Deserialize::" + GetName();
        BS_PROFILE_SCOPE_DYNAMIC(profileName_.c_str());
        Deserialize(json_);
    }

    //=========================================
    // 所有者(GameObject)の設定
    //=========================================
    void SetGameObject(std::shared_ptr<GameObject> owner_)
    {
        gameObject = owner_;
    }

    //=========================================
    // 所有者(GameObject)の取得
    //=========================================
    std::shared_ptr<GameObject> GetGameObject()
    {
        return gameObject.lock();
    }

    // Component側から所属ObjectのManagerへ短くアクセスする。
    GameObjectManager& GetManager();


    bool IsEnabled() const { return enabled; }
    void SetEnabled(bool enabled_) { enabled = enabled_; }

    bool ShouldUpdate() const { return updateEnabled; }
    void SetUpdateEnabled(bool enabled_) { updateEnabled = enabled_; }
    bool ShouldLateUpdate() const { return lateUpdateEnabled; }
    void SetLateUpdateEnabled(bool enabled_) { lateUpdateEnabled = enabled_; }
    int GetUpdatePriority() const { return updatePriority; }
    void SetUpdatePriority(int priority_) { updatePriority = priority_; }
    int GetLateUpdatePriority() const { return lateUpdatePriority; }
    void SetLateUpdatePriority(int priority_) { lateUpdatePriority = priority_; }
    void ResetUpdateTiming()
    {
        // Editor調整などで変更した更新Timingを標準状態へ戻す。
        updateEnabled = true;
        lateUpdateEnabled = true;
        updatePriority = 0;
        lateUpdatePriority = 0;
    }

    // Startが既に呼ばれたかどうか
    bool IsStarted() const { return isStarted; }

    // フラグを手動で立てる（GameObject側からStartを呼ぶ際にセットする）
    void SetStarted(bool started_) { isStarted = started_; }
    bool IsRequiredForSceneStart() const { return requiredForSceneStart; }
    void SetRequiredForSceneStart(bool required_) { requiredForSceneStart = required_; }

protected:
    bool isStarted = false;
    bool requiredForSceneStart = false;
    bool updateEnabled = true; // GameObject::Updateで呼ぶかどうか。
    bool lateUpdateEnabled = true; // GameObject::LateUpdateで呼ぶかどうか。
    int updatePriority = 0; // 小さい値ほど先にUpdateされる。
    int lateUpdatePriority = 0; // 小さい値ほど先にLateUpdateされる。
    std::weak_ptr<GameObject> gameObject;
};

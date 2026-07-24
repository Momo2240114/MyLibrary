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
    bool enabled = true;

    virtual ~Component() = default;
    virtual const std::string& GetName() const = 0;
    virtual void Start() {}
    virtual void Update(float deltaTime_) {}
    virtual void LateUpdate(float elapsedTime_) {}
    virtual void Submit(ID3D11DeviceContext* context_) {}
    virtual void Gui() {}
    virtual void UpdateLoadState() {}
    virtual bool IsLoadReady() const { return true; }
    virtual bool IsRuntimeReady() const { return true; }
    virtual bool HasRuntimeFailure() const { return false; }
    void DrawGuiProfiled()
    {
        const std::string profileName_ = "Component GUI::" + GetName();
        BS_PROFILE_SCOPE_DYNAMIC(profileName_.c_str());
        Gui();
    }

    // セーブ用：引数の json オブジェクトに自分の変数を書き込む
    virtual void Serialize(nlohmann::json& json_) const {}
    // ロード用：引数の json オブジェクトから自分の変数を読み出す
    virtual void Deserialize(const nlohmann::json& json_) {}
    void DeserializeProfiled(const nlohmann::json& json_)
    {
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

    //あたかも自身のメンバであるかのようにマネージャーを返す
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
    bool updateEnabled = true;
    bool lateUpdateEnabled = true;
    int updatePriority = 0;
    int lateUpdatePriority = 0;
    std::weak_ptr<GameObject> gameObject;
};

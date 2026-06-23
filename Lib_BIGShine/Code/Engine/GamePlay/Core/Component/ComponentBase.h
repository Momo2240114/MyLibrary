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

    // セーブ用：引数の json オブジェクトに自分の変数を書き込む
    virtual void Serialize(nlohmann::json& json_) const {}
    // ロード用：引数の json オブジェクトから自分の変数を読み出す
    virtual void Deserialize(const nlohmann::json& json_) {}

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

    // Startが既に呼ばれたかどうか
    bool IsStarted() const { return isStarted; }

    // フラグを手動で立てる（GameObject側からStartを呼ぶ際にセットする）
    void SetStarted(bool started_) { isStarted = started_; }

protected:
    bool isStarted = false;
    std::weak_ptr<GameObject> gameObject;
};

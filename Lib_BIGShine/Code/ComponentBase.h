#pragma once


#include <memory>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <d3d11.h>
#include <vector>

#include <ImGui.h>
#include <string>
#include <map>

#include"Vector.h"
#include <nlohmann/json.hpp>


class GameObject; // 前方宣言
class GlTFModel; // 前方宣言

// =========================================
// Component 基底クラス
// =========================================
class Component
{
public:
    bool enabled = true;
    std::weak_ptr<GameObject> gameObject;
    virtual ~Component() = default;
    virtual const std::string& GetName() const = 0;
    virtual void Start() {}
    virtual void Update(float deltaTime) {}
    virtual void LateUpdate(float et) {};
    virtual void Submit(ID3D11DeviceContext* context) {}
    virtual void Gui() {}

    // セーブ用：引数の json オブジェクトに自分の変数を書き込む
    virtual void Serialize(nlohmann::json& j) const {}
    // ロード用：引数の json オブジェクトから自分の変数を読み出す
    virtual void Deserialize(const nlohmann::json& j) {}

    //=========================================
    // 所有者(GameObject)の設定
    //=========================================
    void SetGameObject(std::shared_ptr<GameObject> owner)
    {
        gameObject = owner;
    }

    //=========================================
    // 所有者(GameObject)の取得
    //=========================================
    std::shared_ptr<GameObject> GetGameObject()
    {
        return gameObject.lock();
    }

    bool IsEnabled() const { return enabled; }
    void SetEnabled(bool e) { enabled = e; }

    // Startが既に呼ばれたかどうか
    bool IsStarted() const { return isStarted; }

    // フラグを手動で立てる（GameObject側からStartを呼ぶ際にセットする）
    void SetStarted(bool started) { isStarted = true; }

protected:
    bool isStarted = false;
};
#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <memory>
#include <unordered_map>
#include <vector>

#include "Engine/Common/StringId.h"
#include "Engine/Math/Vector.h"

void RegisterAllActions();



class ActionRegistry 
{
public:
    using ActionFunc = std::function<void()>;

    // 現在 Scene の ActionRegistry を取得する。SceneBase::Initialize で差し替えられる前提。
    static ActionRegistry& Instance() 
    {
        return *instance;
    }

    // シーン遷移時にこれを呼んで実体を切り替える
    static void SetInstance(std::shared_ptr<ActionRegistry> newInstance) 
    {
        instance = newInstance;
    }

    // カテゴリを指定して登録する
    // 例: "Camera" / "Fov_0" のように、Editor 上で探しやすい分類に分ける。
    void RegisterAction(const std::string& category, const std::string& name, ActionFunc func);
    static StringId MakeActionId(std::string_view name_);

    // すべてのカテゴリ名を取得
    std::vector<std::string> GetCategories();


    // 特定カテゴリ内のアクション名リストを取得
    std::vector<std::string> GetActionNames(const std::string& category) {
        std::vector<std::string> names;
        if (actions.count(category)) {
            for (auto const& [name, _] : actions[category]) names.push_back(name);
        }
        return names;
    }

    // カテゴリをまたいで名前検索し、最初に見つかった Action を返す。
    ActionFunc GetAction(const std::string& name);

    // Scene 破棄や Component 再登録時に、古い Action を外す。
    bool RemoveAction(const std::string& category, const std::string& actionName);

    // 実行（すべてのカテゴリから検索）
    void Call(const std::string& name);

    bool HasAction(const std::string& name);

    // カテゴリが存在するかチェック
    bool HasCategory(const std::string& category) const;

    // 特定のカテゴリ内にアクションが存在するかチェック（これも便利です）
    bool HasActionInCategory(const std::string& category, const std::string& actionName) const;

    void Clear()
    {
        if(this)
        {
            // Scene 切り替え時に前 Scene の Action が残らないよう全消去する。
            actions.clear();
            actionIndex.clear();
            categoryIndex.clear();
        }
    };
private:
    void RebuildActionIndexForName(const std::string& actionName_);

    static std::shared_ptr<ActionRegistry> instance;

    // actions[カテゴリ][アクション名]
    // 同じカテゴリ内の同名 Action は後から登録したものが上書きされる。
    std::unordered_map<std::string, std::unordered_map<std::string, ActionFunc>> actions;
    std::unordered_map<StringId, ActionFunc> actionIndex;
    std::unordered_map<StringId, std::unordered_map<StringId, ActionFunc>> categoryIndex;
};

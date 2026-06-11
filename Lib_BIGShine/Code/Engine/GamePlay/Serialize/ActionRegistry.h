#pragma once

#include <functional>
#include <string>
#include <memory>

#include "Engine/Math/Vector.h"

void RegisterAllActions();



class ActionRegistry 
{
public:
    using ActionFunc = std::function<void()>;


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
    void RegisterAction(const std::string& category, const std::string& name, ActionFunc func);

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

    ActionFunc GetAction(const std::string& name);

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
            actions.clear();
        }
    };
private:
    static std::shared_ptr<ActionRegistry> instance;

    // actions[カテゴリ][アクション名]
    std::unordered_map<std::string, std::unordered_map<std::string, ActionFunc>> actions;
};
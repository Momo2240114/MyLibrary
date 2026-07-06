#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include <typeinfo>
#include <iostream>

class ResourceManager
{
public:
    ResourceManager() = default;
    ~ResourceManager() { Clear(); }

    //あらゆるリソースを追加する
    template<typename T>
    void Add(const std::string& name, std::shared_ptr<T> resource)
    {
        if (!resource) return;
        // 型を消去して voidポインタの形で保管庫へと格納する
        resourceMap[name] = std::static_pointer_cast<void>(resource);
        std::cout << "[Resource] 登録完了: " << name << " (型: " << typeid(T).name() << ")" << std::endl;
    }

    // その名前を持つリソースが存在するか確かめる
    bool Has(const std::string& name) const
    {
        return resourceMap.find(name) != resourceMap.end();
    }

    // 元の型を指定してリソースを解き放つ
    template<typename T>
    std::shared_ptr<T> Get(const std::string& name)
    {
        auto it = resourceMap.find(name);
        if (it == resourceMap.end()) return nullptr;

        // 虚無のポインタ（void*）から、元の光輝く型へと安全にキャストして返す
        return std::static_pointer_cast<T>(it->second);
    }

    // 書庫の全リソースを解放する
    void Clear()
    {
        resourceMap.clear();
    }

private:
    // 文字列を鍵とし、あらゆる型を呑み込む保管庫
    std::unordered_map<std::string, std::shared_ptr<void>> resourceMap;
};
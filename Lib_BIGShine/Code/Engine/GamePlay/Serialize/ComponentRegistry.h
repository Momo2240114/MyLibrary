#pragma once

#include <functional>
#include <memory>
#include <string>

void RegisterAllComponents();

class Component;
class GameObject;

class ComponentRegistry
{
public:
    using CreateFunc = std::function<std::shared_ptr<Component>(std::shared_ptr<GameObject>, bool)>;

    static ComponentRegistry& Instance()
    {
        static ComponentRegistry inst;
        return inst;
    }

    template<typename T>
    inline void Register(const std::string& name,bool csllStart = true);


    std::shared_ptr<Component> Create(
        const std::string& name,
        std::shared_ptr<GameObject> go,
        bool callStart = true
    );

    const auto& GetAll() const { return creators; }

private:
    std::unordered_map<std::string, CreateFunc> creators;
};



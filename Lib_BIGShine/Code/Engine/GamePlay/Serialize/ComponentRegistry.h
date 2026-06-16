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
    inline void Register(const std::string& name, bool csllStart = true);


    std::shared_ptr<Component> Create(
        const std::string& name,
        std::shared_ptr<GameObject> go,
        bool callStart = true
    );

    // --- íºê⁄ä÷êîÇìoò^Ç∑ÇÈ ---
    void AddCreator(const std::string& name, CreateFunc func)
    {
        creators[name] = func;
    }

    const auto& GetAll() const { return creators; }

    class AutoRegister
    {
    public:
        AutoRegister(const std::string& name, CreateFunc func)
        {
            ComponentRegistry::Instance().AddCreator(name, func);
        }
    };

private:
    std::unordered_map<std::string, CreateFunc> creators;
};


// componentìoò^ópÉ}ÉNÉç
#define REGISTER_COMPONENT(Type, Name) \
static ComponentRegistry::AutoRegister g_Register##Type( \
    Name, \
    [](std::shared_ptr<GameObject> go, bool overrideStart) -> std::shared_ptr<Component> { \
        auto comp = go->AddComponent<Type>(overrideStart); \
        return std::static_pointer_cast<Component>(comp); \
    } \
);
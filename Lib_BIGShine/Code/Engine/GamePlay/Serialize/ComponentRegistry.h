#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

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
    inline void Register(const std::string& name_, bool callStart_ = true);


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
        AutoRegister(const std::string& name_, const std::string& typeName_, bool callStart_, CreateFunc func_)
        {
            auto wrappedFunc_ = [=](std::shared_ptr<GameObject> go_, bool overrideStart_) -> std::shared_ptr<Component>
            {
                const bool finalStart_ = callStart_ && overrideStart_;
                return func_(go_, finalStart_);
            };

            auto& registry_ = ComponentRegistry::Instance();
            registry_.AddCreator(name_, wrappedFunc_);

            if (!typeName_.empty() && typeName_ != name_)
            {
                registry_.AddCreator(typeName_, wrappedFunc_);
            }
        }
    };

private:
    std::unordered_map<std::string, CreateFunc> creators;
};


// componentìoò^ópÉ}ÉNÉç
#define REGISTER_COMPONENT(Type, Name) \
static ComponentRegistry::AutoRegister g_Register##Type( \
    Name, \
    Type().GetName(), \
    true, \
    [](std::shared_ptr<GameObject> go_, bool callStart_) -> std::shared_ptr<Component> { \
        auto comp_ = go_->AddComponent<Type>(callStart_); \
        return std::static_pointer_cast<Component>(comp_); \
    } \
);
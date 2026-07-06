#pragma once
// NameComponent.h
#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include <string>

class NameComponent : public Component
{
public:
    std::string name;

    // コンポーネントとしての名を与えます
    const std::string& GetName() const override {
        static const std::string componentName = "NameComponent";
        return componentName;
    }

    // シリアライズにて名前を保存・復元可能にします
    void Serialize(nlohmann::json& j) const override { j["Name"] = name; }
    void Deserialize(const nlohmann::json& j) override { name = j.value("Name", "NewObject"); }
};
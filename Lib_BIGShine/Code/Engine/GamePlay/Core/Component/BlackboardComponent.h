#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/Math/Vector.h"

#include <string>
#include <unordered_map>
#include <variant>

using BlackBoardValue = std::variant<float, int, bool, DirectX::SimpleMath::Vector2>;
using BlackboardCategory = std::unordered_map<std::string, BlackBoardValue>;

class BlackboardComponent : public Component
{
public:
    const std::string& GetName() const override { static std::string name = "BlackboardComponent"; return name; }

    void Gui() override;
    void Serialize(nlohmann::json& j_) const override;
    void Deserialize(const nlohmann::json& j_) override;

    void SetValue(const std::string& category_, const std::string& key_, BlackBoardValue value_);
    void SetFloat(const std::string& category_, const std::string& key_, float value_);
    void SetInt(const std::string& category_, const std::string& key_, int value_);
    void SetBool(const std::string& category_, const std::string& key_, bool value_);
    void SetVector2(const std::string& category_, const std::string& key_, const DirectX::SimpleMath::Vector2& value_);
    void SetTrigger(const std::string& category_, const std::string& key_);

    bool HasValue(const std::string& category_, const std::string& key_) const;
    const BlackBoardValue* GetValue(const std::string& category_, const std::string& key_) const;
    BlackBoardValue* GetValue(const std::string& category_, const std::string& key_);

    bool GetFloat(const std::string& category_, const std::string& key_, float& value_) const;
    bool GetInt(const std::string& category_, const std::string& key_, int& value_) const;
    bool GetBool(const std::string& category_, const std::string& key_, bool& value_) const;
    bool GetVector2(const std::string& category_, const std::string& key_, DirectX::SimpleMath::Vector2& value_) const;
    bool ConsumeTrigger(const std::string& category_, const std::string& key_);

    void ResetTrigger(const std::string& category_, const std::string& key_);
    void EraseValue(const std::string& category_, const std::string& key_);
    void ClearCategory(const std::string& category_);

    const BlackboardCategory& GetCategory(const std::string& category_) const;
    BlackboardCategory& GetOrCreateCategory(const std::string& category_);
    const std::unordered_map<std::string, BlackboardCategory>& GetBoard() const { return board; }

    static float ToFloat(const BlackBoardValue& value_);

private:
    std::unordered_map<std::string, BlackboardCategory> board;
};

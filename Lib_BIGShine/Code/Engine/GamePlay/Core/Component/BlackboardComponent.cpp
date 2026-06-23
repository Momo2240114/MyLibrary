#include "BlackboardComponent.h"

#include <algorithm>
#include <imgui.h>
#include <type_traits>

namespace
{
    nlohmann::json MakeValueJson(const BlackBoardValue& value_)
    {
        nlohmann::json valueJson_;

        std::visit([&](auto&& arg_) {
            using T = std::decay_t<decltype(arg_)>;

            if constexpr (std::is_same_v<T, float>)
            {
                valueJson_["type"] = "float";
                valueJson_["value"] = arg_;
            }
            else if constexpr (std::is_same_v<T, int>)
            {
                valueJson_["type"] = "int";
                valueJson_["value"] = arg_;
            }
            else if constexpr (std::is_same_v<T, bool>)
            {
                valueJson_["type"] = "bool";
                valueJson_["value"] = arg_;
            }
            else if constexpr (std::is_same_v<T, DirectX::SimpleMath::Vector2>)
            {
                valueJson_["type"] = "vector2";
                valueJson_["value"] = { arg_.x, arg_.y };
            }
            else if constexpr (std::is_same_v<T, DirectX::SimpleMath::Vector3>)
            {
                valueJson_["type"] = "vector3";
                valueJson_["value"] = { arg_.x, arg_.y, arg_.z };
            }
            }, value_);

        return valueJson_;
    }

    BlackBoardValue MakeValueFromJson(const nlohmann::json& valueJson_)
    {
        const std::string type_ = valueJson_.value("type", "float");

        if (type_ == "int")
        {
            return valueJson_.value("value", 0);
        }
        if (type_ == "bool")
        {
            return valueJson_.value("value", false);
        }
        if (type_ == "vector2")
        {
            DirectX::SimpleMath::Vector2 value_{ 0.0f, 0.0f };
            if (valueJson_.contains("value") && valueJson_["value"].is_array())
            {
                if (valueJson_["value"].size() > 0) value_.x = valueJson_["value"][0].get<float>();
                if (valueJson_["value"].size() > 1) value_.y = valueJson_["value"][1].get<float>();
            }
            return value_;
        }
        if (type_ == "vector3")
        {
            DirectX::SimpleMath::Vector3 value_{ 0.0f, 0.0f, 0.0f };
            if (valueJson_.contains("value") && valueJson_["value"].is_array())
            {
                if (valueJson_["value"].size() > 0) value_.x = valueJson_["value"][0].get<float>();
                if (valueJson_["value"].size() > 1) value_.y = valueJson_["value"][1].get<float>();
                if (valueJson_["value"].size() > 2) value_.z = valueJson_["value"][2].get<float>();
            }
            return value_;
        }
        return valueJson_.value("value", 0.0f);
    }
}

void BlackboardComponent::Gui()
{
    static char newCategory_[64] = "Anim";
    static char newKey_[64] = "";
    static int newType_ = 0;
    const char* typeNames_[] = { "Float", "Int", "Bool", "Vector2", "Vector3", "Trigger" };

    ImGui::InputText("Category", newCategory_, IM_ARRAYSIZE(newCategory_));
    ImGui::InputText("Key", newKey_, IM_ARRAYSIZE(newKey_));
    ImGui::Combo("Type", &newType_, typeNames_, IM_ARRAYSIZE(typeNames_));

    if (ImGui::Button("Add Blackboard Value"))
    {
        const std::string category_(newCategory_);
        const std::string key_(newKey_);

        if (!category_.empty() && !key_.empty())
        {
            switch (newType_)
            {
            case 0: SetFloat(category_, key_, 0.0f); break;
            case 1: SetInt(category_, key_, 0); break;
            case 2: SetBool(category_, key_, false); break;
            case 3: SetVector2(category_, key_, { 0.0f, 0.0f }); break;
            case 4: SetVector3(category_, key_, { 0.0f, 0.0f, 0.0f }); break;
            case 5: SetTrigger(category_, key_); ResetTrigger(category_, key_); break;
            default: break;
            }

            newKey_[0] = '\0';
        }
    }

    std::string categoryToDelete_;
    std::string keyToDelete_;

    for (auto& categoryPair_ : board)
    {
        const std::string& categoryName_ = categoryPair_.first;
        auto& category_ = categoryPair_.second;

        if (!ImGui::TreeNode(categoryName_.c_str()))
        {
            continue;
        }

        for (auto& valuePair_ : category_)
        {
            const std::string& key_ = valuePair_.first;
            auto& value_ = valuePair_.second;

            ImGui::PushID(key_.c_str());
            ImGui::Text("%s", key_.c_str());
            ImGui::SameLine(180.0f);

            if (auto* floatValue_ = std::get_if<float>(&value_))
            {
                ImGui::DragFloat("##Value", floatValue_, 0.05f);
            }
            else if (auto* intValue_ = std::get_if<int>(&value_))
            {
                ImGui::InputInt("##Value", intValue_);
            }
            else if (auto* boolValue_ = std::get_if<bool>(&value_))
            {
                ImGui::Checkbox("##Value", boolValue_);
            }
            else if (auto* vector2Value_ = std::get_if<DirectX::SimpleMath::Vector2>(&value_))
            {
                ImGui::DragFloat2("##Value", &vector2Value_->x, 0.05f);
            }
            else if (auto* vector3Value_ = std::get_if<DirectX::SimpleMath::Vector3>(&value_))
            {
                ImGui::DragFloat3("##Value", &vector3Value_->x, 0.05f);
            }
            ImGui::SameLine();
            if (ImGui::Button("Remove"))
            {
                categoryToDelete_ = categoryName_;
                keyToDelete_ = key_;
            }

            ImGui::PopID();
        }

        ImGui::TreePop();
    }

    if (!categoryToDelete_.empty() && !keyToDelete_.empty())
    {
        EraseValue(categoryToDelete_, keyToDelete_);
    }
}

void BlackboardComponent::Serialize(nlohmann::json& j_) const
{
    j_ = nlohmann::json::object();

    for (const auto& categoryPair_ : board)
    {
        const auto& categoryName_ = categoryPair_.first;
        const auto& category_ = categoryPair_.second;

        for (const auto& valuePair_ : category_)
        {
            j_["Categories"][categoryName_][valuePair_.first] = MakeValueJson(valuePair_.second);
        }
    }
}

void BlackboardComponent::Deserialize(const nlohmann::json& j_)
{
    board.clear();

    if (!j_.contains("Categories") || !j_["Categories"].is_object())
    {
        return;
    }

    for (const auto& categoryItem_ : j_["Categories"].items())
    {
        const std::string& categoryName_ = categoryItem_.key();

        if (!categoryItem_.value().is_object())
        {
            continue;
        }

        for (const auto& valueItem_ : categoryItem_.value().items())
        {
            board[categoryName_][valueItem_.key()] = MakeValueFromJson(valueItem_.value());
        }
    }
}

void BlackboardComponent::SetValue(const std::string& category_, const std::string& key_, BlackBoardValue value_)
{
    board[category_][key_] = value_;
}

void BlackboardComponent::SetFloat(const std::string& category_, const std::string& key_, float value_)
{
    SetValue(category_, key_, value_);
}

void BlackboardComponent::SetInt(const std::string& category_, const std::string& key_, int value_)
{
    SetValue(category_, key_, value_);
}

void BlackboardComponent::SetBool(const std::string& category_, const std::string& key_, bool value_)
{
    SetValue(category_, key_, value_);
}

void BlackboardComponent::SetVector2(const std::string& category_, const std::string& key_, const DirectX::SimpleMath::Vector2& value_)
{
    SetValue(category_, key_, value_);
}

void BlackboardComponent::SetVector3(const std::string& category_, const std::string& key_, const DirectX::SimpleMath::Vector3& value_)
{
    SetValue(category_, key_, value_);
}

void BlackboardComponent::SetTrigger(const std::string& category_, const std::string& key_)
{
    SetBool(category_, key_, true);
}

bool BlackboardComponent::HasValue(const std::string& category_, const std::string& key_) const
{
    const auto categoryIt_ = board.find(category_);
    if (categoryIt_ == board.end()) return false;

    return categoryIt_->second.find(key_) != categoryIt_->second.end();
}

const BlackBoardValue* BlackboardComponent::GetValue(const std::string& category_, const std::string& key_) const
{
    const auto categoryIt_ = board.find(category_);
    if (categoryIt_ == board.end()) return nullptr;

    const auto valueIt_ = categoryIt_->second.find(key_);
    if (valueIt_ == categoryIt_->second.end()) return nullptr;

    return &valueIt_->second;
}

BlackBoardValue* BlackboardComponent::GetValue(const std::string& category_, const std::string& key_)
{
    auto categoryIt_ = board.find(category_);
    if (categoryIt_ == board.end()) return nullptr;

    auto valueIt_ = categoryIt_->second.find(key_);
    if (valueIt_ == categoryIt_->second.end()) return nullptr;

    return &valueIt_->second;
}

bool BlackboardComponent::GetFloat(const std::string& category_, const std::string& key_, float& value_) const
{
    const auto* valuePtr_ = GetValue(category_, key_);
    if (!valuePtr_) return false;

    value_ = ToFloat(*valuePtr_);
    return true;
}

bool BlackboardComponent::GetInt(const std::string& category_, const std::string& key_, int& value_) const
{
    const auto* valuePtr_ = GetValue(category_, key_);
    if (!valuePtr_) return false;

    if (auto intValue_ = std::get_if<int>(valuePtr_))
    {
        value_ = *intValue_;
        return true;
    }

    value_ = static_cast<int>(ToFloat(*valuePtr_));
    return true;
}

bool BlackboardComponent::GetBool(const std::string& category_, const std::string& key_, bool& value_) const
{
    const auto* valuePtr_ = GetValue(category_, key_);
    if (!valuePtr_) return false;

    value_ = ToFloat(*valuePtr_) >= 0.5f;
    return true;
}

bool BlackboardComponent::GetVector2(const std::string& category_, const std::string& key_, DirectX::SimpleMath::Vector2& value_) const
{
    const auto* valuePtr_ = GetValue(category_, key_);
    if (!valuePtr_) return false;

    if (auto vector2Value_ = std::get_if<DirectX::SimpleMath::Vector2>(valuePtr_))
    {
        value_ = *vector2Value_;
        return true;
    }

    return false;
}

bool BlackboardComponent::GetVector3(const std::string& category_, const std::string& key_, DirectX::SimpleMath::Vector3& value_) const
{
    const auto* valuePtr_ = GetValue(category_, key_);
    if (!valuePtr_) return false;
    if (auto vector3Value_ = std::get_if<DirectX::SimpleMath::Vector3>(valuePtr_))
    {
        value_ = *vector3Value_;
        return true;
    }
    return false;
}

bool BlackboardComponent::ConsumeTrigger(const std::string& category_, const std::string& key_)
{
    auto* valuePtr_ = GetValue(category_, key_);
    if (!valuePtr_) return false;

    const bool isTriggered_ = ToFloat(*valuePtr_) >= 0.5f;
    if (isTriggered_)
    {
        *valuePtr_ = false;
    }

    return isTriggered_;
}

void BlackboardComponent::ResetTrigger(const std::string& category_, const std::string& key_)
{
    if (auto* valuePtr_ = GetValue(category_, key_))
    {
        *valuePtr_ = false;
    }
}

void BlackboardComponent::EraseValue(const std::string& category_, const std::string& key_)
{
    auto categoryIt_ = board.find(category_);
    if (categoryIt_ == board.end()) return;

    categoryIt_->second.erase(key_);
    if (categoryIt_->second.empty())
    {
        board.erase(categoryIt_);
    }
}

void BlackboardComponent::ClearCategory(const std::string& category_)
{
    board.erase(category_);
}

const BlackboardCategory& BlackboardComponent::GetCategory(const std::string& category_) const
{
    static const BlackboardCategory emptyCategory_;

    const auto categoryIt_ = board.find(category_);
    if (categoryIt_ == board.end()) return emptyCategory_;

    return categoryIt_->second;
}

BlackboardCategory& BlackboardComponent::GetOrCreateCategory(const std::string& category_)
{
    return board[category_];
}

float BlackboardComponent::ToFloat(const BlackBoardValue& value_)
{
    return std::visit([](auto&& arg_) -> float {
        using T = std::decay_t<decltype(arg_)>;

        if constexpr (std::is_same_v<T, float>) return arg_;
        if constexpr (std::is_same_v<T, int>) return static_cast<float>(arg_);
        if constexpr (std::is_same_v<T, bool>) return arg_ ? 1.0f : 0.0f;
        if constexpr (std::is_same_v<T, DirectX::SimpleMath::Vector2>) return arg_.Length();
        if constexpr (std::is_same_v<T, DirectX::SimpleMath::Vector3>) return arg_.Length();
        return 0.0f;
    }, value_);

}

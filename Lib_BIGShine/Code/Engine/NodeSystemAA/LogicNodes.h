#pragma once
#include "NodeBase.h"

namespace NodeSystem {

    /// 入力Aが入力Bより大きいかを判定する比較ノード。
    class GreaterNode : public Node {
    public:
        GreaterNode(uintptr_t id_) : Node(id_, "Greater Than") {
            inputs.emplace_back(0, "A", PinType::FLOAT, PinDirection::INPUT);
            inputs.emplace_back(0, "B", PinType::FLOAT, PinDirection::INPUT);
            outputs.emplace_back(0, "Result", PinType::BOOL, PinDirection::OUTPUT);
        }

        void Evaluate() override {
            float valueA_ = AsFloat(inputs[0].value);
            float valueB_ = AsFloat(inputs[1].value);
            outputs[0].value = valueA_ > valueB_;
        }
    };

    /// 2つの真偽値の論理積を出力するノード。
    class AndNode : public Node {
    public:
        AndNode(uintptr_t id_) : Node(id_, "Logical AND") {
            inputs.emplace_back(0, "A", PinType::BOOL, PinDirection::INPUT);
            inputs.emplace_back(0, "B", PinType::BOOL, PinDirection::INPUT);
            outputs.emplace_back(0, "Result", PinType::BOOL, PinDirection::OUTPUT);
        }

        void Evaluate() override {
            bool valueA_ = false;
            if (auto boolValue_ = std::get_if<bool>(&inputs[0].value)) valueA_ = *boolValue_;

            bool valueB_ = false;
            if (auto boolValue_ = std::get_if<bool>(&inputs[1].value)) valueB_ = *boolValue_;

            outputs[0].value = valueA_ && valueB_;
        }
    };

    /// 条件に応じて2つの入力値から一方を選択するノード。
    class SelectNode : public Node {
    public:
        SelectNode(uintptr_t id_) : Node(id_, "Select (Float)") {
            inputs.emplace_back(0, "Condition", PinType::BOOL, PinDirection::INPUT);
            inputs.emplace_back(0, "True Value", PinType::FLOAT, PinDirection::INPUT);
            inputs.emplace_back(0, "False Value", PinType::FLOAT, PinDirection::INPUT);
            outputs.emplace_back(0, "Result", PinType::FLOAT, PinDirection::OUTPUT);
        }

        void Evaluate() override {
            bool condition_ = false;
            if (auto boolValue_ = std::get_if<bool>(&inputs[0].value)) condition_ = *boolValue_;

            outputs[0].value = condition_ ? inputs[1].value : inputs[2].value;
        }
    };
}

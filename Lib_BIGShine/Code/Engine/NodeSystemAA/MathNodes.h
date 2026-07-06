#pragma once
#include "NodeBase.h"
#include <algorithm>

namespace NodeSystem
{

    /// 2つの入力値を加算する演算ノード。
    class AddNode : public Node {
    public:
        AddNode(uintptr_t id_) : Node(id_, "Add") {
            inputs.emplace_back(0, "A", PinType::FLOAT, PinDirection::INPUT);
            inputs.emplace_back(0, "B", PinType::FLOAT, PinDirection::INPUT);
            outputs.emplace_back(0, "Result", PinType::FLOAT, PinDirection::OUTPUT);
        }

        void Evaluate() override {
            float valueA_ = AsFloat(inputs[0].value);
            float valueB_ = AsFloat(inputs[1].value);
            outputs[0].value = valueA_ + valueB_;
        }
    };

    /// 2つの入力値を乗算する演算ノード。
    class MultiplyNode : public Node {
    public:
        MultiplyNode(uintptr_t id_) : Node(id_, "Multiply") {
            inputs.emplace_back(0, "A", PinType::FLOAT, PinDirection::INPUT);
            inputs.emplace_back(0, "B", PinType::FLOAT, PinDirection::INPUT);
            outputs.emplace_back(0, "Result", PinType::FLOAT, PinDirection::OUTPUT);
        }

        void Evaluate() override {
            float valueA_ = AsFloat(inputs[0].value);
            float valueB_ = AsFloat(inputs[1].value);
            outputs[0].value = valueA_ * valueB_;
        }
    };

    /// 入力値を指定された最小値と最大値の範囲へ制限する演算ノード。
    class ClampNode : public Node {
    public:
        ClampNode(uintptr_t id_) : Node(id_, "Clamp") {
            inputs.emplace_back(0, "Value", PinType::FLOAT, PinDirection::INPUT);
            inputs.emplace_back(0, "Min", PinType::FLOAT, PinDirection::INPUT);
            inputs.emplace_back(0, "Max", PinType::FLOAT, PinDirection::INPUT);
            outputs.emplace_back(0, "Result", PinType::FLOAT, PinDirection::OUTPUT);
        }

        void Evaluate() override {
            float value_ = AsFloat(inputs[0].value);
            float minValue_ = AsFloat(inputs[1].value);
            float maxValue_ = AsFloat(inputs[2].value);
            outputs[0].value = std::clamp(value_, minValue_, maxValue_);
        }
    };
}

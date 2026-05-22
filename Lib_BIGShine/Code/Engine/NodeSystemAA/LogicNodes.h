#pragma once
#include "NodeBase.h"

namespace NodeSystem {

    // --- 比較ノード (Greater Than) ---
    class GreaterNode : public Node {
    public:
        GreaterNode(uintptr_t id) : Node(id, "Greater Than") {
            Inputs.emplace_back(0, "A", PinType::Float, PinDirection::Input);
            Inputs.emplace_back(0, "B", PinType::Float, PinDirection::Input);
            Outputs.emplace_back(0, "Result", PinType::Bool, PinDirection::Output);
        }

        void Evaluate() override {
            float a = AsFloat(Inputs[0].Value);
            float b = AsFloat(Inputs[1].Value);
            Outputs[0].Value = (a > b);
        }
    };

    // --- 論理積ノード (AND) ---
    class AndNode : public Node {
    public:
        AndNode(uintptr_t id) : Node(id, "Logical AND") {
            Inputs.emplace_back(0, "A", PinType::Bool, PinDirection::Input);
            Inputs.emplace_back(0, "B", PinType::Bool, PinDirection::Input);
            Outputs.emplace_back(0, "Result", PinType::Bool, PinDirection::Output);
        }

        void Evaluate() override {
            // boolの場合は variant から直接取り出す（AsFloatを使わない場合）
            bool a = false;
            if (auto p = std::get_if<bool>(&Inputs[0].Value)) a = *p;

            bool b = false;
            if (auto p = std::get_if<bool>(&Inputs[1].Value)) b = *p;

            Outputs[0].Value = a && b;
        }
    };

    // --- セレクターノード (Condition ? A : B) ---
    class SelectNode : public Node {
    public:
        SelectNode(uintptr_t id) : Node(id, "Select (Float)") {
            Inputs.emplace_back(0, "Condition", PinType::Bool, PinDirection::Input);
            Inputs.emplace_back(0, "True Value", PinType::Float, PinDirection::Input);
            Inputs.emplace_back(0, "False Value", PinType::Float, PinDirection::Input);
            Outputs.emplace_back(0, "Result", PinType::Float, PinDirection::Output);
        }

        void Evaluate() override {
            bool cond = false;
            if (auto p = std::get_if<bool>(&Inputs[0].Value)) cond = *p;

            Outputs[0].Value = cond ? Inputs[1].Value : Inputs[2].Value;
        }
    };
}
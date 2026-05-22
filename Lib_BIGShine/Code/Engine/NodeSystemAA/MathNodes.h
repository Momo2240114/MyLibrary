#pragma once
#include "NodeBase.h"
#include <algorithm>

namespace NodeSystem 
{

    // --- â¡éZÉmÅ[Éh (A + B) ---
    class AddNode : public Node {
    public:
        AddNode(uintptr_t id) : Node(id, "Add") {
            Inputs.emplace_back(0, "A", PinType::Float, PinDirection::Input);
            Inputs.emplace_back(0, "B", PinType::Float, PinDirection::Input);
            Outputs.emplace_back(0, "Result", PinType::Float, PinDirection::Output);
        }

        void Evaluate() override {
            float a = AsFloat(Inputs[0].Value);
            float b = AsFloat(Inputs[1].Value);
            Outputs[0].Value = a + b;
        }
    };

    // --- èÊéZÉmÅ[Éh (A * B) ---
    class MultiplyNode : public Node {
    public:
        MultiplyNode(uintptr_t id) : Node(id, "Multiply") {
            Inputs.emplace_back(0, "A", PinType::Float, PinDirection::Input);
            Inputs.emplace_back(0, "B", PinType::Float, PinDirection::Input);
            Outputs.emplace_back(0, "Result", PinType::Float, PinDirection::Output);
        }

        void Evaluate() override {
            float a = AsFloat(Inputs[0].Value);
            float b = AsFloat(Inputs[1].Value);
            Outputs[0].Value = a * b;
        }
    };

    // --- ÉNÉâÉìÉvÉmÅ[Éh (íl, ç≈è¨, ç≈ëÂ) ---
    class ClampNode : public Node {
    public:
        ClampNode(uintptr_t id) : Node(id, "Clamp") {
            Inputs.emplace_back(0, "Value", PinType::Float, PinDirection::Input);
            Inputs.emplace_back(0, "Min", PinType::Float, PinDirection::Input);
            Inputs.emplace_back(0, "Max", PinType::Float, PinDirection::Input);
            Outputs.emplace_back(0, "Result", PinType::Float, PinDirection::Output);
        }

        void Evaluate() override {
            float val = AsFloat(Inputs[0].Value);
            float minV = AsFloat(Inputs[1].Value);
            float maxV = AsFloat(Inputs[2].Value);
            Outputs[0].Value = std::clamp(val, minV, maxV);
        }
    };
}
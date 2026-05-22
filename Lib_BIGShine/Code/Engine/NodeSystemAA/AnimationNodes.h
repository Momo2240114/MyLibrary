#pragma once

#include <unordered_map>
#include <string>
#include <variant>

// アップロードされたファイル名と構造に合わせる
#include "NodeValue.h"
#include "NodeBase.h"

namespace NodeSystem {

    // --- Blackboard (変数) 読み込みノード ---
    // 基底クラス Node も NodeSystem 内にあるため、正しく継承させる
    class GetParamNode : public NodeSystem::Node {
    public:
        std::string ParamName;
        // NodeValue.h で定義されている NodeValue を使用
        std::unordered_map<std::string, NodeSystem::NodeValue>* BlackboardRef;

        GetParamNode(uintptr_t id, const std::string& paramName, std::unordered_map<std::string, NodeSystem::NodeValue>* bb)
            : NodeSystem::Node(id, "Get: " + paramName), ParamName(paramName), BlackboardRef(bb)
        {
            // ピンの作成 (NodeSystem 内の列挙型をフルネームで指定)
            Outputs.emplace_back(id + 1, "Value", NodeSystem::PinType::Float, NodeSystem::PinDirection::Output);
        }

        void Evaluate() override {
            if (BlackboardRef) {
                auto it = BlackboardRef->find(ParamName);
                if (it != BlackboardRef->end()) {
                    // Outputs は基底クラス Node のメンバ
                    Outputs[0].Value = it->second;
                }
            }
        }
    };

    // --- アニメーション最終出力ノード ---
    class AnimOutputNode : public NodeSystem::Node {
    public:
        AnimOutputNode(uintptr_t id) : NodeSystem::Node(id, "Anim Output") {
            Inputs.emplace_back(id + 1, "Blend Alpha", NodeSystem::PinType::Float, NodeSystem::PinDirection::Input);
            Inputs.emplace_back(id + 2, "Playback Speed", NodeSystem::PinType::Float, NodeSystem::PinDirection::Input);
        }

        void Evaluate() override {
            // 値の読み出しは外部（Component）から行うため空でOK
        }
    };

} // namespace NodeSystem
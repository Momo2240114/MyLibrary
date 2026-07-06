#pragma once

#include <unordered_map>
#include <string>
#include <variant>

#include "NodeValue.h"
#include "NodeBase.h"

namespace NodeSystem {

    /// Blackboardから指定されたパラメータを読み出し、後続ノードへ出力する。
    class GetParamNode : public NodeSystem::Node {
    public:
        std::string paramName;
        std::unordered_map<std::string, NodeSystem::NodeValue>* blackboardRef;

        GetParamNode(uintptr_t id_, const std::string& paramName_, std::unordered_map<std::string, NodeSystem::NodeValue>* blackboard_)
            : NodeSystem::Node(id_, "Get: " + paramName_), paramName(paramName_), blackboardRef(blackboard_)
        {
            outputs.emplace_back(id_ + 1, "Value", NodeSystem::PinType::FLOAT, NodeSystem::PinDirection::OUTPUT);
        }

        void Evaluate() override {
            if (blackboardRef) {
                auto iterator_ = blackboardRef->find(paramName);
                if (iterator_ != blackboardRef->end()) {
                    outputs[0].value = iterator_->second;
                }
            }
        }
    };

    /// アニメーショングラフの最終結果を外部コンポーネントへ公開する終端ノード。
    class AnimOutputNode : public NodeSystem::Node {
    public:
        AnimOutputNode(uintptr_t id_) : NodeSystem::Node(id_, "Anim Output") {
            inputs.emplace_back(id_ + 1, "Blend Alpha", NodeSystem::PinType::FLOAT, NodeSystem::PinDirection::INPUT);
            inputs.emplace_back(id_ + 2, "Playback Speed", NodeSystem::PinType::FLOAT, NodeSystem::PinDirection::INPUT);
        }

        void Evaluate() override {
            // 終端ノードの入力値は外部コンポーネントが参照するため、追加の評価処理は不要。
        }
    };

} // namespace NodeSystem

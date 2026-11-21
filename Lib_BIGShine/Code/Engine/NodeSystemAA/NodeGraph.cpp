#include "NodeGraph.h"

#include <algorithm>

namespace NodeSystem
{

    void NodeGraph::Update()
    {
        if (needsRebuild)
        {
            Rebuild();
        }

        // ノード評価前にリンク先へ値を転送し、各ノードが最新の入力を参照できるようにする。
        for (const auto& link_ : links) {
            Pin* start_ = FindPin(link_.startPinId);
            Pin* end_ = FindPin(link_.endPinId);
            if (start_ && end_) {
                end_->value = start_->value;
            }
        }

        // 接続関係から構築した順序に従ってノードを評価する。
        for (Node* node_ : evaluationOrder)
        {
            node_->Evaluate();
        }
    }

    void NodeGraph::AddLink(uintptr_t id_, uintptr_t startPinId_, uintptr_t endPinId_)
    {
        links.push_back({ id_, startPinId_, endPinId_ });
        needsRebuild = true;
    }

    void NodeGraph::RemoveLink(uintptr_t linkId_)
    {
        links.erase(
            std::remove_if(links.begin(), links.end(),
                [linkId_](const Link& link_)
        {
            return link_.id == linkId_;
        }),
            links.end()
        );

        // 接続関係の変更を次回Update時の評価順序へ反映する。
        needsRebuild = true;
    }

    void NodeGraph::Rebuild()
    {
        // 現段階では登録順に評価する。循環検出を含むトポロジカルソートは今後の拡張点。
        evaluationOrder.clear();
        for (auto& node_ : nodes) {
            evaluationOrder.push_back(node_.get());
        }
        needsRebuild = false;
    }

    Pin* NodeGraph::FindPin(uintptr_t pinId_)
    {
        for (auto& node_ : nodes) {
            for (auto& pin_ : node_->inputs)
            {
                if (pin_.id == pinId_)
                {
                    return &pin_;
                }
            }
            for (auto& pin_ : node_->outputs)
            {
                if (pin_.id == pinId_)
                {
                    return &pin_;
                }
            }
        }
        return nullptr;
    }

    Node* NodeGraph::FindNodeByPin(uintptr_t pinId_)
    {
        for (auto& node_ : nodes)
        {
            for (auto& pin_ : node_->inputs)
            {
                if (pin_.id == pinId_)
                {
                    return node_.get();
                }
            }
            for (auto& pin_ : node_->outputs)
            {
                if (pin_.id == pinId_)
                {
                    return node_.get();
                }
            }
        }
        return nullptr;
    }

} // namespace NodeSystem

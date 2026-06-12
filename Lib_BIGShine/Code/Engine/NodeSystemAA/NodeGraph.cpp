#include "NodeGraph.h"

namespace NodeSystem 
{

    void NodeGraph::Update() 
    {
        if (m_NeedsRebuild) Rebuild();

        // ノード評価前にリンク先へ値を転送し、各ノードが最新の入力を参照できるようにする。
        for (const auto& link : m_Links) {
            Pin* start = FindPin(link.StartPinID);
            Pin* end = FindPin(link.EndPinID);
            if (start && end) {
                end->Value = start->Value;
            }
        }

        // 接続関係から構築した順序に従ってノードを評価する。
        for (Node* node : m_EvaluationOrder) 
        {
            node->Evaluate();
        }
    }

    void NodeGraph::AddLink(uintptr_t id, uintptr_t startPinID, uintptr_t endPinID) 
    {
        m_Links.push_back({ id, startPinID, endPinID });
        m_NeedsRebuild = true;
    }

    void NodeGraph::RemoveLink(uintptr_t linkID)
    {
        m_Links.erase(
            std::remove_if(m_Links.begin(), m_Links.end(),
                [linkID](const Link& link) { return link.ID == linkID; }),
            m_Links.end()
        );

        // 接続関係の変更を次回Update時の評価順序へ反映する。
        m_NeedsRebuild = true;
    }

    void NodeGraph::Rebuild() 
    {
        // 現段階では登録順に評価する。循環検出を含むトポロジカルソートは今後の拡張点。
        m_EvaluationOrder.clear();
        for (auto& node : m_Nodes) {
            m_EvaluationOrder.push_back(node.get());
        }
        m_NeedsRebuild = false;
    }

    Pin* NodeGraph::FindPin(uintptr_t pinID) 
    {
        for (auto& node : m_Nodes) {
            for (auto& p : node->Inputs) if (p.ID == pinID) return &p;
            for (auto& p : node->Outputs) if (p.ID == pinID) return &p;
        }
        return nullptr;
    }

    Node* NodeGraph::FindNodeByPin(uintptr_t pinID)
    {
        for (auto& node : m_Nodes)
        {
            for (auto& pin : node->Inputs)
            {
                if (pin.ID == pinID) return node.get();
            }
            for (auto& pin : node->Outputs)
            {
                if (pin.ID == pinID) return node.get();
            }
        }
        return nullptr;
    }

} // namespace NodeSystem

#include "NodeGraph.h"

namespace NodeSystem 
{

    void NodeGraph::Update() 
    {
        if (m_NeedsRebuild) Rebuild();

        // 1. 接続されているピン間で値をコピー
        for (const auto& link : m_Links) {
            Pin* start = FindPin(link.StartPinID);
            Pin* end = FindPin(link.EndPinID);
            if (start && end) {
                end->Value = start->Value;
            }
        }

        // 2. 正しい順序で計算実行
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

    // --- リンクの削除 ---
    void NodeGraph::RemoveLink(uintptr_t linkID)
    {
        // 指定されたIDを持つリンクをリストから削除する
        m_Links.erase(
            std::remove_if(m_Links.begin(), m_Links.end(),
                [linkID](const Link& link) { return link.ID == linkID; }),
            m_Links.end()
        );

        // グラフ構造が変わったため再構築フラグを立てる
        m_NeedsRebuild = true;
    }

    void NodeGraph::Rebuild() 
    {
        // 本来は依存関係を解析してトポロジカルソートを行いますが、
        // シンプルな実装として、まずは登録順にリスト化します
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

    // --- ピンIDからその所有ノードを検索 ---
    Node* NodeGraph::FindNodeByPin(uintptr_t pinID)
    {
        for (auto& node : m_Nodes)
        {
            // 入力ピンをチェック
            for (auto& pin : node->Inputs)
            {
                if (pin.ID == pinID) return node.get();
            }
            // 出力ピンをチェック
            for (auto& pin : node->Outputs)
            {
                if (pin.ID == pinID) return node.get();
            }
        }
        return nullptr; // 見つからなかった場合
    }

} // namespace NodeSystem
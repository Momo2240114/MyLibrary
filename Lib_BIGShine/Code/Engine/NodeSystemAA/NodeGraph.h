#pragma once
#include <vector>
#include <unordered_map>
#include <memory>
#include "NodeBase.h"

namespace NodeSystem {

    // 接続情報
    struct Link {
        uintptr_t ID;
        uintptr_t StartPinID; // 出力ピン
        uintptr_t EndPinID;   // 入力ピン
    };

    class NodeGraph {
    public:
        NodeGraph() : m_NextID(1) {} // 1から開始

        // ユニークなIDを発行する
        uintptr_t GenerateUniqueID() {
            return m_NextID++;
        }

        // --- グラフ更新 ---
        void Update(); // 値の転送と全ノードの実行
        void Rebuild(); // 実行順序の再構築（トポロジカルソート）

        // --- ノード操作 ---
        template<typename T, typename... Args>
        T* CreateNode(uintptr_t id, Args&&... args) {
            auto node = std::make_unique<T>(id, std::forward<Args>(args)...);
            auto ptr = node.get();
            m_Nodes.push_back(std::move(node));
            m_NeedsRebuild = true;
            return ptr;
        }

        void AddLink(uintptr_t id, uintptr_t startPinID, uintptr_t endPinID);
        void RemoveLink(uintptr_t linkID);

        // --- エディタ用アクセサ ---
        const std::vector<std::unique_ptr<Node>>& GetNodes() const { return m_Nodes; }
        const std::vector<Link>& GetLinks() const { return m_Links; }

    private:
        std::vector<std::unique_ptr<Node>> m_Nodes;
        std::vector<Link> m_Links;
        uintptr_t m_NextID; // 次に割り当てるID
        // 計算を回す順番に並べたノードリスト
        std::vector<Node*> m_EvaluationOrder;
        bool m_NeedsRebuild = true;

        // ヘルパー
        Pin* FindPin(uintptr_t pinID);
        Node* FindNodeByPin(uintptr_t pinID);
    };
}
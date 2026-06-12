#pragma once
#include <vector>
#include <unordered_map>
#include <memory>
#include "NodeBase.h"

namespace NodeSystem {

    /// ノード間を接続するリンク情報。
    /// 出力ピンから入力ピンへの一方向のデータフローを表す。
    struct Link {
        uintptr_t ID;
        uintptr_t StartPinID; // 出力ピン
        uintptr_t EndPinID;   // 入力ピン
    };

    /// ノードの所有、接続関係、評価順序を一元管理する実行グラフ。
    /// グラフ構造が変更された場合のみ評価順序を再構築し、通常更新時の処理負荷を抑える。
    class NodeGraph {
    public:
        NodeGraph() : m_NextID(1) {} // 0を無効IDとして扱えるよう、1から採番する

        /// ノード、ピン、リンクで共有できる一意なIDを発行する。
        uintptr_t GenerateUniqueID() {
            return m_NextID++;
        }

        /// 接続されたピンへ値を転送した後、評価順序に従って全ノードを実行する。
        void Update();

        /// 現在登録されているノードから評価順序を再構築する。
        void Rebuild();

        /// 指定型のノードを生成してグラフへ追加する。
        /// 戻り値のポインタの所有権はNodeGraphが保持する。
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
        // ノード本体はグラフが所有し、リンクはIDによってピンを参照する。
        std::vector<std::unique_ptr<Node>> m_Nodes;
        std::vector<Link> m_Links;
        uintptr_t m_NextID;

        // Update時に使用する評価順序。構造変更時にのみ再構築する。
        std::vector<Node*> m_EvaluationOrder;
        bool m_NeedsRebuild = true;

        // ID参照を実体へ解決する内部検索関数。
        Pin* FindPin(uintptr_t pinID);
        Node* FindNodeByPin(uintptr_t pinID);
    };
}

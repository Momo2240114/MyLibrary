#pragma once
#include <vector>
#include <unordered_map>
#include <memory>
#include <utility>
#include "NodeBase.h"

namespace NodeSystem {

    /// ノード間を接続するリンク情報。
    /// 出力ピンから入力ピンへの一方向のデータフローを表す。
    struct Link {
        uintptr_t id;
        uintptr_t startPinId;
        uintptr_t endPinId;
    };

    /// ノードの所有、接続関係、評価順序を一元管理する実行グラフ。
    /// グラフ構造が変更された場合のみ評価順序を再構築し、通常更新時の処理負荷を抑える。
    class NodeGraph {
    public:
        NodeGraph() : nextId(1) {} // 0を無効IDとして扱えるよう、1から採番する

        /// ノード、ピン、リンクで共有できる一意なIDを発行する。
        uintptr_t GenerateUniqueId() {
            return nextId++;
        }

        /// 接続されたピンへ値を転送した後、評価順序に従って全ノードを実行する。
        void Update();

        /// 現在登録されているノードから評価順序を再構築する。
        void Rebuild();

        /// 指定型のノードを生成してグラフへ追加する。
        /// 戻り値のポインタの所有権はNodeGraphが保持する。
        template<typename T, typename... Args>
        T* CreateNode(uintptr_t id_, Args&&... args_) {
            auto node_ = std::make_unique<T>(id_, std::forward<Args>(args_)...);
            auto pointer_ = node_.get();
            nodes.push_back(std::move(node_));
            needsRebuild = true;
            return pointer_;
        }

        void AddLink(uintptr_t id_, uintptr_t startPinId_, uintptr_t endPinId_);
        void RemoveLink(uintptr_t linkId_);

        // --- エディタ用アクセサ ---
        const std::vector<std::unique_ptr<Node>>& GetNodes() const { return nodes; }
        const std::vector<Link>& GetLinks() const { return links; }

    private:
        // ノード本体はグラフが所有し、リンクはIDによってピンを参照する。
        std::vector<std::unique_ptr<Node>> nodes;
        std::vector<Link> links;
        uintptr_t nextId;

        // Update時に使用する評価順序。構造変更時にのみ再構築する。
        std::vector<Node*> evaluationOrder;
        bool needsRebuild = true;

        // ID参照を実体へ解決する内部検索関数。
        Pin* FindPin(uintptr_t pinId_);
        Node* FindNodeByPin(uintptr_t pinId_);
    };
}

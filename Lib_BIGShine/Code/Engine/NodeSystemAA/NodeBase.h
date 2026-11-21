#pragma once
#include <vector>
#include <unordered_map>
#include <memory>
#include "NodeValue.h"

namespace NodeSystem
{
    /// ノードの入出力端子。
    /// IDを介してリンクから参照され、valueに評価結果または入力値を保持する。
    struct Pin
    {
        uintptr_t id;
        std::string name;
        PinType type;
        PinDirection direction;
        NodeValue value;

        Pin(uintptr_t id_, const std::string& name_, PinType type_, PinDirection direction_)
            : id(id_), name(name_), type(type_), direction(direction_), value(0.0f)
        {
        }
    };

    /// 実行グラフを構成するノードの抽象基底クラス。
    /// 派生クラスはEvaluateで入力から出力を計算する責務を持つ。
    class Node
    {
    public:
        uintptr_t id;
        std::string name;
        std::vector<Pin> inputs;
        std::vector<Pin> outputs;

        Node(uintptr_t id_, const std::string& name_) : id(id_), name(name_)
        {
        }

        virtual ~Node() = default;

        virtual void Evaluate() = 0;
    };
}

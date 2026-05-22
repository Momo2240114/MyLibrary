#pragma once
#include <vector>
#include <unordered_map>
#include <memory>
#include "NodeValue.h"

namespace NodeSystem
{
    struct Pin
    {
        uintptr_t ID; // 実行時は数値IDのみで管理
        std::string Name;
        PinType Type;
        PinDirection Direction; // ed::PinKind ではなく独自の PinDirection を使用
        NodeValue Value;

        Pin(uintptr_t id, const std::string& name, PinType type, PinDirection dir)
            : ID(id), Name(name), Type(type), Direction(dir), Value(0.0f)
        {
        }
    };

    class Node
    {
    public:
        uintptr_t ID;
        std::string Name;
        std::vector<Pin> Inputs;
        std::vector<Pin> Outputs;

        // コンストラクタを ID と Name の2引数に固定
        Node(uintptr_t id, const std::string& name) : ID(id), Name(name) {}
        virtual ~Node() = default;

        virtual void Evaluate() = 0;
    };
}
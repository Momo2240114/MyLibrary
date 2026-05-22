#pragma once
#include <variant>
#include <string>
#include "Engine/Math/Vector.h" // DirectX::SimpleMath::Vector2


namespace NodeSystem {
    using NodeValue = std::variant<float, int, bool, DirectX::SimpleMath::Vector2>;

    enum class PinType { Float, Int, Bool, Vector2 };

    // --- 追加: Pinの方向（Editorに依存させないための独自定義） ---
    enum class PinDirection { Input, Output };

    // AsFloat をここに実装（MathNodesから見えるようにする）
    inline float AsFloat(const NodeValue& v) {
        if (auto f = std::get_if<float>(&v)) return *f;
        if (auto i = std::get_if<int>(&v)) return static_cast<float>(*i);
        if (auto b = std::get_if<bool>(&v)) return *b ? 1.0f : 0.0f;
        return 0.0f;
    }
}
#pragma once
#include <variant>
#include <string>
#include "Engine/Math/Vector.h" // DirectX::SimpleMath::Vector2


namespace NodeSystem {
    /// ノード間で受け渡し可能な値の集合。
    /// variantを用いることで、型ごとのノード基底クラスを増やさずに値を保持する。
    using NodeValue = std::variant<float, int, bool, DirectX::SimpleMath::Vector2>;

    enum class PinType { FLOAT, INT, BOOL, VECTOR2 };

    /// エディタライブラリへ依存せず、実行グラフ単体で接続方向を判定するための列挙型。
    enum class PinDirection { INPUT, OUTPUT };

    /// 数値演算ノードで扱えるよう、互換性のある値をfloatへ変換する。
    inline float AsFloat(const NodeValue& value_) {
        if (auto floatValue_ = std::get_if<float>(&value_)) return *floatValue_;
        if (auto intValue_ = std::get_if<int>(&value_)) return static_cast<float>(*intValue_);
        if (auto boolValue_ = std::get_if<bool>(&value_)) return *boolValue_ ? 1.0f : 0.0f;
        return 0.0f;
    }
}

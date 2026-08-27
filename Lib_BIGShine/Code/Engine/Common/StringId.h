#pragma once

#include <cstdint>
#include <string_view>

// 文字列キーを実行時比較用の安定した64bit IDへ変換する。
using StringId = uint64_t;

namespace StringIdUtility
{
    constexpr StringId INVALID_STRING_ID = 0u;
    constexpr StringId FNV_OFFSET_BASIS = 14695981039346656037ull;
    constexpr StringId FNV_PRIME = 1099511628211ull;

    // 保存データには文字列を残し、実行時だけこのIDで比較する。
    constexpr StringId Hash(std::string_view text_)
    {
        StringId hash_ = FNV_OFFSET_BASIS;
        for (const char character_ : text_)
        {
            hash_ ^= static_cast<unsigned char>(character_);
            hash_ *= FNV_PRIME;
        }
        return hash_;
    }
}

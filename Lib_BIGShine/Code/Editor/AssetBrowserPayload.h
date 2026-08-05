#pragma once

namespace AssetBrowserPayload
{
    // ImGui Drag&DropでAsset種別を識別するためのPayload名。
    // 受け取り側はこの文字列でTexture/Model/Jsonなどを判定する。
    inline constexpr const char* TEXTURE = "ASSET_TEXTURE";
    inline constexpr const char* MODEL = "ASSET_MODEL";
    inline constexpr const char* JSON = "ASSET_JSON";
    inline constexpr const char* MATERIAL = "ASSET_MATERIAL";
    inline constexpr const char* CURVE = "ASSET_CURVE";
    inline constexpr const char* FILE = "ASSET_FILE";
}

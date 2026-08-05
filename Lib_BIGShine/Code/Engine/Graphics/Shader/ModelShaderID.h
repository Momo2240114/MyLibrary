#pragma once

enum class ModelShaderID
{
    // 基本的な非PBR描画。
    Basic = 0,

    // PBR Material描画。
    PBR,

    // Toon階調描画。
    Toon,
    ToonPBR,

    // ShadowMap生成用。
    ShadowCaster,

    // 配列数やGUIループ上限として使う。
    EnumCount
};

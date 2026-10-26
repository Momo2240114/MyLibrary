#pragma once
#include "Engine/GamePlay/Input/KeyConfig.h"
#include <d3d11.h>
#include <wrl/client.h>
#include <array>

class SpriteRenderer;

// 入力コードと正式素材の対応をUI側へ集約し、3枚のTextureを共有する。
class InputIconAtlas
{
public:
    struct Region
    {
        DirectX::XMFLOAT4 source{ 0,0,0,0 };
        bool valid = false;
    };
    // 既存ResourceLoaderのキャッシュから各シートを一度だけ取得する。
    void Initialize();
    // ピクセル単位の矩形を返す。未収録コードはテキストへフォールバックする。
    static Region Resolve(InputDisplayDevice device_, int code_);
    static InputCombo FindBinding(const std::string& action_, InputDisplayDevice device_);
    static std::string GetKeyLabel(InputDisplayDevice device_, int code_);
    static const char* GetDeviceName(InputDisplayDevice device_);
    // Textureの再読込をせず、既存SpriteへSourceRectを適用する。
    bool Apply(SpriteRenderer& sprite_, InputDisplayDevice device_, int code_) const;
    // 接続状態を変えずに元画像と選択UVを確認する。
    void Gui(InputDisplayDevice device_) const;

private:
    struct Sheet
    {
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture;
        D3D11_TEXTURE2D_DESC description{};
    };
    std::array<Sheet, 3> sheets;
    bool initialized = false;
};

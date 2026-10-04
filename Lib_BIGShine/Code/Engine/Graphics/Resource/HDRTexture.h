#pragma once

#include <cstdint>
#include <cstdio>
#include <d3d11.h>
#include <vector>
#include <wrl/client.h>

// Radiance形式のHDR画像をDirect3D11のShaderResourceViewとして管理するテクスチャ
class HDRTexture
{
public:
    HDRTexture() = default;
    ~HDRTexture() = default;

    // HDRファイルを読み込み、ミップ付きのキューブマップSRVを作成する
    bool Load(
        ID3D11Device* device_,
        ID3D11DeviceContext* deviceContext_,
        const wchar_t* filename_,
        float intensity_ = 1.0f);

    // 指定スロットへPixelShader用SRVとして設定する
    void Set(ID3D11DeviceContext* deviceContext_, int slot_ = 0) const;

    ID3D11ShaderResourceView* GetShaderResourceView() const { return shaderResourceView.Get(); }
    const Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& GetShaderResourceViewComPtr() const { return shaderResourceView; }
    int GetWidth() const { return width; }
    int GetHeight() const { return height; }

private:
    // HDRヘッダーから画像サイズを取得する
    bool CheckHeader(FILE* file_);

    // HDRのRLE圧縮された1行分をRGBAE配列へ展開する
    bool ReadLine(std::vector<std::uint8_t>& scanline_, FILE* file_) const;

    // HDRのRGBEピクセルをfloat RGBAへ変換する
    bool ReadPixels(FILE* file_, std::vector<float>& pixels_) const;

    // 全天球HDR画像をTextureCube用の6面ピクセルへ変換する
    void BuildCubePixels(
        const std::vector<float>& sourcePixels_,
        std::vector<float>& cubePixels_,
        float intensity_) const;

private:
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shaderResourceView;
    int width = 0;
    int height = 0;
    int cubeSize = 0;
};

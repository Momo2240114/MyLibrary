#pragma once

#include "SpriteShader.h"

//=========================================
// SpriteDissolveShader (ディゾルブ用スプライトシェーダ)
//=========================================
class SpriteDissolveShader : public SpriteShader
{
public:
    // ディゾルブ用マスクテクスチャ
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> maskTexture;

    // ディゾルブ進行度 0.0~1.0
    float dissolveAmount = 0.0f;

    SpriteDissolveShader() = default;

    // 定数バッファに値を設定するヘルパー関数
    void SetDissolveParameters(ID3D11DeviceContext* context_, ID3D11Buffer* dissolveCB_)
    {
        struct DissolveCB
        {
            DirectX::XMFLOAT4 parameters; // x = dissolveAmount, yzw unused
        };

        DissolveCB cb_;
        cb_.parameters = DirectX::XMFLOAT4(dissolveAmount, 0, 0, 0);
        context_->UpdateSubresource(dissolveCB_, 0, nullptr, &cb_, 0, 0);
    }
};
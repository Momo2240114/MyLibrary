#include "ShadowCasterShader.h"
#include "Engine/Graphics/Core/CPUResourceLoader.h"

// コンストラクタで「影書き込み用」の頂点シェーダーを読み込む
ShadowCasterShader::ShadowCasterShader(ID3D11Device* device)
{
    // VSだけ読み込む。PSは不要なのでnullptrでよい
    CPUResourceLoader::LoadVertexShader(device, "Shader/ShadowCasterVS.cso",
        GltfModelCommponennt::InputElementDescs.data(),
        static_cast<UINT>(GltfModelCommponennt::InputElementDescs.size()),
        inputLayout.GetAddressOf(), vertexShader.GetAddressOf());
}

void ShadowCasterShader::Begin(const RenderContext& rc)
{
    ID3D11DeviceContext* dc = rc.deviceContext;
    dc->IASetInputLayout(inputLayout.Get());
    dc->VSSetShader(vertexShader.Get(), nullptr, 0);
    dc->PSSetShader(nullptr, nullptr, 0); // 深度のみなのでピクセルシェーダーは不要
}

void ShadowCasterShader::Update(const RenderContext& rc, const GltfModelCommponennt::Mesh& mesh)
{
 
}

void ShadowCasterShader::End(const RenderContext& rc)
{
    // 特に何もせずともシェーダー切り替えで上書きされるが、明示的に解除するなら
    rc.deviceContext->VSSetShader(nullptr, nullptr, 0);
}
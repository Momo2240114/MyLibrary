#include "CPUResourceLoader.h"

#include <filesystem>
#include <wrl.h>

#include "Misc.h"

#include "DDSTextureLoader.h"      


std::map<std::wstring, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> CPUResourceLoader::textureResources;

HRESULT CPUResourceLoader::LoadTextureFromFile(ID3D11Device* device_, 
    const wchar_t* filename_,
    ID3D11ShaderResourceView** shader_resource_view_, 
    D3D11_TEXTURE2D_DESC* texture2d_desc_)
{
    HRESULT hr = S_OK;
    Microsoft::WRL::ComPtr<ID3D11Resource> resource_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv_;

    auto it = textureResources.find(filename_);
    if (it != textureResources.end())
    {
        srv_ = it->second;
        srv_->GetResource(resource_.GetAddressOf());
    }
    else
    {
        // WICではなくDDS専用の関数を使用する
        hr = DirectX::CreateDDSTextureFromFile(device_, filename_, resource_.GetAddressOf(), srv_.GetAddressOf());

        // 失敗した場合はWICで試す
        if (FAILED(hr))
        {
            hr = DirectX::CreateWICTextureFromFile(device_, filename_, resource_.GetAddressOf(), srv_.GetAddressOf());
            _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
            textureResources.insert({ filename_, srv_ });
        }
    }


    // texture2d_desc_ が nullptr でなければ取得
    if (texture2d_desc_)
    {
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture2d_;
        hr = resource_.As(&texture2d_); // QueryInterface の簡易版
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
        texture2d_->GetDesc(texture2d_desc_);
    }
    *shader_resource_view_ = srv_.Detach(); // 呼び出し元に所有権を渡す

    return hr;
}

HRESULT CPUResourceLoader::LoadTextureFromMemory(
    ID3D11Device* device_,
    const void* data, 
    size_t size, 
    ID3D11ShaderResourceView** shader_resource_view_)
{
    HRESULT hr = S_OK;
    Microsoft::WRL::ComPtr<ID3D11Resource> resource;


    //DDS形式のテクスチャデータをして読み込む
    if (hr != S_OK)
    // DDSで試す
    hr = CreateDDSTextureFromMemory(device_,
        reinterpret_cast<const uint8_t*>(data),
        size,
        resource.GetAddressOf(),
        shader_resource_view_ ? shader_resource_view_ : nullptr);

    if (FAILED(hr))
    {
        // WICで試す
        hr = CreateWICTextureFromMemory(device_,
            reinterpret_cast<const uint8_t*>(data),
            size,
            resource.GetAddressOf(),
            shader_resource_view_ ? shader_resource_view_ : nullptr);
    }

    // SRVが nullptr の場合、自前で作る
    if (shader_resource_view_ == nullptr && SUCCEEDED(hr))
    {
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture2D;
        hr = resource->QueryInterface(texture2D.GetAddressOf());
        if (FAILED(hr)) return hr;

        // デバッグ用に SRV なしでもOK
        // 必要に応じてここで他の処理も可能
    }

    return hr;
}




HRESULT CPUResourceLoader::CreateDummyTexture(ID3D11Device* device_, UINT colorRGBA8, ID3D11ShaderResourceView** outSRV_)
{
    if (!device_ || !outSRV_) return E_INVALIDARG;

    UINT32 pixel = colorRGBA8;

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = 1;
    desc.Height = 1;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA init{};
    init.pSysMem = &pixel;
    init.SysMemPitch = sizeof(UINT32);

    Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
    HRESULT hr = device_->CreateTexture2D(&desc, &init, tex.GetAddressOf());
    if (FAILED(hr)) return hr;

    return device_->CreateShaderResourceView(tex.Get(), nullptr, outSRV_);
}

void CPUResourceLoader::ReleaseAllTextures()
{
    textureResources.clear();
}

HRESULT CPUResourceLoader::CreateVSFromCSO(
    ID3D11Device* device_,
    const char* csoName_,
    ID3D11VertexShader** vertexShader_, 
    ID3D11InputLayout** inputLayout_, 
    const D3D11_INPUT_ELEMENT_DESC* inputElementDesc_,
    UINT numElements_)
{
    FILE* fp_{ nullptr };
    fopen_s(&fp_, csoName_, "rb");    // ファイルをバイナリで開く
    if (!fp_) {
        return E_FAIL;
    }

    fseek(fp_, 0, SEEK_END);			// ファイルの終端へ移動
    long csoSz_ = ftell(fp_);		// ファイルサイズ取得
    fseek(fp_, 0, SEEK_SET);			// ファイル先頭へ戻る

    std::unique_ptr<unsigned char[]> csoData_ = std::make_unique<unsigned char[]>(csoSz_);
    fread(csoData_.get(), csoSz_, 1, fp_);
    fclose(fp_);

    HRESULT hr = device_->CreateVertexShader(csoData_.get(), csoSz_, nullptr, vertexShader_);
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    if (inputLayout_)
    {
        hr = device_->CreateInputLayout(inputElementDesc_, numElements_, csoData_.get(), csoSz_, inputLayout_);
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    }

    return hr;
}

HRESULT CPUResourceLoader::CreatePSFromCSO(ID3D11Device* device_, const char* csoName_, ID3D11PixelShader** pixelShader_)
{
    FILE* fp_{ nullptr };
    fopen_s(&fp_, csoName_, "rb");
    if (!fp_) {
        return E_FAIL;
    }


    fseek(fp_, 0, SEEK_END);
    long csoSz_{ ftell(fp_) };
    fseek(fp_, 0, SEEK_SET);

    std::unique_ptr<unsigned char[]> csoData_{ std::make_unique<unsigned char[]>(csoSz_) };
    fread(csoData_.get(), csoSz_, 1, fp_);
    fclose(fp_);

    HRESULT hr{ S_OK };
    hr = device_->CreatePixelShader(csoData_.get(), csoSz_, nullptr, pixelShader_);
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    return hr;

}

HRESULT CPUResourceLoader::LoadVertexShader(ID3D11Device* device, const char* filename, const D3D11_INPUT_ELEMENT_DESC inputElementDescs[], UINT inputElementCount, ID3D11InputLayout** inputLayout, ID3D11VertexShader** vertexShader)
{
    // ファイルを開く
    FILE* fp = nullptr;
    fopen_s(&fp, filename, "rb");
    _ASSERT_EXPR_A(fp, "Vertex Shader File not found");

    // ファイルのサイズを求める
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // メモリ上に頂点シェーダーデータを格納する領域を用意する
    std::unique_ptr<u_char[]> data = std::make_unique<u_char[]>(size);
    fread(data.get(), size, 1, fp);
    fclose(fp);

    // 頂点シェーダー生成
    HRESULT hr = device->CreateVertexShader(data.get(), size, nullptr, vertexShader);
    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

    // 入力レイアウト
    if (inputLayout != nullptr)
    {
        hr = device->CreateInputLayout(inputElementDescs, inputElementCount, data.get(), size, inputLayout);
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
    }

    return hr;
}

HRESULT CPUResourceLoader::LoadPixelShader(ID3D11Device* device, const char* filename, ID3D11PixelShader** pixelShader)
{
    // ファイルを開く
    FILE* fp = nullptr;
    fopen_s(&fp, filename, "rb");
    _ASSERT_EXPR_A(fp, "Pixel Shader File not found");

    // ファイルのサイズを求める
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // メモリ上に頂点シェーダーデータを格納する領域を用意する
    std::unique_ptr<u_char[]> data = std::make_unique<u_char[]>(size);
    fread(data.get(), size, 1, fp);
    fclose(fp);

    // ピクセルシェーダー生成
    HRESULT hr = device->CreatePixelShader(data.get(), size, nullptr, pixelShader);
    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

    return hr;
}

HRESULT CPUResourceLoader::CreateConstantBuffer(ID3D11Device* device, UINT bufferSize, ID3D11Buffer** constantBuffer)
{
    D3D11_BUFFER_DESC desc{};
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;
    desc.ByteWidth = bufferSize;
    desc.StructureByteStride = 0;

    HRESULT hr = device->CreateBuffer(&desc, 0, constantBuffer);
    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

    return hr;
}

HRESULT CPUResourceLoader::LoadGeometryShader(ID3D11Device* device, const char* filename, ID3D11GeometryShader** geometryShader)
{
    // ファイルを開く
    FILE* fp = nullptr;
    fopen_s(&fp, filename, "rb");
    _ASSERT_EXPR_A(fp, "Geometry Shader File not found");

    // ファイルのサイズを求める
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // メモリ上にシェーダーデータを格納する領域を用意する
    std::unique_ptr<unsigned char[]> data = std::make_unique<unsigned char[]>(size);
    fread(data.get(), size, 1, fp);
    fclose(fp);

    // ジオメトリシェーダー生成
    HRESULT hr = device->CreateGeometryShader(data.get(), size, nullptr, geometryShader);
    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

    return hr;
}

HRESULT CPUResourceLoader::LoadComputeShader(ID3D11Device* device, const char* filename, ID3D11ComputeShader** computeShader)
{
    // ファイルを開く
    FILE* fp = nullptr;
    fopen_s(&fp, filename, "rb");
    _ASSERT_EXPR_A(fp, "Compute Shader File not found");

    // ファイルのサイズを求める
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // メモリ上にシェーダーデータを格納する領域を用意する
    std::unique_ptr<unsigned char[]> data = std::make_unique<unsigned char[]>(size);
    fread(data.get(), size, 1, fp);
    fclose(fp);

    // コンピュートシェーダー生成
    HRESULT hr = device->CreateComputeShader(data.get(), size, nullptr, computeShader);
    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

    return hr;
}
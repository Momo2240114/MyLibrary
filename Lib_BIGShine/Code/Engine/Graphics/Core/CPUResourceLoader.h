#pragma once

#include <WICTextureLoader.h>
using namespace DirectX;
#include <wrl.h>
#include <string> 
#include <map>
#include <d3d11.h>
#include <d3dcompiler.h>

enum class TextureType {
	Color, // sRGBとして解釈し、色を正しく表示する
	Data   // Linearとして解釈し、計算用の数値として扱う（法線、ラフネス等）
};

class CPUResourceLoader
{
private:
	static std::map<std::wstring, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> textureResources;

public:
	//テクスチャのロードをモジュール化
	static HRESULT LoadTextureFromFile(
		ID3D11Device* device_,
		const wchar_t* filename_,
		ID3D11ShaderResourceView** shader_resource_view_,
		D3D11_TEXTURE2D_DESC* texture2d_desc_ = nullptr,
		TextureType type = TextureType::Color);;

	static HRESULT LoadTextureFromMemory(
		ID3D11Device* device_,
		const void* data,
		size_t size,
		ID3D11ShaderResourceView** shader_resource_view_ = nullptr,
		TextureType type = TextureType::Color);;

	static HRESULT CreateDummyTexture(
		ID3D11Device* device_,
		UINT colorRGBA8,
		ID3D11ShaderResourceView** outSRV_);

	static void ReleaseAllTextures();

	static HRESULT CreateVSFromCSO(ID3D11Device* device_, const char* csoName_, ID3D11VertexShader** vertexShader_,
		ID3D11InputLayout** inputLayout_, const D3D11_INPUT_ELEMENT_DESC* inputElementDesc_, UINT numElements_);


	static HRESULT CreatePSFromCSO(ID3D11Device* device_,
		const char* csoName_, ID3D11PixelShader** pixelShader_);

	// 頂点シェーダー読み込み
	static HRESULT LoadVertexShader(
		ID3D11Device* device,
		const char* filename,
		const D3D11_INPUT_ELEMENT_DESC inputElementDescs[],
		UINT inputElementCount,
		ID3D11InputLayout** inputLayout,
		ID3D11VertexShader** vertexShader);
	

	// ピクセルシェーダー読み込み
	static HRESULT LoadPixelShader(
		ID3D11Device* device,
		const char* filename,
		ID3D11PixelShader** pixelShader);

	// 定数バッファ作成
	static HRESULT CreateConstantBuffer(
		ID3D11Device* device,
		UINT bufferSize,
		ID3D11Buffer** constantBuffer);

	// ジオメトリシェーダー読み込み
	static HRESULT LoadGeometryShader(
		ID3D11Device* device,
		const char* filename,
		ID3D11GeometryShader** geometryShader);

	// コンピュートシェーダー読み込み
	static HRESULT LoadComputeShader(
		ID3D11Device* device,
		const char* filename,
		ID3D11ComputeShader** computeShader);
	

};
#pragma once


#include <d3d11.h>

// GPUリソースユーティリティ
class GpuResourceUtils
{
public:
	// 頂点シェーダー読み込み
	// CSOを読み込み、必要ならInputLayoutも同時に作成する。
	static HRESULT LoadVertexShader(
		ID3D11Device* device,
		const char* filename,
		const D3D11_INPUT_ELEMENT_DESC inputElementDescs[],
		UINT inputElementCount,
		ID3D11InputLayout** inputLayout,
		ID3D11VertexShader** vertexShader);

	// ピクセルシェーダー読み込み
	// コンパイル済みPSバイナリからPixelShaderを作成する。
	static HRESULT LoadPixelShader(
		ID3D11Device* device,
		const char* filename,
		ID3D11PixelShader** pixelShader);

	// テクスチャ読み込み
	// ファイルパスからSRVを作成し、必要ならTexture2DDescも返す。
	static HRESULT LoadTexture(
		ID3D11Device* device,
		const char* filename,
		ID3D11ShaderResourceView** shaderResourceView,
		D3D11_TEXTURE2D_DESC* texture2dDesc = nullptr);

	// テクスチャ読み込み
	// メモリ上の画像データからSRVを作成する。
	static HRESULT LoadTexture(
		ID3D11Device* device,
		const void* data,
		size_t size,
		ID3D11ShaderResourceView** shaderResourceView,
		D3D11_TEXTURE2D_DESC* texture2dDesc = nullptr);

	// ダミーテクスチャ作成
	// Texture未設定時の代替として、単色1pxのSRVを作る。
	static HRESULT CreateDummyTexture(
		ID3D11Device* device,
		UINT color,
		ID3D11ShaderResourceView** shaderResourceView,
		D3D11_TEXTURE2D_DESC* texture2dDesc = nullptr);

	// 定数バッファ作成
	// HLSL ConstantBufferとして使うDynamic Bufferを作成する。
	static HRESULT CreateConstantBuffer(
		ID3D11Device* device,
		UINT bufferSize,
		ID3D11Buffer** constantBuffer);

};

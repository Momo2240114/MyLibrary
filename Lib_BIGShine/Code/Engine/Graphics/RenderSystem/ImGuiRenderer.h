#pragma once



#include <Windows.h>
#include <d3d11.h>

class ImGuiRenderer
{
public:
	// 初期化
	static void Initialize(HWND hWnd_, ID3D11Device* device_, ID3D11DeviceContext* dc_);

	// 終了化
	static void Finalize();

	// フレーム開始処理
	static void NewFrame();

	// 描画実行
	static void RenderAll(ID3D11DeviceContext* context_);

	// WIN32メッセージハンドラー
	static LRESULT HandleMessage(HWND hWnd_, UINT msg_, WPARAM wParam_, LPARAM lParam_);
};

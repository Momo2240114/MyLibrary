#include "PrecompiledHeader.h"

#include <time.h>

#include "framework.h"
#include "ApplicationResources.h"
#include "Engine/Core/GameRuntimeSettings.h"

extern "C"
{
	// ノートPCなどで統合GPUではなく高性能GPUを優先して使うため、NVIDIA/AMDドライバへ明示する。
	__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}


LRESULT CALLBACK window_procedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	// Win32の静的コールバックから、生成済みframeworkインスタンスのメッセージ処理へ橋渡しする。
	framework* p{ reinterpret_cast<framework*>(GetWindowLongPtr(hwnd, GWLP_USERDATA)) };
	return p ? p->handle_message(hwnd, msg, wparam, lparam) : DefWindowProc(hwnd, msg, wparam, lparam);
}

int WINAPI WinMain(_In_ HINSTANCE instance, _In_opt_  HINSTANCE prev_instance, _In_ LPSTR cmd_line, _In_ int cmd_show)
{
    // 配布フォルダーからの起動では、相対AssetパスをEXEの場所へそろえる。
    std::string runtimeError_;
    if (!GameRuntimeSettings::Initialize(runtimeError_))
    {
        // 日本語を含むDataの絶対パスも、起動エラーで正しく表示する。
        const int messageLength_ = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
            runtimeError_.c_str(), -1, nullptr, 0);
        std::wstring message_;
        if (messageLength_ > 0)
        {
            message_.resize(messageLength_);
            MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, runtimeError_.c_str(), -1,
                message_.data(), messageLength_);
        }
        else
        {
            message_ = L"Game startup failed. Check the Data folder and GamePackage.json.";
        }
        MessageBoxW(nullptr, message_.c_str(), L"Game startup failed", MB_OK | MB_ICONERROR);
        return 1;
    }
	srand(static_cast<unsigned int>(time(nullptr)));

#if defined(DEBUG) | defined(_DEBUG)
	// Debug実行時は終了時にCRTメモリリークを検出できるようにする。
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	//_CrtSetBreakAlloc(55362);
#endif

	// アプリ本体のWindowClassを登録し、すべてのWin32メッセージをwindow_procedureへ集約する。
	WNDCLASSEXW wcex{};
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = window_procedure;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = instance;
	// EXEに埋め込んだ共通IDから、システムの大・小アイコン寸法に合わせて読み込む。
	wcex.hIcon = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_APPLICATION_ICON),
		IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_SHARED));
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = APPLICATION_NAME;
	wcex.hIconSm = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_APPLICATION_ICON),
		IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED));
	RegisterClassExW(&wcex);

	const DWORD windowStyle_ = WS_OVERLAPPEDWINDOW;
	RECT windowRect_{ 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };
	// クライアント領域がSCREEN_WIDTH/HEIGHTになるよう、枠やタイトルバー分を含めた外形サイズへ補正する。
	AdjustWindowRect(&windowRect_, windowStyle_, FALSE);

	HWND hwnd = CreateWindowExW(0,
		APPLICATION_NAME, APPLICATION_NAME,
		windowStyle_ | WS_VISIBLE,
		CW_USEDEFAULT, CW_USEDEFAULT,
		windowRect_.right - windowRect_.left,
		windowRect_.bottom - windowRect_.top,
		NULL, NULL,
		instance, NULL);
	ShowWindow(hwnd, cmd_show);

	framework framework(hwnd);
	// Windowハンドル側にframeworkのthisポインタを保存し、以後のメッセージをインスタンスへ転送する。
	SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&framework));
	return framework.run();


}

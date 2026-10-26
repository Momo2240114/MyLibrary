#pragma once

// ビルドモードはDebug/Release/DevelopmentEditor/Shippingのどれか1つだけ有効にする。
#if defined(BS_BUILD_DEBUG)
#define BS_BUILD_DEBUG_COUNT 1
#else
#define BS_BUILD_DEBUG_COUNT 0
#endif

#if defined(BS_BUILD_RELEASE)
#define BS_BUILD_RELEASE_COUNT 1
#else
#define BS_BUILD_RELEASE_COUNT 0
#endif

#if defined(BS_BUILD_DEVELOPMENT_EDITOR)
#define BS_BUILD_DEVELOPMENT_EDITOR_COUNT 1
#else
#define BS_BUILD_DEVELOPMENT_EDITOR_COUNT 0
#endif

#if defined(BS_BUILD_SHIPPING)
#define BS_BUILD_SHIPPING_COUNT 1
#else
#define BS_BUILD_SHIPPING_COUNT 0
#endif

#if BS_BUILD_DEBUG_COUNT + \
    BS_BUILD_RELEASE_COUNT + \
    BS_BUILD_DEVELOPMENT_EDITOR_COUNT + \
    BS_BUILD_SHIPPING_COUNT != 1
#error Invalid build mode configuration.
#endif

#if defined(BS_BUILD_DEBUG)
// Debugはエディタ、ログ、Tracy、DebugDrawをすべて有効にして開発しやすくする。
#define BS_ENABLE_EDITOR 1
#define BS_ENABLE_IMGUI 1
#define BS_ENABLE_GAME_IMGUI_UI 1
#define BS_ENABLE_DEVELOPER_GUI 1
#if defined(BS_FORCE_ENABLE_TRACY)
#define BS_ENABLE_TRACY 1
#else
// 通常のメモリ計測ではTracyの収集バッファを作らず、必要な時だけBS_FORCE_ENABLE_TRACYで有効にする。
#define BS_ENABLE_TRACY 0
#endif
#define BS_ENABLE_PROFILING 1
#define BS_ENABLE_DEBUG_LOG 1
#define BS_ENABLE_WARNING_LOG 1
#define BS_ENABLE_ERROR_LOG 1
#define BS_ENABLE_DEBUG_DRAW 1
#define BS_ENABLE_ASSERT 1
#define BS_ENABLE_CONSOLE 1
#define BS_ENABLE_EDITOR_FILE_OUTPUT 1

#elif defined(BS_BUILD_DEVELOPMENT_EDITOR)
// DevelopmentEditorはエディタ機能を残しつつ、ログやProfilingを軽くした確認用。
#define BS_ENABLE_EDITOR 1
#define BS_ENABLE_IMGUI 1
#define BS_ENABLE_GAME_IMGUI_UI 1
#define BS_ENABLE_DEVELOPER_GUI 1
#if defined(BS_FORCE_ENABLE_TRACY)
#define BS_ENABLE_TRACY 1
#else
// DevelopmentEditorでも、計測が必要な時だけBS_FORCE_ENABLE_TRACYでTracyを有効にする。
#define BS_ENABLE_TRACY 0
#endif
#define BS_ENABLE_PROFILING 0
#define BS_ENABLE_DEBUG_LOG 0
#define BS_ENABLE_WARNING_LOG 1
#define BS_ENABLE_ERROR_LOG 1
#define BS_ENABLE_DEBUG_DRAW 0
#define BS_ENABLE_ASSERT 0
#define BS_ENABLE_CONSOLE 0
#define BS_ENABLE_EDITOR_FILE_OUTPUT 1

#elif defined(BS_BUILD_SHIPPING)
// Shippingは製品実行向け。Editor/Debug/Log出力を極力無効化する。
#define BS_ENABLE_EDITOR 0
#define BS_ENABLE_IMGUI 0
#define BS_ENABLE_GAME_IMGUI_UI 1
#define BS_ENABLE_DEVELOPER_GUI 0
#define BS_ENABLE_TRACY 0
#define BS_ENABLE_PROFILING 0
#define BS_ENABLE_DEBUG_LOG 0
#define BS_ENABLE_WARNING_LOG 0
#define BS_ENABLE_ERROR_LOG 0
#define BS_ENABLE_DEBUG_DRAW 0
#define BS_ENABLE_ASSERT 0
#define BS_ENABLE_CONSOLE 0
#define BS_ENABLE_EDITOR_FILE_OUTPUT 0

#elif defined(BS_BUILD_RELEASE)
// Releaseは最適化寄りだが、開発確認用にEditorやTracyを残す。
#define BS_ENABLE_EDITOR 1
#define BS_ENABLE_IMGUI 1
#define BS_ENABLE_GAME_IMGUI_UI 1
#define BS_ENABLE_DEVELOPER_GUI 1
#if defined(BS_FORCE_ENABLE_TRACY)
#define BS_ENABLE_TRACY 1
#else
// Release確認時も通常はTracyを止め、計測が必要なビルドだけ明示的に有効化する。
#define BS_ENABLE_TRACY 0
#endif
#define BS_ENABLE_PROFILING 1
#define BS_ENABLE_DEBUG_LOG 1
#define BS_ENABLE_WARNING_LOG 1
#define BS_ENABLE_ERROR_LOG 1
#define BS_ENABLE_DEBUG_DRAW 1
#define BS_ENABLE_ASSERT 0
#define BS_ENABLE_CONSOLE 1
#define BS_ENABLE_EDITOR_FILE_OUTPUT 1
#endif

// Game_Debugでもゲーム用UIを維持し、Editorと開発用の描画・保存だけを止める。
#if defined(BS_APP_GAME)
#undef BS_ENABLE_EDITOR
#undef BS_ENABLE_IMGUI
#undef BS_ENABLE_DEVELOPER_GUI
#undef BS_ENABLE_DEBUG_DRAW
#undef BS_ENABLE_EDITOR_FILE_OUTPUT
#define BS_ENABLE_EDITOR 0
#define BS_ENABLE_IMGUI 0
#define BS_ENABLE_DEVELOPER_GUI 0
#define BS_ENABLE_DEBUG_DRAW 0
#define BS_ENABLE_EDITOR_FILE_OUTPUT 0
#endif

#if BS_ENABLE_TRACY && !defined(TRACY_ON_DEMAND)
// Viewer未接続時は計測イベントを蓄積せず、通常起動中のメモリ増加を抑える。
#define TRACY_ON_DEMAND
#endif

#if BS_ENABLE_TRACY && !defined(TRACY_NO_SYSTEM_TRACING)
// OS側の自動トレースは通常起動時のメモリ増加要因になりやすいため、手動Zone計測だけを使う。
#define TRACY_NO_SYSTEM_TRACING
#endif

#if BS_ENABLE_TRACY && !defined(TRACY_NO_SAMPLING)
// サンプリングプロファイラは常時監視の負荷が大きいため、必要な箇所は明示的なZoneで計測する。
#define TRACY_NO_SAMPLING
#endif

#if BS_ENABLE_TRACY && !defined(TRACY_NO_CONTEXT_SWITCH)
// Context Switch収集を止め、ゲーム実行中に蓄積されるOSイベント量を抑える。
#define TRACY_NO_CONTEXT_SWITCH
#endif

#if BS_ENABLE_TRACY && !defined(TRACY_ENABLE)
// Tracyを使うビルドでは、Tracy側の有効マクロもここで揃える。
#define TRACY_ENABLE
#endif

#if !defined(BS_ENABLE_D3D11_DEBUG_LAYER)
// D3D11 Debug Layerは詳細な追跡を行うため、通常のメモリ計測時は無効にする。
#define BS_ENABLE_D3D11_DEBUG_LAYER 0
#endif

#if !defined(BS_ENABLE_PHYSX_PVD)
// PhysX Visual Debuggerは接触やScene Queryを蓄積・送信するため、通常起動では無効にする。
#define BS_ENABLE_PHYSX_PVD 0
#endif

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
#define BS_ENABLE_TRACY 1
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
#define BS_ENABLE_TRACY 0
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
#define BS_ENABLE_TRACY 1
#define BS_ENABLE_PROFILING 1
#define BS_ENABLE_DEBUG_LOG 1
#define BS_ENABLE_WARNING_LOG 1
#define BS_ENABLE_ERROR_LOG 1
#define BS_ENABLE_DEBUG_DRAW 1
#define BS_ENABLE_ASSERT 0
#define BS_ENABLE_CONSOLE 1
#define BS_ENABLE_EDITOR_FILE_OUTPUT 1
#endif

#if BS_ENABLE_TRACY && !defined(TRACY_ENABLE)
// Tracyを使うビルドでは、Tracy側の有効マクロもここで揃える。
#define TRACY_ENABLE
#endif

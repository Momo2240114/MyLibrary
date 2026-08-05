#pragma once

#include "PrecompiledHeaderConfig.h"

#if PCH_ENABLE_WINDOWS
// Windows.hの余計な定義とmin/maxマクロ衝突を抑える。
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#endif

#if PCH_ENABLE_STANDARD_LIBRARY
// 多くのcppで共通して使う標準ライブラリをPCHへまとめる。
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>
#endif

#if PCH_ENABLE_DIRECTX11
// D3D11/DirectXMath/WRLは描画・物理・UIで広く使うためPCH対象にする。
#include <d3d11.h>
#include <dxgi.h>
#include <DirectXMath.h>
#include <DirectXCollision.h>
#include <wrl/client.h>
#endif

#if PCH_ENABLE_IMGUI
// Editor/Debug GUIを使うビルド向け。
#include <imgui.h>
#endif

#if PCH_ENABLE_NLOHMANN_JSON
// Serialize/Deserializeで共通利用するJSONライブラリ。
#include <nlohmann/json.hpp>
#endif

#if PCH_ENABLE_CEREAL
// ModelResourceなどのバイナリ/キャッシュ保存で使うCereal。
#include <cereal/cereal.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#endif

#pragma once

#include "PrecompiledHeaderConfig.h"

#if PCH_ENABLE_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#endif

#if PCH_ENABLE_STANDARD_LIBRARY
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
#include <d3d11.h>
#include <dxgi.h>
#include <DirectXMath.h>
#include <DirectXCollision.h>
#include <wrl/client.h>
#endif

#if PCH_ENABLE_IMGUI
#include <imgui.h>
#endif

#if PCH_ENABLE_NLOHMANN_JSON
#include <nlohmann/json.hpp>
#endif

#if PCH_ENABLE_CEREAL
#include <cereal/cereal.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#endif

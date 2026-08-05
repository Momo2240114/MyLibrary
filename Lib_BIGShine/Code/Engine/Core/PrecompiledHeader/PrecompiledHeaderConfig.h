#pragma once

#include "Engine/Core/BuildConfig.h"

// PCHへ含める大分類を切り替える設定。
#define PCH_ENABLE_WINDOWS 1
#define PCH_ENABLE_STANDARD_LIBRARY 1
#define PCH_ENABLE_DIRECTX11 1
#define PCH_ENABLE_IMGUI 1
#define PCH_ENABLE_NLOHMANN_JSON 1
#define PCH_ENABLE_CEREAL 1

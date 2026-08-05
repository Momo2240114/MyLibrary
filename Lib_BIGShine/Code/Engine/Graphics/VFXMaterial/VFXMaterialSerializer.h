#pragma once

#include "VFXMaterialData.h"

#include <string>

class VFXMaterialSerializer
{
public:
    // JSONファイルからVFXMaterialDataを読み込む。
    static bool Load(
        const std::string& filePath_,
            VFXMaterialData& materialData_);

    // VFXMaterialDataをJSONファイルとして保存する。
    static bool Save(
        const std::string& filePath_,
        const VFXMaterialData& materialData_);
};

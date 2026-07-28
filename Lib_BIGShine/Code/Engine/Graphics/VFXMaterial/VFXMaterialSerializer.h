#pragma once

#include "VFXMaterialData.h"

#include <string>

class VFXMaterialSerializer
{
public:
    static bool Load(
        const std::string& filePath_,
        VFXMaterialData& materialData_);

    static bool Save(
        const std::string& filePath_,
        const VFXMaterialData& materialData_);
};

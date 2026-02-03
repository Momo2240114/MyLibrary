#pragma once
#include "CurveData.h"

#include <filesystem>

inline std::string GetCurveFolder()
{
    std::filesystem::path solutionDir = std::filesystem::current_path();
    solutionDir /= "Curves";

    if (!std::filesystem::exists(solutionDir))
        std::filesystem::create_directory(solutionDir);

    return solutionDir.string();
}

class CurveSerializer
{
public:
    static bool Save(const std::string& path, const CurveData& data);
    static bool Load(const std::string& path, CurveData& data);
};

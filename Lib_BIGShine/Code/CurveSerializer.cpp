#include "CurveSerializer.h"

#include <nlohmann/json.hpp>
#include <fstream>

bool CurveSerializer::Save(const std::string& path, const CurveData& data)
{
    using json = nlohmann::json;
    json j;

    j["name"] = data.name;
    j["interpolation"] = (int)data.interpolation;
    j["mode"] = (int)data.mode;

    if (data.mode == CurveEditorMode::FREE)
    {
        for (auto& k : data.keys)
        {
            j["keys"].push_back({
                {"time", k.time},
                {"value", k.value},
                {"in", k.inTangent},
                {"out", k.outTangent}
                });
        }
    }
    else if (data.mode == CurveEditorMode::BEZIER)
    {
        j["P"] = { data.P[0], data.P[1], data.P[2], data.P[3] };
    }

    std::ofstream file(path);
    if (!file) return false;
    file << j.dump(4);
    return true;
}

bool CurveSerializer::Load(const std::string& path, CurveData& data)
{
    using json = nlohmann::json;
    std::ifstream file(path);
    if (!file) return false;

    json j;
    file >> j;

    data.name = j["name"];
    data.interpolation = (CurveInterpolation)j["interpolation"];
    data.mode = (CurveEditorMode)j["mode"];
    data.keys.clear();

    if (data.mode == CurveEditorMode::FREE)
    {
        for (auto& k : j["keys"])
        {
            data.keys.push_back({
                k["time"],
                k["value"],
                k["in"],
                k["out"]
                });
        }
    }
    else if (data.mode == CurveEditorMode::BEZIER)
    {
        auto arr = j["P"];
        for (int i = 0; i < 4; i++)
            data.P[i] = arr[i];
    }

    return true;
}
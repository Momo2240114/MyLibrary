#pragma once

#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include "Vector.h"

class Serializer {
public:
    // --- 基本型のリテラル保存用ヘルパー ---
    static void WriteVector3(nlohmann::json& j, const Vector3& v) {
        j = { v.x, v.y, v.z };
    }
    static Vector3 ReadVector3(const nlohmann::json& j) {
        if (j.is_array() && j.size() == 3) {
            return Vector3(j[0], j[1], j[2]);
        }
        return Vector3(0, 0, 0);
    }

    // --- ファイル保存の実体 ---
    static bool SaveToFile(const std::string& sceneName, const nlohmann::json& data) {
        std::string dir = "Sin/Objects/";
        std::filesystem::create_directories(dir);
        std::ofstream file(dir + sceneName + ".json");
        if (file.is_open()) {
            file << data.dump(4);
            return true;
        }
        return false;
    }
};
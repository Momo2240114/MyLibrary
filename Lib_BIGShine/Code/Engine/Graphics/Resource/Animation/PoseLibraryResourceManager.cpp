#include "PrecompiledHeader.h"

#include "PoseLibraryResourceManager.h"
#include <fstream>
#include <iostream>

const std::string POSE_ASSET_DIR = "Data/AnimeJson/";

std::shared_ptr<PoseLibraryResource> PoseLibraryResourceManager::GetOrCreateResource(const std::string& modelName)
{
    // 1. すでにそのモデルのポーズ集が記憶に刻まれているか確認
    auto it = m_resources.find(modelName);
    if (it != m_resources.end()) {
        return it->second;
    }

    // 2. 記憶になければ、新たなリソースの器を錬成する
    auto newResource = std::make_shared<PoseLibraryResource>();
    newResource->modelName = modelName;

    // 3. 伝承の地（ファイル）に旧き記録が残されているか探る
    std::string filePath = POSE_ASSET_DIR + modelName + "/" + modelName + "_PoseLib.json";
    if (std::filesystem::exists(filePath)) {
        if (LoadFromJson(filePath, newResource)) {
            printf("記録からポーズライブラリを解読: %s\n", modelName.c_str());
        }
    }
    else {
        printf("ポーズライブラリの器が定義されました: %s\n", modelName.c_str());
    }

    // 4. 完成した結晶をリソースマップに書き留める
    m_resources[modelName] = newResource;
    return newResource;
}

bool PoseLibraryResourceManager::SaveToJson(const std::string& filename, const std::shared_ptr<PoseLibraryResource>& resource)
{
    if (!resource)
    {
        return false;
    }

    std::filesystem::path path(filename);
    std::filesystem::create_directories(path.parent_path()); // 途中の路（ディレクトリ）を拓く

    std::ofstream ostream(filename);
    if (!ostream.is_open())
    {
        return false;
    }

    nlohmann::json j;
    j["modelName"] = resource->modelName;
    j["poseLibrary"] = resource->poseLibrary;
    j["customAnims"] = resource->customAnims;

    ostream << j.dump(4); // 4マスのインデントで記す
    return true;
}

bool PoseLibraryResourceManager::LoadFromJson(const std::string& filename, std::shared_ptr<PoseLibraryResource>& resource)
{
    if (!resource)
    {
        return false;
    }

    std::ifstream istream(filename);
    if (!istream.is_open())
    {
        return false;
    }

    nlohmann::json j;
    istream >> j;

    resource->modelName = j.value("modelName", resource->modelName);

    if (j.contains("poseLibrary")) {
        resource->poseLibrary = j["poseLibrary"].get<std::vector<SavedPose>>();
    }
    if (j.contains("customAnims")) {
        resource->customAnims = j["customAnims"].get<std::vector<CustomAnimation>>();
    }

    return true;
}
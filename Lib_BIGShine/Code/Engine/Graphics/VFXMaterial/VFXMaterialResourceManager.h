#pragma once

#include "VFXMaterialResource.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

class VFXMaterialResourceManager
{
public:
    static VFXMaterialResourceManager& GetInstance();

    std::shared_ptr<VFXMaterialResource> Load(const std::string& filePath_);
    std::shared_ptr<VFXMaterialResource> Find(const std::string& filePath_) const;
    std::shared_ptr<VFXMaterialResource> GetDefaultMaterial();
    std::shared_ptr<VFXMaterialResource> CreateTemporary(const VFXMaterialData& materialData_);

    bool Reload(const std::string& filePath_);
    bool Save(
        const std::string& filePath_,
        const std::shared_ptr<VFXMaterialResource>& materialResource_);
    void RegisterOrReplace(
        const std::string& filePath_,
        const std::shared_ptr<VFXMaterialResource>& materialResource_);
    void Unload(const std::string& filePath_);
    void Clear();

    std::string NormalizePath(const std::string& filePath_) const;

private:
    void LogLoadFailureOnce(
        const std::string& filePath_,
        const char* reason_) const;

private:
    std::unordered_map<std::string, std::shared_ptr<VFXMaterialResource>> materialResources;
    mutable std::unordered_set<std::string> loggedLoadFailures;
    std::shared_ptr<VFXMaterialResource> defaultMaterial;
};

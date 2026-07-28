#include "PrecompiledHeader.h"

#include "VFXMaterialResourceManager.h"

#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "VFXMaterialSerializer.h"

#include <cstdio>
#include <filesystem>

VFXMaterialResourceManager& VFXMaterialResourceManager::GetInstance()
{
    static VFXMaterialResourceManager instance_;
    return instance_;
}

std::shared_ptr<VFXMaterialResource> VFXMaterialResourceManager::Load(
    const std::string& filePath_)
{
    BS_PROFILE_SCOPE("VFXMaterialResourceManager::Load");
    if (filePath_.empty()) return GetDefaultMaterial();

    const std::string key_ = NormalizePath(filePath_);
    if (auto found_ = Find(key_)) return found_;

    VFXMaterialData materialData_{};
    if (!VFXMaterialSerializer::Load(filePath_, materialData_))
    {
        LogLoadFailureOnce(filePath_, "serializer failed");
        return GetDefaultMaterial();
    }

    auto resource_ = std::make_shared<VFXMaterialResource>();
    resource_->SetSourceFilePath(key_);
    resource_->ApplyLoadedData(materialData_);
    resource_->LoadTextures(GraphicsDeviceManager::Instance().GetDevice());
    materialResources[key_] = resource_;
    return resource_;
}

std::shared_ptr<VFXMaterialResource> VFXMaterialResourceManager::Find(
    const std::string& filePath_) const
{
    const std::string key_ = NormalizePath(filePath_);
    const auto iterator_ = materialResources.find(key_);
    return iterator_ != materialResources.end() ? iterator_->second : nullptr;
}

std::shared_ptr<VFXMaterialResource> VFXMaterialResourceManager::GetDefaultMaterial()
{
    if (defaultMaterial) return defaultMaterial;

    VFXMaterialData materialData_{};
    materialData_.materialName = "DefaultVFXMaterial";
    defaultMaterial = std::make_shared<VFXMaterialResource>();
    defaultMaterial->SetSourceFilePath("__DefaultVFXMaterial__");
    defaultMaterial->ApplyLoadedData(materialData_);
    defaultMaterial->LoadTextures(GraphicsDeviceManager::Instance().GetDevice());
    return defaultMaterial;
}

std::shared_ptr<VFXMaterialResource> VFXMaterialResourceManager::CreateTemporary(
    const VFXMaterialData& materialData_)
{
    BS_PROFILE_SCOPE("VFXMaterialResourceManager::CreateTemporary");
    auto resource_ = std::make_shared<VFXMaterialResource>();
    resource_->SetSourceFilePath("__TemporaryVFXMaterial__");
    resource_->ApplyLoadedData(materialData_);
    resource_->LoadTextures(GraphicsDeviceManager::Instance().GetDevice());
    return resource_;
}

bool VFXMaterialResourceManager::Reload(const std::string& filePath_)
{
    BS_PROFILE_SCOPE("VFXMaterialResourceManager::Reload");
    if (filePath_.empty()) return false;
    const std::string key_ = NormalizePath(filePath_);
    auto resource_ = Find(key_);
    if (!resource_)
    {
        const auto loadedResource_ = Load(filePath_);
        return loadedResource_ && loadedResource_->GetSourceFilePath() == key_;
    }

    VFXMaterialData materialData_{};
    if (!VFXMaterialSerializer::Load(filePath_, materialData_))
    {
        LogLoadFailureOnce(filePath_, "reload failed");
        return false;
    }
    resource_->ApplyLoadedData(materialData_);
    resource_->LoadTextures(GraphicsDeviceManager::Instance().GetDevice());
    return true;
}

bool VFXMaterialResourceManager::Save(
    const std::string& filePath_,
    const std::shared_ptr<VFXMaterialResource>& materialResource_)
{
    BS_PROFILE_SCOPE("VFXMaterialResourceManager::Save");
    if (filePath_.empty() || !materialResource_) return false;
    if (!VFXMaterialSerializer::Save(filePath_, materialResource_->GetMaterialData())) return false;
    RegisterOrReplace(filePath_, materialResource_);
    materialResource_->SetSourceFilePath(NormalizePath(filePath_));
    return true;
}

void VFXMaterialResourceManager::RegisterOrReplace(
    const std::string& filePath_,
    const std::shared_ptr<VFXMaterialResource>& materialResource_)
{
    if (filePath_.empty() || !materialResource_) return;
    materialResources[NormalizePath(filePath_)] = materialResource_;
}

void VFXMaterialResourceManager::Unload(const std::string& filePath_)
{
    if (filePath_.empty()) return;
    materialResources.erase(NormalizePath(filePath_));
}

void VFXMaterialResourceManager::Clear()
{
    materialResources.clear();
    loggedLoadFailures.clear();
    defaultMaterial.reset();
}

std::string VFXMaterialResourceManager::NormalizePath(const std::string& filePath_) const
{
    if (filePath_.empty()) return {};
    std::filesystem::path path_(filePath_);
    std::error_code errorCode_;
    if (std::filesystem::exists(path_, errorCode_))
    {
        path_ = std::filesystem::weakly_canonical(path_, errorCode_);
    }
    if (errorCode_) path_ = std::filesystem::path(filePath_);
    return path_.lexically_normal().generic_string();
}

void VFXMaterialResourceManager::LogLoadFailureOnce(
    const std::string& filePath_,
    const char* reason_) const
{
    const std::string key_ = NormalizePath(filePath_) + ":" + reason_;
    if (!loggedLoadFailures.insert(key_).second) return;
    std::printf(
        "[VFXMaterialResourceManager] Failed to load material '%s' (%s). Using fallback material.\n",
        filePath_.c_str(),
        reason_);
}

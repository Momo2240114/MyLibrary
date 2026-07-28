#include "PrecompiledHeader.h"

#include "VFXMaterialSerializer.h"

#include "Engine/Debug/TracyProfiler.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace
{
    constexpr uint32_t VFX_MATERIAL_VERSION = 1u;

    nlohmann::json WriteVector2(const DirectX::XMFLOAT2& value_)
    {
        return { value_.x, value_.y };
    }

    nlohmann::json WriteVector3(const DirectX::XMFLOAT3& value_)
    {
        return { value_.x, value_.y, value_.z };
    }

    nlohmann::json WriteVector4(const DirectX::XMFLOAT4& value_)
    {
        return { value_.x, value_.y, value_.z, value_.w };
    }

    DirectX::XMFLOAT2 ReadVector2(
        const nlohmann::json& json_,
        const char* key_,
        const DirectX::XMFLOAT2& fallback_)
    {
        const nlohmann::json value_ = json_.value(key_, nlohmann::json::array());
        if (!value_.is_array() || value_.size() < 2u) return fallback_;
        return { value_[0].get<float>(), value_[1].get<float>() };
    }

    DirectX::XMFLOAT3 ReadVector3(
        const nlohmann::json& json_,
        const char* key_,
        const DirectX::XMFLOAT3& fallback_)
    {
        const nlohmann::json value_ = json_.value(key_, nlohmann::json::array());
        if (!value_.is_array() || value_.size() < 3u) return fallback_;
        return { value_[0].get<float>(), value_[1].get<float>(), value_[2].get<float>() };
    }

    DirectX::XMFLOAT4 ReadVector4(
        const nlohmann::json& json_,
        const char* key_,
        const DirectX::XMFLOAT4& fallback_)
    {
        const nlohmann::json value_ = json_.value(key_, nlohmann::json::array());
        if (!value_.is_array() || value_.size() < 4u) return fallback_;
        return {
            value_[0].get<float>(),
            value_[1].get<float>(),
            value_[2].get<float>(),
            value_[3].get<float>() };
    }

    const char* ToString(VFX_MATERIAL_BLEND_MODE mode_)
    {
        return mode_ == VFX_MATERIAL_BLEND_MODE::ADDITIVE ? "ADDITIVE" : "ALPHA";
    }

    const char* ToString(VFX_MATERIAL_CULL_MODE mode_)
    {
        switch (mode_)
        {
        case VFX_MATERIAL_CULL_MODE::BACK: return "BACK";
        case VFX_MATERIAL_CULL_MODE::FRONT: return "FRONT";
        default: return "NONE";
        }
    }

    VFX_MATERIAL_BLEND_MODE ParseBlendMode(const nlohmann::json& json_)
    {
        if (json_.is_number_integer())
        {
            return static_cast<VFX_MATERIAL_BLEND_MODE>(std::clamp(json_.get<int>(), 0, 1));
        }
        const std::string value_ = json_.is_string() ? json_.get<std::string>() : "ALPHA";
        return value_ == "ADDITIVE" ? VFX_MATERIAL_BLEND_MODE::ADDITIVE : VFX_MATERIAL_BLEND_MODE::ALPHA;
    }

    VFX_MATERIAL_CULL_MODE ParseCullMode(const nlohmann::json& json_)
    {
        if (json_.is_number_integer())
        {
            return static_cast<VFX_MATERIAL_CULL_MODE>(std::clamp(json_.get<int>(), 0, 2));
        }
        const std::string value_ = json_.is_string() ? json_.get<std::string>() : "NONE";
        if (value_ == "BACK") return VFX_MATERIAL_CULL_MODE::BACK;
        if (value_ == "FRONT") return VFX_MATERIAL_CULL_MODE::FRONT;
        return VFX_MATERIAL_CULL_MODE::NONE;
    }

    nlohmann::json WriteUv(const VFXMaterialUVData& uv_)
    {
        return {
            { "Scale", WriteVector2(uv_.scale) },
            { "Offset", WriteVector2(uv_.offset) },
            { "ScrollSpeed", WriteVector2(uv_.scrollSpeed) },
            { "RotationSpeed", uv_.rotationSpeed },
            { "Pivot", WriteVector2(uv_.pivot) },
        };
    }

    void ReadUv(
        const nlohmann::json& json_,
        VFXMaterialUVData& uv_)
    {
        uv_.scale = ReadVector2(json_, "Scale", uv_.scale);
        uv_.offset = ReadVector2(json_, "Offset", uv_.offset);
        uv_.scrollSpeed = ReadVector2(json_, "ScrollSpeed", uv_.scrollSpeed);
        uv_.rotationSpeed = json_.value("RotationSpeed", uv_.rotationSpeed);
        uv_.pivot = ReadVector2(json_, "Pivot", uv_.pivot);
    }
}

bool VFXMaterialSerializer::Load(
    const std::string& filePath_,
    VFXMaterialData& materialData_)
{
    BS_PROFILE_SCOPE("VFXMaterialSerializer::Load");
    if (filePath_.empty()) return false;
    std::ifstream file_(filePath_);
    if (!file_.is_open()) return false;

    try
    {
        nlohmann::json json_;
        file_ >> json_;
        materialData_.version = json_.value("Version", json_.value("version", VFX_MATERIAL_VERSION));
        materialData_.materialName = json_.value(
            "MaterialName",
            json_.value("materialName", materialData_.materialName));

        const nlohmann::json textures_ = json_.value("Textures", json_.value("textures", nlohmann::json::object()));
        materialData_.baseTexturePath = textures_.value("BaseTexturePath", textures_.value("baseTexturePath", materialData_.baseTexturePath));
        materialData_.maskTexturePath = textures_.value("MaskTexturePath", textures_.value("maskTexturePath", materialData_.maskTexturePath));
        materialData_.noiseTexturePath = textures_.value("NoiseTexturePath", textures_.value("noiseTexturePath", materialData_.noiseTexturePath));
        materialData_.normalTexturePath = textures_.value("NormalTexturePath", textures_.value("normalTexturePath", materialData_.normalTexturePath));
        materialData_.distortionTexturePath = textures_.value("DistortionTexturePath", textures_.value("distortionTexturePath", materialData_.distortionTexturePath));

        const nlohmann::json color_ = json_.value("Color", json_.value("color", nlohmann::json::object()));
        materialData_.baseColor = ReadVector4(color_, "BaseColor", ReadVector4(color_, "baseColor", materialData_.baseColor));
        materialData_.emissiveColor = ReadVector3(color_, "EmissiveColor", ReadVector3(color_, "emissiveColor", materialData_.emissiveColor));
        materialData_.emissiveIntensity = color_.value("EmissiveIntensity", color_.value("emissiveIntensity", materialData_.emissiveIntensity));
        materialData_.alpha = color_.value("Alpha", color_.value("alpha", materialData_.alpha));

        if (json_.contains("MainUV")) ReadUv(json_["MainUV"], materialData_.mainUV);
        else if (json_.contains("mainUV")) ReadUv(json_["mainUV"], materialData_.mainUV);
        if (json_.contains("NoiseUV")) ReadUv(json_["NoiseUV"], materialData_.noiseUV);
        else if (json_.contains("noiseUV")) ReadUv(json_["noiseUV"], materialData_.noiseUV);

        const nlohmann::json dissolve_ = json_.value("Dissolve", json_.value("dissolve", nlohmann::json::object()));
        materialData_.dissolve.enabled = dissolve_.value("Enabled", dissolve_.value("enabled", materialData_.dissolve.enabled));
        materialData_.dissolve.threshold = dissolve_.value("Threshold", dissolve_.value("threshold", materialData_.dissolve.threshold));
        materialData_.dissolve.edgeWidth = dissolve_.value("EdgeWidth", dissolve_.value("edgeWidth", materialData_.dissolve.edgeWidth));
        materialData_.dissolve.edgeColor = ReadVector4(dissolve_, "EdgeColor", ReadVector4(dissolve_, "edgeColor", materialData_.dissolve.edgeColor));

        const nlohmann::json distortion_ = json_.value("Distortion", json_.value("distortion", nlohmann::json::object()));
        materialData_.distortion.enabled = distortion_.value("Enabled", distortion_.value("enabled", materialData_.distortion.enabled));
        materialData_.distortion.strength = distortion_.value("Strength", distortion_.value("strength", materialData_.distortion.strength));

        const nlohmann::json renderState_ = json_.value("RenderState", json_.value("renderState", nlohmann::json::object()));
        const nlohmann::json blendMode_ = renderState_.contains("BlendMode")
            ? renderState_["BlendMode"]
            : renderState_.value("blendMode", nlohmann::json{ "ALPHA" });
        const nlohmann::json cullMode_ = renderState_.contains("CullMode")
            ? renderState_["CullMode"]
            : renderState_.value("cullMode", nlohmann::json{ "NONE" });
        materialData_.renderState.blendMode = ParseBlendMode(blendMode_);
        materialData_.renderState.cullMode = ParseCullMode(cullMode_);
        materialData_.renderState.depthTest = renderState_.value("DepthTest", renderState_.value("depthTest", materialData_.renderState.depthTest));
        materialData_.renderState.depthWrite = renderState_.value("DepthWrite", renderState_.value("depthWrite", materialData_.renderState.depthWrite));
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

bool VFXMaterialSerializer::Save(
    const std::string& filePath_,
    const VFXMaterialData& materialData_)
{
    BS_PROFILE_SCOPE("VFXMaterialSerializer::Save");
    if (filePath_.empty()) return false;
    const std::filesystem::path outputPath_(filePath_);
    if (outputPath_.has_parent_path())
    {
        std::filesystem::create_directories(outputPath_.parent_path());
    }

    nlohmann::json json_;
    json_["Version"] = VFX_MATERIAL_VERSION;
    json_["AssetType"] = "VFXMaterial";
    json_["MaterialName"] = materialData_.materialName;
    json_["Textures"] =
    {
        { "BaseTexturePath", materialData_.baseTexturePath },
        { "MaskTexturePath", materialData_.maskTexturePath },
        { "NoiseTexturePath", materialData_.noiseTexturePath },
        { "NormalTexturePath", materialData_.normalTexturePath },
        { "DistortionTexturePath", materialData_.distortionTexturePath },
    };
    json_["Color"] =
    {
        { "BaseColor", WriteVector4(materialData_.baseColor) },
        { "EmissiveColor", WriteVector3(materialData_.emissiveColor) },
        { "EmissiveIntensity", materialData_.emissiveIntensity },
        { "Alpha", materialData_.alpha },
    };
    json_["MainUV"] = WriteUv(materialData_.mainUV);
    json_["NoiseUV"] = WriteUv(materialData_.noiseUV);
    json_["Dissolve"] =
    {
        { "Enabled", materialData_.dissolve.enabled },
        { "Threshold", materialData_.dissolve.threshold },
        { "EdgeWidth", materialData_.dissolve.edgeWidth },
        { "EdgeColor", WriteVector4(materialData_.dissolve.edgeColor) },
    };
    json_["Distortion"] =
    {
        { "Enabled", materialData_.distortion.enabled },
        { "Strength", materialData_.distortion.strength },
    };
    json_["RenderState"] =
    {
        { "BlendMode", ToString(materialData_.renderState.blendMode) },
        { "CullMode", ToString(materialData_.renderState.cullMode) },
        { "DepthTest", materialData_.renderState.depthTest },
        { "DepthWrite", materialData_.renderState.depthWrite },
    };

    std::ofstream file_(outputPath_, std::ios::out | std::ios::trunc);
    if (!file_.is_open()) return false;
    file_ << json_.dump(4);
    return file_.good();
}

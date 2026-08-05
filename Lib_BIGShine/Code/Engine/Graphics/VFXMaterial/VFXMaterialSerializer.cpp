#include "PrecompiledHeader.h"

#include "VFXMaterialSerializer.h"

#include "Engine/Debug/TracyProfiler.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace
{
    // VFXMaterial JSONの保存形式バージョン。
    constexpr uint32_t VFX_MATERIAL_VERSION = 1u;

    nlohmann::json WriteVector2(const DirectX::XMFLOAT2& value_)
    {
        // DirectX型はJSONへ直接書けないため、数値配列へ分解する。
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
        // 指定キーが無い/配列長が足りない場合は既存値を維持する。
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
        // JSONはEditorで読みやすいよう、enum値ではなく文字列で保存する。
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

    const char* ToString(VFX_MATERIAL_COLOR_RAMP_SAMPLE_SOURCE source_)
    {
        switch (source_)
        {
        case VFX_MATERIAL_COLOR_RAMP_SAMPLE_SOURCE::TIME: return "TIME";
        case VFX_MATERIAL_COLOR_RAMP_SAMPLE_SOURCE::FIXED: return "FIXED";
        default: return "LIFE";
        }
    }

    const char* ToString(VFX_MATERIAL_COLOR_RAMP_BLEND_MODE mode_)
    {
        switch (mode_)
        {
        case VFX_MATERIAL_COLOR_RAMP_BLEND_MODE::OVERRIDE: return "OVERRIDE";
        case VFX_MATERIAL_COLOR_RAMP_BLEND_MODE::ADD: return "ADD";
        default: return "MULTIPLY";
        }
    }

    const char* ToString(VFX_MATERIAL_COLOR_RAMP_INTERPOLATION interpolation_)
    {
        switch (interpolation_)
        {
        case VFX_MATERIAL_COLOR_RAMP_INTERPOLATION::CONSTANT: return "CONSTANT";
        case VFX_MATERIAL_COLOR_RAMP_INTERPOLATION::SMOOTH: return "SMOOTH";
        default: return "LINEAR";
        }
    }

    const char* ToString(VFX_MATERIAL_COLOR_RAMP_WRAP_MODE mode_)
    {
        switch (mode_)
        {
        case VFX_MATERIAL_COLOR_RAMP_WRAP_MODE::REPEAT: return "REPEAT";
        case VFX_MATERIAL_COLOR_RAMP_WRAP_MODE::MIRROR: return "MIRROR";
        default: return "CLAMP";
        }
    }

    const char* ToString(VFX_MATERIAL_RANDOM_COLOR_MODE mode_)
    {
        switch (mode_)
        {
        case VFX_MATERIAL_RANDOM_COLOR_MODE::OVERRIDE: return "OVERRIDE";
        case VFX_MATERIAL_RANDOM_COLOR_MODE::ADD: return "ADD";
        default: return "MULTIPLY";
        }
    }

    VFX_MATERIAL_BLEND_MODE ParseBlendMode(const nlohmann::json& json_)
    {
        // 旧データ互換のため、整数enumと文字列の両方を受け付ける。
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

    VFX_MATERIAL_COLOR_RAMP_SAMPLE_SOURCE ParseColorRampSampleSource(const nlohmann::json& json_)
    {
        if (json_.is_number_integer())
        {
            return static_cast<VFX_MATERIAL_COLOR_RAMP_SAMPLE_SOURCE>(std::clamp(json_.get<int>(), 0, 2));
        }
        const std::string value_ = json_.is_string() ? json_.get<std::string>() : "LIFE";
        if (value_ == "TIME") return VFX_MATERIAL_COLOR_RAMP_SAMPLE_SOURCE::TIME;
        if (value_ == "FIXED") return VFX_MATERIAL_COLOR_RAMP_SAMPLE_SOURCE::FIXED;
        return VFX_MATERIAL_COLOR_RAMP_SAMPLE_SOURCE::LIFE;
    }

    VFX_MATERIAL_COLOR_RAMP_BLEND_MODE ParseColorRampBlendMode(const nlohmann::json& json_)
    {
        if (json_.is_number_integer())
        {
            return static_cast<VFX_MATERIAL_COLOR_RAMP_BLEND_MODE>(std::clamp(json_.get<int>(), 0, 2));
        }
        const std::string value_ = json_.is_string() ? json_.get<std::string>() : "MULTIPLY";
        if (value_ == "OVERRIDE") return VFX_MATERIAL_COLOR_RAMP_BLEND_MODE::OVERRIDE;
        if (value_ == "ADD") return VFX_MATERIAL_COLOR_RAMP_BLEND_MODE::ADD;
        return VFX_MATERIAL_COLOR_RAMP_BLEND_MODE::MULTIPLY;
    }

    VFX_MATERIAL_COLOR_RAMP_INTERPOLATION ParseColorRampInterpolation(const nlohmann::json& json_)
    {
        if (json_.is_number_integer())
        {
            return static_cast<VFX_MATERIAL_COLOR_RAMP_INTERPOLATION>(std::clamp(json_.get<int>(), 0, 2));
        }
        const std::string value_ = json_.is_string() ? json_.get<std::string>() : "LINEAR";
        if (value_ == "CONSTANT") return VFX_MATERIAL_COLOR_RAMP_INTERPOLATION::CONSTANT;
        if (value_ == "SMOOTH") return VFX_MATERIAL_COLOR_RAMP_INTERPOLATION::SMOOTH;
        return VFX_MATERIAL_COLOR_RAMP_INTERPOLATION::LINEAR;
    }

    VFX_MATERIAL_COLOR_RAMP_WRAP_MODE ParseColorRampWrapMode(const nlohmann::json& json_)
    {
        if (json_.is_number_integer())
        {
            return static_cast<VFX_MATERIAL_COLOR_RAMP_WRAP_MODE>(std::clamp(json_.get<int>(), 0, 2));
        }
        const std::string value_ = json_.is_string() ? json_.get<std::string>() : "CLAMP";
        if (value_ == "REPEAT") return VFX_MATERIAL_COLOR_RAMP_WRAP_MODE::REPEAT;
        if (value_ == "MIRROR") return VFX_MATERIAL_COLOR_RAMP_WRAP_MODE::MIRROR;
        return VFX_MATERIAL_COLOR_RAMP_WRAP_MODE::CLAMP;
    }

    VFX_MATERIAL_RANDOM_COLOR_MODE ParseRandomColorMode(const nlohmann::json& json_)
    {
        if (json_.is_number_integer())
        {
            return static_cast<VFX_MATERIAL_RANDOM_COLOR_MODE>(std::clamp(json_.get<int>(), 0, 2));
        }
        const std::string value_ = json_.is_string() ? json_.get<std::string>() : "MULTIPLY";
        if (value_ == "OVERRIDE") return VFX_MATERIAL_RANDOM_COLOR_MODE::OVERRIDE;
        if (value_ == "ADD") return VFX_MATERIAL_RANDOM_COLOR_MODE::ADD;
        return VFX_MATERIAL_RANDOM_COLOR_MODE::MULTIPLY;
    }

    nlohmann::json WriteUv(const VFXMaterialUVData& uv_)
    {
        // MainUV/NoiseUVで同じ構造を使うため、共通関数で書き出す。
        return {
            { "Scale", WriteVector2(uv_.scale) },
            { "Offset", WriteVector2(uv_.offset) },
            { "ScrollSpeed", WriteVector2(uv_.scrollSpeed) },
            { "Rotation", uv_.rotation },
            { "RotationSpeed", uv_.rotationSpeed },
            { "Pivot", WriteVector2(uv_.pivot) },
        };
    }

    void ReadUv(
        const nlohmann::json& json_,
        VFXMaterialUVData& uv_)
    {
        // 不足キーは現在のデフォルト値を残し、古いMaterialでも読めるようにする。
        uv_.scale = ReadVector2(json_, "Scale", uv_.scale);
        uv_.offset = ReadVector2(json_, "Offset", uv_.offset);
        uv_.scrollSpeed = ReadVector2(json_, "ScrollSpeed", uv_.scrollSpeed);
        uv_.rotation = json_.value("Rotation", uv_.rotation);
        uv_.rotationSpeed = json_.value("RotationSpeed", uv_.rotationSpeed);
        uv_.pivot = ReadVector2(json_, "Pivot", uv_.pivot);
    }

    nlohmann::json WriteColorRamp(const VFXMaterialColorRampData& colorRamp_)
    {
        // Ramp Stopは任意数なので配列として保存する。
        nlohmann::json stops_ = nlohmann::json::array();
        for (const VFXMaterialColorRampStopData& stop_ : colorRamp_.stops)
        {
            stops_.push_back(
                {
                    { "Position", stop_.position },
                    { "Color", WriteVector4(stop_.color) },
                });
        }

        return
        {
            { "Enabled", colorRamp_.enabled },
            { "SampleSource", ToString(colorRamp_.sampleSource) },
            { "BlendMode", ToString(colorRamp_.blendMode) },
            { "Interpolation", ToString(colorRamp_.interpolation) },
            { "WrapMode", ToString(colorRamp_.wrapMode) },
            { "FixedPosition", colorRamp_.fixedPosition },
            { "ScrollSpeed", colorRamp_.scrollSpeed },
            { "Influence", colorRamp_.influence },
            { "AffectAlpha", colorRamp_.affectAlpha },
            { "Stops", stops_ },
        };
    }

    void ReadColorRamp(
        const nlohmann::json& json_,
        VFXMaterialColorRampData& colorRamp_)
    {
        // 大文字キーと小文字キーの両方を読み、過去の保存形式と互換を取る。
        colorRamp_.enabled = json_.value("Enabled", json_.value("enabled", colorRamp_.enabled));
        const nlohmann::json sampleSource_ = json_.contains("SampleSource")
            ? json_["SampleSource"]
            : json_.value("sampleSource", nlohmann::json{ "LIFE" });
        const nlohmann::json blendMode_ = json_.contains("BlendMode")
            ? json_["BlendMode"]
            : json_.value("blendMode", nlohmann::json{ "MULTIPLY" });
        const nlohmann::json interpolation_ = json_.contains("Interpolation")
            ? json_["Interpolation"]
            : json_.value("interpolation", nlohmann::json{ "LINEAR" });
        const nlohmann::json wrapMode_ = json_.contains("WrapMode")
            ? json_["WrapMode"]
            : json_.value("wrapMode", nlohmann::json{ "CLAMP" });
        colorRamp_.sampleSource = ParseColorRampSampleSource(sampleSource_);
        colorRamp_.blendMode = ParseColorRampBlendMode(blendMode_);
        colorRamp_.interpolation = ParseColorRampInterpolation(interpolation_);
        colorRamp_.wrapMode = ParseColorRampWrapMode(wrapMode_);
        colorRamp_.fixedPosition = json_.value("FixedPosition", json_.value("fixedPosition", colorRamp_.fixedPosition));
        colorRamp_.scrollSpeed = json_.value("ScrollSpeed", json_.value("scrollSpeed", colorRamp_.scrollSpeed));
        colorRamp_.influence = json_.value("Influence", json_.value("influence", colorRamp_.influence));
        colorRamp_.affectAlpha = json_.value("AffectAlpha", json_.value("affectAlpha", colorRamp_.affectAlpha));

        colorRamp_.stops.clear();
        const nlohmann::json stops_ = json_.value("Stops", json_.value("stops", nlohmann::json::array()));
        if (stops_.is_array())
        {
            for (const nlohmann::json& stop_ : stops_)
            {
                VFXMaterialColorRampStopData stopData_{};
                stopData_.position = std::clamp(stop_.value("Position", stop_.value("position", 0.0f)), 0.0f, 1.0f);
                stopData_.color = ReadVector4(stop_, "Color", ReadVector4(stop_, "color", stopData_.color));
                colorRamp_.stops.push_back(stopData_);
            }
        }

        if (colorRamp_.stops.empty() && json_.contains("StartColor") && json_.contains("EndColor"))
        {
            // 古いStart/EndだけのRamp形式を、2 StopのRampへ変換する。
            colorRamp_.stops.push_back({ 0.0f, ReadVector4(json_, "StartColor", { 1.0f, 1.0f, 1.0f, 1.0f }) });
            colorRamp_.stops.push_back({ 1.0f, ReadVector4(json_, "EndColor", { 1.0f, 1.0f, 1.0f, 1.0f }) });
        }
        if (colorRamp_.stops.empty())
        {
            // Stopが無いとサンプリングできないため、白Rampを最低限用意する。
            colorRamp_.stops.push_back({ 0.0f, { 1.0f, 1.0f, 1.0f, 1.0f } });
            colorRamp_.stops.push_back({ 1.0f, { 1.0f, 1.0f, 1.0f, 1.0f } });
        }
    }

    nlohmann::json WriteRandomColor(const VFXMaterialRandomColorData& randomColor_)
    {
        return
        {
            { "Enabled", randomColor_.enabled },
            { "Mode", ToString(randomColor_.mode) },
            { "MinColor", WriteVector4(randomColor_.minColor) },
            { "MaxColor", WriteVector4(randomColor_.maxColor) },
            { "Influence", randomColor_.influence },
            { "AffectAlpha", randomColor_.affectAlpha },
            { "Seed", randomColor_.seed },
        };
    }

    void ReadRandomColor(
        const nlohmann::json& json_,
        VFXMaterialRandomColorData& randomColor_)
    {
        // RandomColorも大文字/小文字キーと整数/文字列enumの両方を許容する。
        randomColor_.enabled = json_.value("Enabled", json_.value("enabled", randomColor_.enabled));
        const nlohmann::json mode_ = json_.contains("Mode")
            ? json_["Mode"]
            : json_.value("mode", nlohmann::json{ "MULTIPLY" });
        randomColor_.mode = ParseRandomColorMode(mode_);
        randomColor_.minColor = ReadVector4(json_, "MinColor", ReadVector4(json_, "minColor", randomColor_.minColor));
        randomColor_.maxColor = ReadVector4(json_, "MaxColor", ReadVector4(json_, "maxColor", randomColor_.maxColor));
        randomColor_.influence = json_.value("Influence", json_.value("influence", randomColor_.influence));
        randomColor_.affectAlpha = json_.value("AffectAlpha", json_.value("affectAlpha", randomColor_.affectAlpha));
        randomColor_.seed = json_.value("Seed", json_.value("seed", randomColor_.seed));
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
        // ここではJSON構造だけを読み、TextureロードはVFXMaterialResource側に任せる。
        nlohmann::json json_;
        file_ >> json_;
        materialData_.version = json_.value("Version", json_.value("version", VFX_MATERIAL_VERSION));
        materialData_.materialName = json_.value(
            "MaterialName",
            json_.value("materialName", materialData_.materialName));

        const nlohmann::json textures_ = json_.value("Textures", json_.value("textures", nlohmann::json::object()));
        // Textureパスは空でも有効。描画側でDummy Textureへフォールバックする。
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

        if (json_.contains("ColorRamp")) ReadColorRamp(json_["ColorRamp"], materialData_.colorRamp);
        else if (json_.contains("colorRamp")) ReadColorRamp(json_["colorRamp"], materialData_.colorRamp);
        if (json_.contains("RandomColor")) ReadRandomColor(json_["RandomColor"], materialData_.randomColor);
        else if (json_.contains("randomColor")) ReadRandomColor(json_["randomColor"], materialData_.randomColor);

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
        // 壊れたJSONや型違いのデータは読み込み失敗として扱う。
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
        // Editorから新規Asset保存する時、フォルダが無ければ作る。
        std::filesystem::create_directories(outputPath_.parent_path());
    }

    nlohmann::json json_;
    // 保存は現行形式の大文字キーへ統一し、読み込み側で旧形式を吸収する。
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
    json_["ColorRamp"] = WriteColorRamp(materialData_.colorRamp);
    json_["RandomColor"] = WriteRandomColor(materialData_.randomColor);
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

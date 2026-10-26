#include "PrecompiledHeader.h"
#include "UIPresentationPresets.h"
#include "Engine/Debug/TracyProfiler.h"
#include <imgui.h>
#include <imgui_stdlib.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <map>
#include "Editor/AssetSlotGui.h"
#include "Engine/Graphics/Core/CPUResourceLoader.h"
#include "Engine/Platform/WindowsDialogHelper.h"
nlohmann::json UIPresentationPresets::Load(const std::string& path_)
{
    BS_PROFILE_SCOPE("UIPresentationPresets::Load");
    std::ifstream file_(path_,std::ios::binary);
    if (!file_) return nlohmann::json::object();
    auto json_ = nlohmann::json::parse(file_,nullptr,false);
    return json_.is_object() ? json_ : nlohmann::json::object();
}
bool UIPresentationPresets::Save(const std::string& path_,const nlohmann::json& catalog_)
{
    BS_PROFILE_SCOPE("UIPresentationPresets::Save");
    std::ofstream file_(path_,std::ios::binary);
    if (!file_) return false;
    const auto text_ = catalog_.dump(4);
    for (const char character_ : text_) { if (character_ == '\n') file_.put('\r'); file_.put(character_); }
    file_ << "\r\n"; return !!file_;
}
float UIPresentationPresets::Number(const nlohmann::json& json_,const char* key_,float fallback_,float minimum_,float maximum_)
{
    if (!json_.is_object() || !json_.contains(key_) || !json_[key_].is_number()) return fallback_;
    const float value_ = json_[key_].get<float>();
    return std::isfinite(value_) ? std::clamp(value_,minimum_,maximum_) : fallback_;
}
bool UIPresentationPresets::Boolean(const nlohmann::json& json_,const char* key_,bool fallback_)
{
    return json_.is_object() && json_.contains(key_) && json_[key_].is_boolean() ? json_[key_].get<bool>() : fallback_;
}
std::string UIPresentationPresets::String(const nlohmann::json& json_,const char* key_,const std::string& fallback_)
{
    return json_.is_object() && json_.contains(key_) && json_[key_].is_string() ? json_[key_].get<std::string>() : fallback_;
}
DirectX::XMFLOAT2 UIPresentationPresets::Vector2(const nlohmann::json& json_,const char* key_,DirectX::XMFLOAT2 fallback_,float minimum_,float maximum_)
{
    if (!json_.is_object() || !json_.contains(key_) || !json_[key_].is_array() || json_[key_].size() != 2) return fallback_;
    float* values_ = &fallback_.x;
    for (int index_ = 0; index_ < 2; ++index_)
        if (json_[key_][index_].is_number()) { const float value_ = json_[key_][index_].get<float>(); if (std::isfinite(value_)) values_[index_] = std::clamp(value_,minimum_,maximum_); }
    return fallback_;
}
DirectX::XMFLOAT4 UIPresentationPresets::Color(const nlohmann::json& json_,const char* key_,DirectX::XMFLOAT4 fallback_)
{
    if (!json_.is_object() || !json_.contains(key_) || !json_[key_].is_array() || json_[key_].size() != 4) return fallback_;
    float* values_ = &fallback_.x;
    for (int index_ = 0; index_ < 4; ++index_)
        if (json_[key_][index_].is_number()) { const float value_ = json_[key_][index_].get<float>(); if (std::isfinite(value_)) values_[index_] = std::clamp(value_,0.0f,1.0f); }
    return fallback_;
}
bool UIPresentationPresets::DrawPicker(const nlohmann::json& catalog_,std::string& name_)
{
    bool changed_ = false;
    std::vector<std::string> names_;
    for (const auto& entry_ : catalog_.items()) if (entry_.value().is_object()) names_.push_back(entry_.key());
    if (ImGui::BeginCombo("Preset",name_.c_str()))
    {
        for (const auto& candidate_ : names_) if (ImGui::Selectable(candidate_.c_str(),candidate_ == name_)) { name_ = candidate_; changed_ = true; }
        ImGui::EndCombo();
    }
    const auto found_ = std::find(names_.begin(),names_.end(),name_);
    int index_ = found_ == names_.end() ? -1 : static_cast<int>(found_ - names_.begin());
    int step_ = 0;
    if (ImGui::Button("Previous Preset")) step_ = -1;
    ImGui::SameLine(); if (ImGui::Button("Next Preset")) step_ = 1;
    if (step_ && !names_.empty())
    {
        index_ = (index_ < 0 ? 0 : index_ + step_ + static_cast<int>(names_.size())) % static_cast<int>(names_.size());
        name_ = names_[index_]; changed_ = true;
    }
    return changed_;
}

namespace
{
    std::map<std::string,Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> texturePreviews;
    ID3D11Device* previewDevice = nullptr;
}
bool UIPresentationPresets::LoadTexture(const std::string& path_,Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& output_)
{
    BS_PROFILE_SCOPE("UIPresentationPresets::LoadTexture");
    output_.Reset();
    const auto device_ = GraphicsDeviceManager::Instance().GetDevice();
    std::error_code error_;
    const auto nativePath_ = WindowsDialogHelper::Utf8ToPath(path_);
    if (!device_ || path_.empty() || !std::filesystem::is_regular_file(nativePath_,error_)) return false;
    return SUCCEEDED(CPUResourceLoader::LoadTextureFromFile(device_,nativePath_.c_str(),output_.GetAddressOf(),nullptr,TextureType::Data));
}
void UIPresentationPresets::ClearTexturePreviews() { texturePreviews.clear(); previewDevice = nullptr; }
bool UIPresentationPresets::DrawTexture(nlohmann::json& settings_,const char* key_,const char* label_)
{
    BS_PROFILE_SCOPE("UIPresentationPresets::DrawTexture");
    auto path_ = String(settings_,key_,"");
    const auto device_ = GraphicsDeviceManager::Instance().GetDevice();
    if (previewDevice != device_) { ClearTexturePreviews(); previewDevice = device_; }
    if (!path_.empty() && !texturePreviews.count(path_))
    {
        // パス切替時だけ読み込み、プレビューの保持数を制限する。
        if (texturePreviews.size() >= 16) texturePreviews.erase(texturePreviews.begin());
        LoadTexture(path_,texturePreviews[path_]);
    }
    const auto found_ = texturePreviews.find(path_);
    const bool changed_ = AssetSlotGui::Draw(label_,AssetSlotType::TEXTURE,path_,found_ == texturePreviews.end() ? nullptr : found_->second.Get(),"Data/Texture");
    if (changed_) settings_[key_] = path_;
    if (!path_.empty() && found_ != texturePreviews.end() && !found_->second) ImGui::TextDisabled("Texture unavailable: choose a readable image.");
    return changed_;
}

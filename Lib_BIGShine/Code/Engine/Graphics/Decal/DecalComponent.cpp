#include "DecalComponent.h"

#include "Editor/AssetSlotGui.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/Graphics/Core/CPUResourceLoader.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Graphics/Resource/TextureResourceManager.h"
#include "Engine/Platform/WindowsDialogHelper.h"

#include <cstdio>
#include <algorithm>
#include <filesystem>

void DecalComponent::Start()
{
    if (!texturePath.empty() && !texture) SetTexture(texturePath);
    ResetLifeTime();
}

void DecalComponent::SetLifeTime(float lifeTime_, float fadeOutDuration_)
{
    lifeTime = (std::max)(lifeTime_, 0.0f);
    fadeOutDuration = (std::clamp)(fadeOutDuration_, 0.0f, lifeTime);
    ResetLifeTime();
}

void DecalComponent::ResetLifeTime()
{
    elapsedTime = 0.0f;
    initialAlpha = color.w;
}

void DecalComponent::SetTexture(const std::string& path_)
{
    texturePath = path_;
    texture.Reset();
    if (texturePath.empty()) return;

    ID3D11Device* device_ = GraphicsDeviceManager::Instance().GetDevice();
    if (!device_) return;

    ID3D11ShaderResourceView* loadedTexture_ =
        TextureResourceManager::Instance().LoadTexture(device_, texturePath);
    if (loadedTexture_)
    {
        texture = loadedTexture_;
        return;
    }

    CPUResourceLoader::CreateDummyTexture(
        device_,
        0xFFFFFFFF,
        texture.ReleaseAndGetAddressOf());
    std::printf(
        "[DecalComponent] Failed to load texture: %s\n",
        texturePath.c_str());
}

void DecalComponent::SubmitDecal(DecalManager& manager_)
{
    if (!IsEnabled()) return;

    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;

    DecalData data_{};
    DirectX::XMStoreFloat4x4(
        &data_.worldMatrix,
        owner_->transform->GetWorldMatrix());
    data_.color = color;
    data_.texture = texture.Get();
    manager_.AddDecal(data_);
}

void DecalComponent::Update(float elapsedTime_)
{
    if (lifeTime > 0.0f)
    {
        elapsedTime += (std::max)(elapsedTime_, 0.0f);
        if (fadeOutDuration > 0.0f && elapsedTime > lifeTime - fadeOutDuration)
        {
            const float remainingTime_ = (std::max)(lifeTime - elapsedTime, 0.0f);
            color.w = initialAlpha * (remainingTime_ / fadeOutDuration);
        }
        if (elapsedTime >= lifeTime)
        {
            color.w = 0.0f;
            SetEnabled(false);
            if (destroyOnLifeEnd)
            {
                const std::shared_ptr<GameObject> owner_ = GetGameObject();
                if (owner_) GetManager().Destroy(owner_);
            }
            return;
        }
    }
    if (!showDebugBounds) return;

    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_ || !owner_->transform) return;

    const std::shared_ptr<Transform> transform_ = owner_->transform;
    const DirectX::XMFLOAT3 rotationDegrees_ = transform_->GetWorldRotation();
    const DirectX::XMFLOAT3 rotationRadians_ =
    {
        DirectX::XMConvertToRadians(rotationDegrees_.x),
        DirectX::XMConvertToRadians(rotationDegrees_.y),
        DirectX::XMConvertToRadians(rotationDegrees_.z),
    };

    GraphicsDeviceManager::Instance().GetShapeRendererSystem().DrawBox(
        transform_->GetWorldPosition(),
        rotationRadians_,
        transform_->GetWorldScale(),
        DirectX::XMFLOAT4(1.0f, 1.0f, 0.0f, 0.5f));
}

void DecalComponent::Gui()
{
    ImGui::ColorEdit4("Decal Color", &color.x);
    ImGui::Checkbox("Show Debug Bounds", &showDebugBounds);
    ImGui::DragFloat("Life Time", &lifeTime, 0.05f, 0.0f, 120.0f);
    ImGui::DragFloat("Fade Out Duration", &fadeOutDuration, 0.05f, 0.0f, lifeTime);
    ImGui::Checkbox("Destroy On Life End", &destroyOnLifeEnd);

    std::string selectedPath_ = texturePath;
    if (AssetSlotGui::Draw(
        "Decal Texture",
        AssetSlotType::TEXTURE,
        selectedPath_,
        texture.Get(),
        "Data"))
    {
        SetTexture(selectedPath_);
    }
}

void DecalComponent::Serialize(nlohmann::json& json_) const
{
    json_["ComponentName"] = componentName;
    json_["TexturePath"] = texturePath;
    json_["TextureFile"] = texturePath.empty()
        ? std::string{}
        : WindowsDialogHelper::PathToUtf8(
            WindowsDialogHelper::Utf8ToPath(texturePath).filename());
    json_["Color"] = { color.x, color.y, color.z, color.w };
    json_["ShowDebugBounds"] = showDebugBounds;
    json_["LifeTime"] = lifeTime;
    json_["FadeOutDuration"] = fadeOutDuration;
    json_["DestroyOnLifeEnd"] = destroyOnLifeEnd;
}

void DecalComponent::Deserialize(const nlohmann::json& json_)
{
    if (json_.contains("Color") &&
        json_["Color"].is_array() &&
        json_["Color"].size() >= 4u)
    {
        color.x = json_["Color"][0].get<float>();
        color.y = json_["Color"][1].get<float>();
        color.z = json_["Color"][2].get<float>();
        color.w = json_["Color"][3].get<float>();
    }
    showDebugBounds = json_.value("ShowDebugBounds", true);
    lifeTime = (std::max)(json_.value("LifeTime", 0.0f), 0.0f);
    fadeOutDuration = (std::clamp)(
        json_.value("FadeOutDuration", 0.0f),
        0.0f,
        lifeTime);
    destroyOnLifeEnd = json_.value("DestroyOnLifeEnd", false);

    std::string loadedPath_ = json_.value("TexturePath", std::string{});
    if (loadedPath_.empty())
    {
        const std::string legacyFile_ =
            json_.value("TextureFile", std::string{});
        if (!legacyFile_.empty())
        {
            const std::filesystem::path legacyPath_ =
                WindowsDialogHelper::Utf8ToPath(legacyFile_);
            loadedPath_ = legacyPath_.has_parent_path()
                ? legacyFile_
                : "Data/Decal/" + legacyFile_;
        }
    }
    SetTexture(loadedPath_);
    ResetLifeTime();
}

#include "PrecompiledHeader.h"

#include "DecalComponent.h"

#include "Editor/AssetSlotGui.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/Graphics/Core/CPUResourceLoader.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Graphics/Resource/TextureResourceManager.h"
#include "Engine/Graphics/VFXMaterial/VFXMaterialResourceManager.h"
#include "Engine/Graphics/VFXMaterial/VFXMaterialConversionUtility.h"
#include "Engine/Graphics/VFXMaterial/VFXMaterialSerializer.h"
#include "Engine/Platform/WindowsDialogHelper.h"
#include "Engine/Debug/TracyProfiler.h"

#include <cstdio>
#include <algorithm>
#include <filesystem>

void DecalComponent::Start()
{
    BS_PROFILE_SCOPE("DecalComponent::Start");
    // 保存済みの参照情報から、優先度の高いVFXマテリアルを先に復元する。
    // マテリアルがない古いデータでは、従来のテクスチャ指定をそのまま使う。
    if (!materialPath.empty() && !texture)
    {
        SetMaterial(materialPath);
    }
    else if (!texturePath.empty() && !texture)
    {
        SetTexture(texturePath);
    }
    ResetLifeTime();
}

void DecalComponent::SetLifeTime(float lifeTime_, float fadeOutDuration_)
{
    BS_PROFILE_SCOPE("DecalComponent::SetLifeTime");
    // 寿命とフェード時間は負数を受け付けず、フェードが寿命を超えない範囲に丸める。
    lifeTime = (std::max)(lifeTime_, 0.0f);
    fadeOutDuration = (std::clamp)(fadeOutDuration_, 0.0f, lifeTime);
    ResetLifeTime();
}

void DecalComponent::ResetLifeTime()
{
    BS_PROFILE_SCOPE("DecalComponent::ResetLifeTime");
    // フェードアウト計算の基準になる初期アルファを、再生開始時点の色から保存する。
    elapsedTime = 0.0f;
    materialTime = 0.0f;
    initialAlpha = color.w;
}

void DecalComponent::SetTexture(const std::string& path_)
{
    BS_PROFILE_SCOPE("DecalComponent::SetTexture");

    // 通常テクスチャ指定へ切り替えるため、VFXマテリアル側の参照は一度外す。
    materialPath.clear();
    materialResource.reset();
    texturePath = path_;
    texture.Reset();
    if (texturePath.empty())
    {
        return;
    }

    ID3D11Device* device_ = GraphicsDeviceManager::Instance().GetDevice();
    if (!device_)
    {
        return;
    }

    ID3D11ShaderResourceView* loadedTexture_ =
        TextureResourceManager::Instance().LoadTexture(device_, texturePath);
    if (loadedTexture_)
    {
        texture = loadedTexture_;
        return;
    }

    // 読み込み失敗時も描画側がnullptrを扱わずに済むよう、白ダミーテクスチャを入れておく。
    CPUResourceLoader::CreateDummyTexture(
        device_,
        0xFFFFFFFF,
        texture.ReleaseAndGetAddressOf());
    std::printf(
        "[DecalComponent] Failed to load texture: %s\n",
        texturePath.c_str());
}

void DecalComponent::SetGroundSurface(float emissiveStrength_, const std::string& maskPath_)
{
    groundSurface = true;
    groundEmissiveStrength = emissiveStrength_;
    showDebugBounds = false;
    groundMask.Reset();
    if (!maskPath_.empty())
    {
        if (auto* device_ = GraphicsDeviceManager::Instance().GetDevice())
        {
            groundMask = TextureResourceManager::Instance().LoadTexture(device_, maskPath_);
        }
    }
}

void DecalComponent::SetMaterial(const std::string& path_)
{
    BS_PROFILE_SCOPE("DecalComponent::SetMaterial");
    SetMaterials(path_.empty() ? std::vector<std::string>{} : std::vector<std::string>{ path_ });
}

void DecalComponent::SetMaterials(const std::vector<std::string>& paths_)
{
    BS_PROFILE_SCOPE("DecalComponent::SetMaterials");
    // 複数マテリアルを受け取りつつ、旧コード互換のため先頭をmaterialPath/materialResourceにも反映する。
    materialPaths = paths_;
    materialPath = materialPaths.empty() ? std::string{} : materialPaths.front();
    materialResource.reset();
    materialResources.clear();
    if (materialPaths.empty())
    {
        return;
    }

    for (const std::string& materialPath_ : materialPaths)
    {
        if (materialPath_.empty())
        {
            continue;
        }
        std::shared_ptr<VFXMaterialResource> loadedMaterial_ =
            VFXMaterialResourceManager::GetInstance().Load(materialPath_);
        if (!loadedMaterial_)
        {
            continue;
        }
        materialResources.push_back(loadedMaterial_);
        if (!materialResource)
        {
            materialResource = loadedMaterial_;
        }
        // プレビューやフォールバック描画で使えるよう、最後に見つかったベーステクスチャを保持する。
        if (loadedMaterial_->GetBaseTexture())
        {
            texture = loadedMaterial_->GetBaseTexture();
            texturePath = loadedMaterial_->GetMaterialData().baseTexturePath;
        }
    }

}

void DecalComponent::ApplyMaterialToDecalData(DecalData& data_) const
{
    BS_PROFILE_SCOPE("DecalComponent::ApplyMaterialToDecalData");

    if (materialResources.empty() && !materialResource)
    {
        return;
    }
    // DecalDataは1つなので、複数マテリアルは色やUVなどを順に合成して最終値へ畳み込む。
    const size_t materialCount_ = materialResources.empty() ? 1u : materialResources.size();
    for (size_t materialIndex_ = 0; materialIndex_ < materialCount_; ++materialIndex_)
    {
        const std::shared_ptr<VFXMaterialResource>& materialResource_ =
            materialResources.empty() ? materialResource : materialResources[materialIndex_];
        if (!materialResource_)
        {
            continue;
        }
        const VFXMaterialData& materialData_ = materialResource_->GetMaterialData();
    // 色はコンポーネント色とマテリアル色を掛け合わせ、alphaもマテリアル側の値を反映する。
    data_.color.x *= materialData_.baseColor.x;
    data_.color.y *= materialData_.baseColor.y;
    data_.color.z *= materialData_.baseColor.z;
    data_.color.w *= materialData_.baseColor.w * materialData_.alpha;
    data_.emissiveColorAndIntensity = {
        materialData_.emissiveColor.x,
        materialData_.emissiveColor.y,
        materialData_.emissiveColor.z,
        materialData_.emissiveIntensity
    };
    data_.uvScaleOffset = {
        materialData_.mainUV.scale.x,
        materialData_.mainUV.scale.y,
        materialData_.mainUV.offset.x + materialData_.mainUV.scrollSpeed.x * materialTime,
        materialData_.mainUV.offset.y + materialData_.mainUV.scrollSpeed.y * materialTime
    };
    // materialTimeを使って、デカールでもVFXマテリアルのUVスクロールを再現する。
    data_.noiseUvScaleOffset = {
        materialData_.noiseUV.scale.x,
        materialData_.noiseUV.scale.y,
        materialData_.noiseUV.offset.x + materialData_.noiseUV.scrollSpeed.x * materialTime,
        materialData_.noiseUV.offset.y + materialData_.noiseUV.scrollSpeed.y * materialTime
    };
    data_.dissolveParams = {
        materialData_.dissolve.enabled ? 1.0f : 0.0f,
        materialData_.dissolve.threshold,
        materialData_.dissolve.edgeWidth,
        0.0f
    };
    data_.dissolveEdgeColor = materialData_.dissolve.edgeColor;
    data_.blendMode = materialData_.renderState.blendMode == VFX_MATERIAL_BLEND_MODE::ADDITIVE ? 1u : 0u;
    data_.useDissolve = materialData_.dissolve.enabled ? 1u : 0u;
    // 実際のSRVはマテリアル側を優先し、未設定ならSubmit時点のtextureを使い続ける。
    if (materialResource_->GetBaseTexture())
        {
            data_.texture = materialResource_->GetBaseTexture();
        }
        if (materialResource_->GetMaskTexture())
        {
            data_.maskTexture = materialResource_->GetMaskTexture();
        }
        if (materialResource_->GetNoiseTexture())
        {
            data_.noiseTexture = materialResource_->GetNoiseTexture();
        }
    }
}

void DecalComponent::SubmitDecal(DecalManager& manager_)
{
    BS_PROFILE_SCOPE("DecalComponent::SubmitDecal");
    if (!IsEnabled())
    {
        return;
    }

    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_)
    {
        return;
    }

    DecalData data_{};
    // 描画システムへ渡すデカール情報を、現在のTransformと素材状態から毎フレーム構築する。
    DirectX::XMStoreFloat4x4(
        &data_.worldMatrix,
        owner_->transform->GetWorldMatrix());
    data_.color = color;
    data_.texture = texture.Get();
    ApplyMaterialToDecalData(data_);
    data_.groundSurface = groundSurface;
    if (groundSurface)
    {
        data_.emissiveColorAndIntensity.w = groundEmissiveStrength;
        data_.maskTexture = groundMask;
    }
    manager_.AddDecal(data_);
}

void DecalComponent::Update(float elapsedTime_)
{
    BS_PROFILE_SCOPE("DecalComponent::Update");
    // 素材アニメーションは寿命とは独立して進める。負のdeltaは外部入力の揺れとして無視する。
    materialTime += (std::max)(elapsedTime_, 0.0f);
    if (lifeTime > 0.0f)
    {
        elapsedTime += (std::max)(elapsedTime_, 0.0f);
        if (fadeOutDuration > 0.0f && elapsedTime > lifeTime - fadeOutDuration)
        {
            // 終了直前だけ初期アルファから0へ線形に落として、自然に消えるようにする。
            const float remainingTime_ = (std::max)(lifeTime - elapsedTime, 0.0f);
            color.w = initialAlpha * (remainingTime_ / fadeOutDuration);
        }
        if (elapsedTime >= lifeTime)
        {
            // 寿命を終えたデカールは描画を止め、必要なら所有GameObjectごと破棄する。
            color.w = 0.0f;
            SetEnabled(false);
            if (destroyOnLifeEnd)
            {
                const std::shared_ptr<GameObject> owner_ = GetGameObject();
                if (owner_)
                {
                    GetManager().Destroy(owner_);
                }
            }
            return;
        }
    }
    if (!showDebugBounds)
    {
        return;
    }

    // デバッグ時は投影範囲をボックスで表示し、向きやスケールの調整を確認しやすくする。
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_ || !owner_->transform)
    {
        return;
    }

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
    BS_PROFILE_SCOPE("DecalComponent::Gui");
    // デカール単体の見た目、寿命、素材差し替えをエディタ上で直接調整する。
    ImGui::ColorEdit4("Decal Color", &color.x);
    ImGui::Checkbox("Show Debug Bounds", &showDebugBounds);
    ImGui::DragFloat("Life Time", &lifeTime, 0.05f, 0.0f, 120.0f);
    ImGui::DragFloat("Fade Out Duration", &fadeOutDuration, 0.05f, 0.0f, lifeTime);
    ImGui::Checkbox("Destroy On Life End", &destroyOnLifeEnd);

    std::string selectedMaterialPath_ = materialPath;
    if (AssetSlotGui::Draw(
        "VFX Material",
        AssetSlotType::MATERIAL,
        selectedMaterialPath_,
        materialResource ? materialResource->GetBaseTexture() : nullptr,
        "Data/VFXMaterials"))
    {
        SetMaterial(selectedMaterialPath_);
    }
    if (materialPaths.empty())
    {
        materialPaths.push_back(materialPath);
    }
    int removeMaterialIndex_ = -1;
    // 複数スロットは順番に適用されるため、GUI上でも並びを保ったまま編集する。
    for (int materialIndex_ = 0; materialIndex_ < static_cast<int>(materialPaths.size()); ++materialIndex_)
    {
        ImGui::PushID(materialIndex_);
        ID3D11ShaderResourceView* materialPreviewTexture_ = nullptr;
        if (materialIndex_ < static_cast<int>(materialResources.size()) &&
            materialResources[materialIndex_])
        {
            materialPreviewTexture_ = materialResources[materialIndex_]->GetBaseTexture();
        }
        if (AssetSlotGui::Draw(
            "VFX Material Slot",
            AssetSlotType::MATERIAL,
            materialPaths[materialIndex_],
            materialPreviewTexture_,
            "Data/VFXMaterials"))
        {
            SetMaterials(materialPaths);
        }
        if (materialPaths.size() > 1u)
        {
            ImGui::SameLine();
            if (ImGui::Button("Remove"))
            {
                removeMaterialIndex_ = materialIndex_;
            }
        }
        ImGui::PopID();
    }
    if (removeMaterialIndex_ >= 0)
    {
        materialPaths.erase(materialPaths.begin() + removeMaterialIndex_);
        SetMaterials(materialPaths);
    }
    if (ImGui::Button("Add VFX Material Slot"))
    {
        materialPaths.emplace_back();
    }
    ImGui::SameLine();
    if (ImGui::Button("Convert To VFX Material"))
    {
        ConvertToMaterial();
    }

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
    BS_PROFILE_SCOPE("DecalComponent::Serialize");
    // MaterialPathsは現在形式、MaterialPath/TextureFileは古いデータとの互換用に残す。
    json_["ComponentName"] = componentName;
    json_["MaterialPath"] = materialPath;
    json_["MaterialPaths"] = materialPaths;
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
    BS_PROFILE_SCOPE("DecalComponent::Deserialize");
    // 色は配列形式で保存されるため、古い/壊れたデータでも要素数を確認してから読む。
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
    materialPath = json_.value("MaterialPath", std::string{});
    materialPaths.clear();
    // 新形式の複数マテリアルを優先し、なければ単体MaterialPathへフォールバックする。
    if (json_.contains("MaterialPaths") && json_["MaterialPaths"].is_array())
    {
        for (const auto& materialPathJson_ : json_["MaterialPaths"])
        {
            materialPaths.push_back(materialPathJson_.get<std::string>());
        }
    }
    if (materialPaths.empty() && !materialPath.empty())
    {
        materialPaths.push_back(materialPath);
    }
    if (materialPath.empty() && !materialPaths.empty())
    {
        materialPath = materialPaths.front();
    }

    std::string loadedPath_ = json_.value("TexturePath", std::string{});
    if (loadedPath_.empty())
    {
        // TextureFileだけを持つ旧データは、親パスがなければData/Decal配下として扱う。
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
    if (!materialPaths.empty())
    {
        SetMaterials(materialPaths);
    }
    else if (!materialPath.empty())
    {
        SetMaterial(materialPath);
    }
    else
    {
        SetTexture(loadedPath_);
    }
    ResetLifeTime();
}

bool DecalComponent::ConvertToMaterial()
{
    BS_PROFILE_SCOPE("DecalComponent::ConvertToMaterial");

    const std::string defaultPath_ = "Data/VFXMaterials/DecalMaterial.vfxmat";
    const std::string selectedPath_ = WindowsDialogHelper::SaveVFXMaterialDialog(
        L"Convert Decal To VFX Material",
        defaultPath_,
        GraphicsDeviceManager::Instance().GetWindowHandle());
    if (selectedPath_.empty())
    {
        return false;
    }

    VFXMaterialData materialData_ = VFXMaterialConversionUtility::FromDecal(
        "DecalMaterial",
        texturePath,
        color);
    if (!VFXMaterialSerializer::Save(selectedPath_, materialData_))
    {
        return false;
    }

    SetMaterial(selectedPath_);
    return true;
}

#include "PrecompiledHeader.h"

#include "AfterimageComponent.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/Graphics/Resource/GLTFModel.h"

#include <algorithm>
#include <cmath>
#include <imgui.h>
#include <nlohmann/json.hpp>
#include <utility>

AfterimageComponent::AfterimageComponent()
{
    // 残像の初期表示は「青い加算フレネル」の1パーツにして、設定なしでも効果が見える状態にする。
    MeshVFXPart part_;
    part_.renderEnabled = true;
    part_.useMaterialBaseTexture = false;
    part_.useMaterialNormalTexture = false;
    part_.color = { 0.2f, 0.75f, 1.0f, 1.0f };
    part_.alpha = 1.0f;
    part_.useFresnel = true;
    part_.fresnelPower = 2.0f;
    part_.fresnelStrength = 0.8f;
    part_.blendMode = MeshVFXBlendMode::ADDITIVE;
    GetAsset().parts.push_back(part_);
}

void AfterimageComponent::Start()
{
    MeshVFXComponent::Start();
    RegisterActions();
}

void AfterimageComponent::Update(float deltaTime_)
{
    MeshVFXComponent::Update(deltaTime_);
}

void AfterimageComponent::LateUpdate(float deltaTime_)
{

    // 既に保持しているスナップショットを老化させ、寿命を超えたものから描画対象外にする。
    for (Snapshot& snapshot_ : snapshots)
    {
        snapshot_.age += deltaTime_;
    }
    snapshots.erase(
        std::remove_if(
            snapshots.begin(), snapshots.end(),
            [this](const Snapshot& snapshot_)
    {
        return snapshot_.age >= lifetime;
    }),
        snapshots.end());

    if (!isEmitting && pendingBurstCount <= 0)
    {
        return;
    }

    spawnTimer += deltaTime_;
    const float safeInterval_ = (std::max)(spawnInterval, 0.001f);
    // フレーム落ちでdeltaが大きくなっても、間隔分だけ複数枚を補う。
    while (spawnTimer >= safeInterval_ && (isEmitting || pendingBurstCount > 0))
    {
        spawnTimer -= safeInterval_;
        Snapshot snapshot_;
        if (CaptureSnapshot(snapshot_))
        {
            AddSnapshotWithSpacing(snapshot_);
        }
        if (pendingBurstCount > 0)
        {
            --pendingBurstCount;
        }
    }
}

void AfterimageComponent::Gui()
{
    // 生成間隔、寿命、補間枚数をエディタで調整しながら、実際の残像密度を確認できるようにする。
    ImGui::SeparatorText("Afterimage");
    ImGui::DragFloat("Spawn Interval", &spawnInterval, 0.005f, 0.001f, 1.0f);
    ImGui::DragFloat("Lifetime", &lifetime, 0.01f, 0.01f, 10.0f);
    ImGui::DragFloat("Start Alpha", &startAlpha, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("Maximum Snapshot Spacing", &maximumSnapshotSpacing, 0.01f, 0.0f, 100.0f);
    ImGui::DragInt("Max Snapshots", &maxSnapshots, 1.0f, 1, 64);
    ImGui::DragInt("Max Render Snapshots", &maxRenderSnapshots, 1.0f, 1, 64);
    ImGui::DragInt("Max Render Parts", &maxRenderParts, 1.0f, 1, 16);
    ImGui::DragInt("Burst Count", &burstCount, 1.0f, 1, 32);
    ImGui::DragInt("Max Extra Snapshots", &maxExtraSnapshotsPerFrame, 1.0f, 0, 32);
    ImGui::Checkbox("Include Children", &includeChildren);
    ImGui::Text("Active: %zu", snapshots.size());

    if (ImGui::Button(isEmitting ? "Stop Emission" : "Start Emission"))
    {
        isEmitting ? StopEmission() : StartEmission();
    }
    ImGui::SameLine();
    if (ImGui::Button("Burst"))
    {
        Burst();
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear"))
    {
        Clear();
    }

    MeshVFXComponent::Gui();
}

void AfterimageComponent::Serialize(nlohmann::json& json_) const
{
    MeshVFXComponent::Serialize(json_);
    // MeshVFXの見た目設定に加えて、残像専用の生成・保持ルールを保存する。
    json_["SpawnInterval"] = spawnInterval;
    json_["Lifetime"] = lifetime;
    json_["StartAlpha"] = startAlpha;
    json_["MaximumSnapshotSpacing"] = maximumSnapshotSpacing;
    json_["MaxSnapshots"] = maxSnapshots;
    json_["MaxRenderSnapshots"] = maxRenderSnapshots;
    json_["MaxRenderParts"] = maxRenderParts;
    json_["BurstCount"] = burstCount;
    json_["MaxExtraSnapshotsPerFrame"] = maxExtraSnapshotsPerFrame;
    json_["IncludeChildren"] = includeChildren;
}

void AfterimageComponent::Deserialize(const nlohmann::json& json_)
{
    MeshVFXComponent::Deserialize(json_);
    // 不正な設定値でスナップショット管理が破綻しないよう、読み込み時に範囲を丸める。
    spawnInterval = (std::max)(json_.value("SpawnInterval", 0.06f), 0.001f);
    lifetime = (std::max)(json_.value("Lifetime", 0.4f), 0.01f);
    startAlpha = std::clamp(json_.value("StartAlpha", 0.8f), 0.0f, 1.0f);
    maximumSnapshotSpacing = (std::max)(json_.value("MaximumSnapshotSpacing", 0.8f), 0.0f);
    maxSnapshots = std::clamp(json_.value("MaxSnapshots", 16), 1, 64);
    maxRenderSnapshots = std::clamp(json_.value("MaxRenderSnapshots", 8), 1, maxSnapshots);
    maxRenderParts = std::clamp(json_.value("MaxRenderParts", 1), 1, 16);
    burstCount = std::clamp(json_.value("BurstCount", 3), 1, 32);
    maxExtraSnapshotsPerFrame = std::clamp(json_.value("MaxExtraSnapshotsPerFrame", 8), 0, 32);
    includeChildren = json_.value("IncludeChildren", true);
    Clear();
}

size_t AfterimageComponent::GetMaxRenderParts() const
{
    return static_cast<size_t>((std::max)(maxRenderParts, 1));
}

bool AfterimageComponent::GetModelPoseViews(std::vector<MeshVFXModelPoseView>& poses_) const
{
    poses_.clear();
    // 古いスナップショットをすべて描くと重くなるため、末尾の新しいものだけを描画ビューに変換する。
    const size_t snapshotCount_ = snapshots.size();
    const size_t renderSnapshotCount_ = (std::min)(
        snapshotCount_,
        static_cast<size_t>((std::max)(maxRenderSnapshots, 1)));
    const size_t firstSnapshotIndex_ = snapshotCount_ - renderSnapshotCount_;
    size_t poseCount_ = 0;
    for (size_t snapshotIndex_ = firstSnapshotIndex_; snapshotIndex_ < snapshotCount_; ++snapshotIndex_)
    {
        poseCount_ += snapshots[snapshotIndex_].poses.size();
    }
    poses_.reserve(poseCount_);

    for (size_t snapshotIndex_ = firstSnapshotIndex_; snapshotIndex_ < snapshotCount_; ++snapshotIndex_)
    {
        const Snapshot& snapshot_ = snapshots[snapshotIndex_];
        const float normalizedAge_ = std::clamp(snapshot_.age / lifetime, 0.0f, 1.0f);
        const float alpha_ = startAlpha * (1.0f - normalizedAge_);
        // Snapshot内の姿勢情報を直接参照する軽量ビューにして、行列配列のコピーを避ける。
        for (const MeshVFXModelPose& sourcePose_ : snapshot_.poses)
        {
            MeshVFXModelPoseView pose_;
            pose_.model = sourcePose_.model;
            pose_.meshTransforms = &sourcePose_.meshTransforms;
            pose_.time = snapshot_.age;
            pose_.alpha = alpha_;
            poses_.push_back(std::move(pose_));
        }
    }
    return !poses_.empty();
}

void AfterimageComponent::StartEmission()
{
    // 開始直後に1枚出せるよう、タイマーを生成間隔ぶん進めた状態にする。
    isEmitting = true;
    spawnTimer = spawnInterval;
}

void AfterimageComponent::StopEmission()
{
    isEmitting = false;
    pendingBurstCount = 0;
}

void AfterimageComponent::Burst()
{
    // 単発アクションから複数枚の残像を作る。実際の生成はLateUpdate側で間隔処理に乗せる。
    pendingBurstCount = burstCount;
    spawnTimer = spawnInterval;
}

void AfterimageComponent::Clear()
{
    // 状態リセット時もreserveしておき、戦闘中の再発生で余計な再確保を減らす。
    snapshots.clear();
    snapshots.reserve(static_cast<size_t>(maxSnapshots));
    pendingBurstCount = 0;
    spawnTimer = 0.0f;
}

void AfterimageComponent::RegisterActions()
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_)
    {
        return;
    }

    // アニメーションイベントや他コンポーネントから、名前指定で残像制御できるように登録する。
    auto& registry_ = owner_->GetLocalRegistry();
    registry_.RegisterAction("VFX", "AfterimageStart", [this]()
    {
        StartEmission();
    });
    registry_.RegisterAction("VFX", "AfterimageStop", [this]()
    {
        StopEmission();
    });
    registry_.RegisterAction("VFX", "AfterimageBurst", [this]()
    {
        Burst();
    });
    registry_.RegisterAction("VFX", "AfterimageClear", [this]()
    {
        Clear();
    });
}

bool AfterimageComponent::CaptureSnapshot(Snapshot& snapshot_)
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_ || !owner_->transform)
    {
        return false;
    }

    // ルートの現在位置を距離補間用に保存し、描画用のモデル姿勢はCaptureObjectで集める。
    snapshot_.poses.reserve(includeChildren ? 4 : 1);
    DirectX::XMFLOAT4X4 worldTransform_{};
    DirectX::XMStoreFloat4x4(&worldTransform_, owner_->transform->GetWorldMatrix());
    snapshot_.samplePosition =
    {
        worldTransform_._41,
        worldTransform_._42,
        worldTransform_._43
    };
    CaptureObject(owner_, snapshot_);
    return !snapshot_.poses.empty();
}

void AfterimageComponent::AddSnapshot(const Snapshot& snapshot_)
{
    // 最大枚数を超えたら最古を捨てる。残像は新しいほど視覚的に重要なので末尾を残す。
    snapshots.push_back(snapshot_);
    while (static_cast<int>(snapshots.size()) > maxSnapshots)
    {
        snapshots.erase(snapshots.begin());
    }
}

void AfterimageComponent::AddSnapshotWithSpacing(const Snapshot& snapshot_)
{
    // 前回位置から大きく移動したフレームでは、間に補間スナップショットを挟んで残像の途切れを防ぐ。
    if (!snapshots.empty() && maximumSnapshotSpacing > 0.0f && maxExtraSnapshotsPerFrame > 0)
    {
        const Snapshot previousSnapshot_ = snapshots.back();
        const float distance_ = CalculateSnapshotDistance(previousSnapshot_, snapshot_);
        const int extraSnapshotCount_ = (std::min)(
            (std::max)(static_cast<int>(std::ceil(distance_ / maximumSnapshotSpacing)) - 1, 0),
            maxExtraSnapshotsPerFrame);
        for (int extraIndex_ = 1; extraIndex_ <= extraSnapshotCount_; ++extraIndex_)
        {
            const float ratio_ =
                static_cast<float>(extraIndex_) / static_cast<float>(extraSnapshotCount_ + 1);
            AddSnapshot(InterpolateSnapshot(previousSnapshot_, snapshot_, ratio_));
        }
    }

    AddSnapshot(snapshot_);
}

AfterimageComponent::Snapshot AfterimageComponent::InterpolateSnapshot(
    const Snapshot& firstSnapshot_,
    const Snapshot& secondSnapshot_,
    float ratio_) const
{
    // 2つのスナップショット間を線形補間し、瞬間移動のような大きな移動でも軌跡を滑らかにする。
    Snapshot snapshot_;
    const float inverseRatio_ = 1.0f - ratio_;
    snapshot_.samplePosition =
    {
        firstSnapshot_.samplePosition.x * inverseRatio_ + secondSnapshot_.samplePosition.x * ratio_,
        firstSnapshot_.samplePosition.y * inverseRatio_ + secondSnapshot_.samplePosition.y * ratio_,
        firstSnapshot_.samplePosition.z * inverseRatio_ + secondSnapshot_.samplePosition.z * ratio_
    };
    snapshot_.poses = firstSnapshot_.poses;

    const size_t poseCount_ = (std::min)(snapshot_.poses.size(), secondSnapshot_.poses.size());
    for (size_t poseIndex_ = 0; poseIndex_ < poseCount_; ++poseIndex_)
    {
        MeshVFXModelPose& pose_ = snapshot_.poses[poseIndex_];
        const MeshVFXModelPose& secondPose_ = secondSnapshot_.poses[poseIndex_];
        const size_t meshCount_ = (std::min)(
            pose_.meshTransforms.size(),
            secondPose_.meshTransforms.size());
        for (size_t meshIndex_ = 0; meshIndex_ < meshCount_; ++meshIndex_)
        {
            std::vector<DirectX::XMFLOAT4X4>& transforms_ = pose_.meshTransforms[meshIndex_];
            const std::vector<DirectX::XMFLOAT4X4>& secondTransforms_ =
                secondPose_.meshTransforms[meshIndex_];
            const size_t transformCount_ = (std::min)(transforms_.size(), secondTransforms_.size());
            // 行列要素をそのまま補間する。残像用なので厳密な回転補間より見た目の連続性を優先する。
            for (size_t transformIndex_ = 0; transformIndex_ < transformCount_; ++transformIndex_)
            {
                float* firstValues_ = reinterpret_cast<float*>(&transforms_[transformIndex_]);
                const float* secondValues_ =
                    reinterpret_cast<const float*>(&secondTransforms_[transformIndex_]);
                for (int valueIndex_ = 0; valueIndex_ < 16; ++valueIndex_)
                {
                    firstValues_[valueIndex_] =
                        firstValues_[valueIndex_] * inverseRatio_ +
                        secondValues_[valueIndex_] * ratio_;
                }
            }
        }
    }
    return snapshot_;
}

float AfterimageComponent::CalculateSnapshotDistance(
    const Snapshot& firstSnapshot_,
    const Snapshot& secondSnapshot_) const
{
    const float x_ = secondSnapshot_.samplePosition.x - firstSnapshot_.samplePosition.x;
    const float y_ = secondSnapshot_.samplePosition.y - firstSnapshot_.samplePosition.y;
    const float z_ = secondSnapshot_.samplePosition.z - firstSnapshot_.samplePosition.z;
    return std::sqrt(x_ * x_ + y_ * y_ + z_ * z_);
}

void AfterimageComponent::CaptureObject(
    const std::shared_ptr<GameObject>& object_,
    Snapshot& snapshot_)
{
    if (!object_ || !object_->transform)
    {
        return;
    }

    // 対象GameObjectにモデルがあれば、現在のスキニング/メッシュ姿勢を残像用に保存する。
    const std::shared_ptr<GltfModelCommponennt> model_ =
        object_->GetComponent<GltfModelCommponennt>();
    if (model_)
    {
        model_->UpdateTransformIfDirty();
        const auto& meshes_ = model_->GetMeshes();
        MeshVFXModelPose pose_;
        pose_.model = model_;
        pose_.meshTransforms.resize(meshes_.size());
        for (size_t meshIndex_ = 0; meshIndex_ < meshes_.size(); ++meshIndex_)
        {
            const GltfModelCommponennt::Mesh& mesh_ = meshes_[meshIndex_];
            std::vector<DirectX::XMFLOAT4X4>& transforms_ = pose_.meshTransforms[meshIndex_];
            if (!mesh_.bones.empty())
            {
                // シェーダ側の定数数に合わせ、ボーン行列は最大256本まで保存する。
                const size_t boneCount_ = (std::min)(mesh_.bones.size(), static_cast<size_t>(256));
                transforms_.resize(boneCount_);
                for (size_t boneIndex_ = 0; boneIndex_ < boneCount_; ++boneIndex_)
                {
                    const GltfModelCommponennt::Bone& bone_ = mesh_.bones[boneIndex_];
                    const DirectX::XMMATRIX offset_ =
                        DirectX::XMLoadFloat4x4(&bone_.offsetTransform);
                    const DirectX::XMMATRIX nodeWorld_ = bone_.node
                        ? DirectX::XMLoadFloat4x4(&bone_.node->worldTransform)
                        : DirectX::XMMatrixIdentity();
                    DirectX::XMStoreFloat4x4(
                        &transforms_[boneIndex_], offset_ * nodeWorld_);
                }
            }
            else
            {
                // ボーンを持たないメッシュはノードのワールド行列1つを姿勢として扱う。
                transforms_.resize(1);
                const DirectX::XMMATRIX nodeWorld_ = mesh_.node
                    ? DirectX::XMLoadFloat4x4(&mesh_.node->worldTransform)
                    : DirectX::XMMatrixIdentity();
                DirectX::XMStoreFloat4x4(&transforms_[0], nodeWorld_);
            }
        }
        snapshot_.poses.push_back(std::move(pose_));
    }

    if (!includeChildren)
    {
        return;
    }
    // 武器や分割モデルもまとめて残像化したい場合は、子GameObjectを再帰的に拾う。
    for (const std::shared_ptr<Transform>& childTransform_ : object_->transform->children)
    {
        if (!childTransform_)
        {
            continue;
        }
        CaptureObject(childTransform_->gameObject.lock(), snapshot_);
    }
}

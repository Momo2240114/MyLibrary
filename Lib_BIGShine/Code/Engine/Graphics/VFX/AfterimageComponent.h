#pragma once

#include "MeshVFXComponent.h"

#include <vector>

class GameObject;

class AfterimageComponent : public MeshVFXComponent
{
public:
    AfterimageComponent();

    const std::string& GetName() const override
    {
        static std::string name = "AfterimageComponent";
        return name;
    }

    void Start() override;
    void Update(float deltaTime_) override;
    void LateUpdate(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    // 所有 GameObject のモデル姿勢をコピーして MeshVFX として描くコンポーネント。
    bool UsesOwnerModel() const override
    {
        return true;
    }

    bool UsesModelPoses() const override
    {
        return true;
    }

    bool HasModelPoses() const override
    {
        return !snapshots.empty();
    }

    size_t GetMaxRenderParts() const override;
    bool GetModelPoseViews(std::vector<MeshVFXModelPoseView>& poses_) const override;

    // 通常の時間間隔サンプリングを開始する。
    void StartEmission();

    // 新しい姿勢の追加だけを止め、既存の残像は寿命で自然に消す。
    void StopEmission();

    // 攻撃や回避の瞬間など、短時間に複数枚の残像を追加したい時に使う。
    void Burst();

    // 現在保持している残像姿勢を即座に破棄する。
    void Clear();

private:
    struct Snapshot
    {
        // 1 フレーム分のモデル姿勢。子オブジェクトを含める場合は複数パーツになる。
        std::vector<MeshVFXModelPose> poses;

        // 前回サンプルとの距離補間に使う代表座標。
        DirectX::XMFLOAT3 samplePosition = { 0.0f, 0.0f, 0.0f };

        // 残像として生成されてからの経過時間。アルファと削除判定に使う。
        float age = 0.0f;
    };

    // アクションイベントから残像開始・停止・バーストを呼べるように登録する。
    void RegisterActions();

    // 現在の所有オブジェクト階層から Snapshot を作る。
    bool CaptureSnapshot(Snapshot& snapshot_);

    // 単一 GameObject の MeshRenderer 姿勢を Snapshot に追加する。
    void CaptureObject(const std::shared_ptr<GameObject>& object_, Snapshot& snapshot_);

    // 保持数の上限を守りながら Snapshot を末尾へ追加する。
    void AddSnapshot(const Snapshot& snapshot_);

    // 移動距離が大きいフレームでも残像が飛び飛びにならないよう、中間 Snapshot を補間追加する。
    void AddSnapshotWithSpacing(const Snapshot& snapshot_);

    // 2 つの姿勢の間を補間し、最大間隔を埋めるための中間姿勢を作る。
    Snapshot InterpolateSnapshot(
        const Snapshot& firstSnapshot_,
        const Snapshot& secondSnapshot_,
        float ratio_) const;

    // サンプル同士の代表座標距離。残像の密度調整に使う。
    float CalculateSnapshotDistance(
        const Snapshot& firstSnapshot_,
        const Snapshot& secondSnapshot_) const;

    // 古い順に年齢が進む残像スナップショット。
    std::vector<Snapshot> snapshots;

    // 何秒ごとに姿勢を採取するか。小さいほど滑らかだが描画数が増える。
    float spawnInterval = 0.06f;

    // 1 枚の残像が残る時間。
    float lifetime = 0.4f;
    float startAlpha = 0.8f;

    // 速く動いた時に中間姿勢を補う距離の上限。
    float maximumSnapshotSpacing = 0.8f;
    float spawnTimer = 0.0f;

    // 保持枚数と描画枚数を分け、内部補間を残しつつ描画負荷を抑える。
    int maxSnapshots = 16;
    int maxRenderSnapshots = 8;
    int maxRenderParts = 1;

    // Burst で一度に追加する枚数と、1 フレーム内の補間追加数の上限。
    int burstCount = 3;
    int maxExtraSnapshotsPerFrame = 8;
    int pendingBurstCount = 0;

    // true の場合、所有 GameObject 配下の子モデルもまとめて残像化する。
    bool includeChildren = true;
    bool isEmitting = false;
};

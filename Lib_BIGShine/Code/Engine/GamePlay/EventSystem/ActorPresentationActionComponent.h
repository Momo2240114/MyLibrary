#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/GamePlay/EventSystem/IActionProvider.h"
#include <memory>
#include <string>
#include <utility>
#include <vector>

class GameObject;

enum class ActorPresentationActionType
{
    PLAY_ANIMATION,
    FACE_TARGET,
    RELEASE_ANIMATION,
    MOVE_FROM_OFFSET,
    PLAY_MESH_VFX,
    STOP_MESH_VFX,
    PLAY_PARTICLE,
    STOP_PARTICLE,
    MOVE_TO_TARGET,
    SET_VISIBLE,
    SET_HIDDEN,
    FREEZE_ACTOR,
    RELEASE_ACTOR,
    PLAY_AUDIO,
    STOP_AUDIO,
    SET_LOCK_ON_TARGET,
    SET_UI_TEXT,
    CLOSE_HIT_BOXES,
    RESET_PARTICLE,
    SPAWN_PREFAB = 19,
    DESTROY_ACTOR,
    CLEANUP_SPAWNED,
    COMMIT_ACTOR
};

// 演出対象と設定値を保持し、SequenceからAnimation・移動・VFXを呼び出す。
struct ActorPresentationActionPreset
{
    std::string actionName;
    std::string actorName;
    std::string stateName;
    std::string stateAssetPath;
    // 演出の再生時にAssetを反映し、同じStateも先頭から再開始する。
    bool refreshStateAssetOnPlay = false;
    std::string targetName;
    std::string prefabName;
    bool skipInPreview = false;
    float trailWidth = 1.2f;
    ActorPresentationActionType type = ActorPresentationActionType::PLAY_ANIMATION;
    float duration = 0.2f;
    bool smoothMovement = true;
    bool matchTargetRotation = false; // 暗転中の即時配置でMarkerの向きも使用する。
    bool includeChildren = false; // ドラゴンの部位Colliderなど、子Objectも停止・復帰する。
    // 移動経路に横方向の蛇行を加え、泥の襲撃にも同じ移動Actionを使う。
    float swayAmplitude = 0.0f;
    float swayCycles = 2.0f;
    // 現在位置を到着点とし、このローカル座標オフセットから歩いて登場させる。
    DirectX::XMFLOAT3 positionOffset{ 0.0f, 0.0f, 0.0f };
};

// 演出開始時の配置を、再生中に変化するTransformとは別に保存する。
struct ActorPresentationSpawnPoint
{
    std::string actorName;
    DirectX::XMFLOAT3 position{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 rotation{ 0.0f, 0.0f, 0.0f };
};

// 通常の入力・AIとは独立して、指定した対象へ演出用Actionを公開するComponent。
class ActorPresentationActionComponent : public Component, public IActionProvider
{
public:
    const std::string& GetName() const override
    {
        static const std::string NAME = "ActorPresentationActionComponent";
        return NAME;
    }
    // Scene開始時に設定済みActionを公開する。
    void Start() override;
    void OnDestroy() override;
    // 演出中の移動と水平方向の旋回を補間する。
    void Update(float deltaTime_) override;
    // 対象ObjectとAnimation・移動・VFX設定を編集する。
    void Gui() override;
    // 設定したAction一覧を保存・復元する。
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;
    // 設定済みのActionを所有Object名付きでGlobal Registryへ公開する。
    void RegisterActions() override;
    // Editorで配置した対象の位置と向きを、出現位置として登録する。
    void CaptureSpawnPoints();
    // 保存済みの配置へ戻し、登場演出を同じ位置から開始できるようにする。
    void RestoreSpawnPoints();
    // 単体再生前の配置を一度だけ記録し、終了時に戦闘中の位置へ戻す。
    void CaptureRuntimePositions();
    void RestoreRuntimePositions();

private:
    // 演出前の一時停止状態を保持し、保存用の有効設定は変更しない。
    struct FrozenActor
    {
        std::weak_ptr<GameObject> actor;
        std::vector<std::pair<std::weak_ptr<Component>, bool>> componentStates;
    };
    // 重複停止では状態を保存し直さず、復帰時に最初の状態を戻す。
    void SetActorFrozen(const std::shared_ptr<GameObject>& actor_, bool frozen_);
    std::vector<FrozenActor> frozenActors;
    // 指定対象のAnimation、移動、VFXの再生・停止と通常遷移への復帰を実行する。
    void ExecutePreset(int index_);
    // 通常入力とは独立して登場位置を補間し、既存ColliderのTransform同期へ渡す。
    void UpdateMovement(float deltaTime_);
    std::vector<ActorPresentationActionPreset> presets;
    std::vector<ActorPresentationSpawnPoint> spawnPoints; // 親Transformに対するローカル配置を保存する。
    std::vector<ActorPresentationSpawnPoint> runtimePositions;
    // 初回だけ現在の配置を登録し、旧JSONでも再生後の保存による位置ずれを防ぐ。
    void CaptureMissingSpawnPoints();
    std::weak_ptr<GameObject> movingActor;
    DirectX::XMFLOAT3 startMovePosition{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 targetMovePosition{ 0.0f, 0.0f, 0.0f };
    float moveElapsed = 0.0f;
    float moveDuration = 0.0f;
    bool smoothMovement = false;
    float moveSwayAmplitude = 0.0f;
    float moveSwayCycles = 0.0f;
    float moveTrailWidth = 1.2f;
    // 旋回だけを時間補間し、演出全体の時刻はActionSequenceへ任せる。
    std::weak_ptr<GameObject> turningActor;
    float startYaw = 0.0f;
    float targetYaw = 0.0f;
    float turnElapsed = 0.0f;
    float turnDuration = 0.0f;
};

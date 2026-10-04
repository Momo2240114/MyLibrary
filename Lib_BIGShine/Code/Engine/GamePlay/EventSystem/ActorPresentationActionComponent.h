#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/GamePlay/EventSystem/IActionProvider.h"
#include <memory>
#include <string>
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
    STOP_PARTICLE
};

// 演出対象と設定値を保持し、SequenceからAnimation・移動・VFXを呼び出す。
struct ActorPresentationActionPreset
{
    std::string actionName;
    std::string actorName;
    std::string stateName;
    std::string stateAssetPath;
    std::string targetName;
    ActorPresentationActionType type = ActorPresentationActionType::PLAY_ANIMATION;
    float duration = 0.2f;
    // 現在位置を到着点とし、このローカル座標オフセットから歩いて登場させる。
    DirectX::XMFLOAT3 positionOffset{ 0.0f, 0.0f, 0.0f };
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
    // 演出中の移動と水平方向の旋回を補間する。
    void Update(float deltaTime_) override;
    // 対象ObjectとAnimation・移動・VFX設定を編集する。
    void Gui() override;
    // 設定したAction一覧を保存・復元する。
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;
    // 設定済みのActionを所有Object名付きでGlobal Registryへ公開する。
    void RegisterActions() override;

private:
    // 指定対象のAnimation、移動、VFXの再生・停止と通常遷移への復帰を実行する。
    void ExecutePreset(int index_);
    // 通常入力とは独立して登場位置を補間し、既存ColliderのTransform同期へ渡す。
    void UpdateMovement(float deltaTime_);
    std::vector<ActorPresentationActionPreset> presets;
    std::weak_ptr<GameObject> movingActor;
    DirectX::XMFLOAT3 startMovePosition{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 targetMovePosition{ 0.0f, 0.0f, 0.0f };
    float moveElapsed = 0.0f;
    float moveDuration = 0.0f;
    // 旋回だけを時間補間し、演出全体の時刻はActionSequenceへ任せる。
    std::weak_ptr<GameObject> turningActor;
    float startYaw = 0.0f;
    float targetYaw = 0.0f;
    float turnElapsed = 0.0f;
    float turnDuration = 0.0f;
};

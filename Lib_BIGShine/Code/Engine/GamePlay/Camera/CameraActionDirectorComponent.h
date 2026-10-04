#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/GamePlay/EventSystem/IActionProvider.h"

#include <DirectXMath.h>
#include <memory>
#include <string>
#include <vector>

class BlackboardComponent;
class CameraCollisionComponent;
class CameraComponent;
class CameraLockOnComponent;
class GameObject;
class ThirdPersonCameraComponent;

enum class CAMERA_ACTION_PHASE
{
    // 通常カメラ制御中。演出上書きは行わない。
    IDLE,

    // 通常カメラ位置から演出用カメラ位置へ移動する。
    ENTER,

    // 演出用カメラ位置を維持する。
    HOLD,

    // 演出開始前の通常カメラ制御へ戻す。
    RETURN
};

enum class CAMERA_ACTION_TARGET_MODE
{
    // Preset または外部呼び出しで渡されたワールド座標を見る。
    WORLD_POSITION,

    // 指定 GameObject の現在位置を見る。
    GAME_OBJECT,

    // GameObject 位置に focusOffset を足した地点を見る。
    GAME_OBJECT_OFFSET,

    // Blackboard に保存されたヒット位置を見る。
    HIT_POSITION,

    // Player GameObject を探して見る。
    PLAYER,

    // LockOnComponent が現在捕捉している対象を見る。
    LOCK_ON_TARGET
};

enum class CAMERA_ACTION_POSITION_MODE
{
    // 現在カメラ位置を基準に cameraOffset を足す。
    CURRENT_CAMERA_RELATIVE,

    // 注視対象のローカル軸を基準に cameraOffset を解釈する。
    TARGET_LOCAL_RELATIVE,

    // Player の向きを基準に、前後左右の相対位置を作る。
    PLAYER_FORWARD_RELATIVE,

    // TPS カメラの見ている向きを基準に配置する。
    TPS_VIEW_RELATIVE,

    // cameraOffset をそのままワールド座標として使う。
    WORLD_POSITION,

    // TPS追従対象の位置と回転から、保存したカメラ位置・注視点・上方向を復元する。
    TPS_TARGET_RELATIVE
};

// ジャスト回避・ジャストガードなど、短時間だけ通常カメラへ上書きする演出プリセット。
// ターゲットの取り方、カメラ位置の解釈、抑制する通常制御をまとめてJSON保存する。
struct CameraActionPreset
{
    std::string actionName = "JustGuard";
    std::string targetObjectName; // 指定時はScene内のObject位置を注視基準にする。

    CAMERA_ACTION_TARGET_MODE targetMode =
        CAMERA_ACTION_TARGET_MODE::WORLD_POSITION;
    CAMERA_ACTION_POSITION_MODE positionMode =
        CAMERA_ACTION_POSITION_MODE::CURRENT_CAMERA_RELATIVE;

    // cameraOffset はカメラ位置、focusOffset は注視点側の補正に使う。
    DirectX::XMFLOAT3 cameraOffset{ 0.0f, 0.5f, -1.5f };
    DirectX::XMFLOAT3 focusOffset{ 0.0f, 0.0f, 0.0f };
    // TPS_TARGET_RELATIVEで使用する対象ローカルの上方向。カメラの傾きも保持する。
    DirectX::XMFLOAT3 cameraUp{ 0.0f, 1.0f, 0.0f };

    // ENTER/HOLD/RETURN の各フェーズ時間。
    float enterDuration = 0.08f;
    float holdDuration = 0.12f;
    float returnDuration = 0.18f;

    // 演出中だけ差し替える FOV と、必要なら追加する揺れ設定。
    float targetFov = 45.0f;
    bool useExternalFov = false; // HOLD中はFovDirectorの画角を維持し、独立したFOV演出を重ねる。
    float shakeStrength = 0.0f;
    float shakeFrequency = 20.0f;

    // 同時に要求された演出の勝ち負けを決める。高い値ほど優先。
    int priority = 0;

    // 通常カメラ側の制御をどこまで止めるか。演出の破綻を防ぐため Preset ごとに選ぶ。
    bool suppressThirdPersonControl = true;
    bool suppressLockOnControl = true;
    bool suppressCameraCollision = false;
    bool useUnscaledTime = true;

    // HIT_POSITION などで Blackboard から読むカテゴリとキー。
    std::string blackboardCategory = "Guard";
    std::string focusPositionKey = "Position";
    std::string attackDirectionKey = "Normal";
};

// EventSystemから呼ばれる戦闘用カメラ演出Component。
// TPS、LockOn、Collisionを必要に応じて一時停止し、ENTER/HOLD/RETURNで自然に通常カメラへ戻す。
class CameraActionDirectorComponent : public Component, public IActionProvider
{
public:
    const std::string& GetName() const override
    {
        static std::string name_ = "CameraActionDirectorComponent";
        return name_;
    }

    void Start() override;
    void LateUpdate(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;
    void RegisterActions() override;

    // エディタ上の Preset 番号から再生する。
    void PlayActionByIndex(int index_);

    // ActionSequence から名前指定で再生する。
    void PlayActionByName(const std::string& actionName_);

    // 外部で計算した注視点を使って再生する。
    void PlayActionAtPosition(
        const std::string& actionName_,
        const DirectX::XMFLOAT3& focusPosition_);
    void PlayActionAtPosition(
        const std::string& actionName_,
        const DirectX::XMFLOAT3& focusPosition_,
        const DirectX::XMFLOAT3& attackDirection_);
    void PlayActionForTarget(
        const std::string& actionName_,
        const std::shared_ptr<GameObject>& target_);

    // 現在の演出を止める。immediate_ が true なら補間なしで通常制御へ戻す。
    void StopAction(bool immediate_);

private:
    // 所有 GameObject から Camera/TPS/LockOn/Blackboard などの参照を取り直す。
    void ResolveReferences();

    // Preset と解決済みターゲットから、ENTER フェーズに必要な始点・終点を確定する。
    void BeginAction(
        const CameraActionPreset& preset_,
        const DirectX::XMFLOAT3& focusPosition_,
        const DirectX::XMFLOAT3& attackDirection_,
        const std::shared_ptr<GameObject>& target_);
    bool ResolvePresetFocus(
        const CameraActionPreset& preset_,
        DirectX::XMFLOAT3& focusPosition_,
        DirectX::XMFLOAT3& attackDirection_);

    // Blackboard に保存された Vector3 を読み、ヒット位置や攻撃方向の入力として使う。
    bool TryReadBlackboardVector3(
        const std::string& category_,
        const std::string& key_,
        DirectX::XMFLOAT3& value_) const;
    DirectX::XMFLOAT3 ResolveTargetPosition(
        const CameraActionPreset& preset_,
        const DirectX::XMFLOAT3& fallbackPosition_) const;

    // targetMode/positionMode/attackDirection を組み合わせ、実際のカメラ座標を作る。
    DirectX::XMFLOAT3 CalculateCameraPosition(
        const CameraActionPreset& preset_,
        const DirectX::XMFLOAT3& focusPosition_,
        const DirectX::XMFLOAT3& attackDirection_) const;
    void ApplyCameraPose(
        const DirectX::XMFLOAT3& cameraPosition_,
        const DirectX::XMFLOAT3& lookAtPosition_,
        float fov_,
        const DirectX::XMFLOAT3& up_);

    // 演出中だけ止めていた通常カメラ制御を元に戻す。
    void ReleaseSuppression();

    // 現在のカメラ状態から Preset 初期値を作り、エディタで調整しやすくする。
    void CaptureCurrentCameraAsPreset(CameraActionPreset& preset_) const;
    int FindPresetIndex(const std::string& actionName_) const;
    float GetPhaseDuration() const;

    // ENTER/RETURN の補間を急に始まらず急に止まらないカーブにする。
    float SmoothStep(float value_) const;
    float ClampFov(float fov_) const;

private:
    // 通常カメラ制御を持つコンポーネント群。演出中だけ一時的に抑制する。
    std::weak_ptr<CameraComponent> camera;
    std::weak_ptr<ThirdPersonCameraComponent> thirdPersonCamera;
    std::weak_ptr<CameraLockOnComponent> lockOnCamera;
    std::weak_ptr<CameraCollisionComponent> collisionCamera;
    std::weak_ptr<BlackboardComponent> blackboard;
    std::weak_ptr<GameObject> activeTarget;
    std::weak_ptr<GameObject> activeFollowTarget; // 演出開始時のTPS追従対象を演出終了まで保持する。

    // エディタで保存する演出一覧と、現在再生中の Preset コピー。
    std::vector<CameraActionPreset> presets;
    CameraActionPreset activePreset;
    CAMERA_ACTION_PHASE phase = CAMERA_ACTION_PHASE::IDLE;

    // ENTER 開始時のカメラ状態と、Preset から計算した演出中のカメラ状態。
    DirectX::XMFLOAT3 startCameraPosition{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 startFocusPosition{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 actionCameraPosition{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 actionFocusPosition{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 startCameraUp{ 0.0f, 1.0f, 0.0f };
    DirectX::XMFLOAT3 actionCameraUp{ 0.0f, 1.0f, 0.0f };
    DirectX::XMFLOAT3 returnStartCameraUp{ 0.0f, 1.0f, 0.0f };

    // RETURN 開始時の状態。HOLD 中に通常側が動いても、戻りの始点を安定させる。
    DirectX::XMFLOAT3 returnStartCameraPosition{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 returnStartFocusPosition{ 0.0f, 0.0f, 0.0f };
    float startFov = 60.0f;
    float activeFov = 60.0f;
    float returnStartFov = 60.0f;
    float phaseTimer = 0.0f;
    int activePriority = -1;

    // 自分が抑制した制御だけを ReleaseSuppression で戻すための所有フラグ。
    bool ownsThirdPersonSuppression = false;
    bool ownsLockOnSuppression = false;
    bool ownsCollisionSuppression = false;
};

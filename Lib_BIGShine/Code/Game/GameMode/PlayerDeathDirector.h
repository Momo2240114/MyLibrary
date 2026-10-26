#pragma once

#include <DirectXMath.h>
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "Engine/UI/MenuPresentation.h"

class GameObjectManager;
class CameraComponent;
class Component;
class GameObject;

// 死亡からコンテニュー後の起き上がりまで、通常の戦闘更新を止めて演出する。
class PlayerDeathDirector
{
public:
    explicit PlayerDeathDirector(GameObjectManager& manager_);
    // 致死ダメージの通知は予約だけに留め、当たり判定の走査後に開始する。
    void RequestDeath(const std::shared_ptr<GameObject>& player_);
    // 死亡を検出し、専用時計でアニメーションとカメラを進める。
    bool Update(float deltaTime_);
    bool IsActive() const { return active || !requestedPlayer.expired(); }
    std::shared_ptr<CameraComponent> GetCamera() const { return camera; }
    // 死亡暗転とコンテニューメニューを通常UIとして描画する。
    void RenderOutput(SpriteRenderSystem& sprites_);
    // Game Modeウィンドウから演出設定を編集・保存する。
    void DrawSettingsGUI();
    // シーン破棄やEditor Stopでも一時的な当たり判定抑制とロックを戻す。
    void Stop();

private:
    enum class Phase { DYING, CONTINUE, RECOVERING };
    // 戦闘状態を退避し、死亡地点での専用再生を準備する。
    void Begin(const std::shared_ptr<GameObject>& player_);
    // 敵を更新せず、Playerステートと既存アニメーショングラフ、装備追従だけを更新する。
    void UpdatePlayerPresentation(float deltaTime_);
    // HPを全回復し、ロックを維持したまま起き上がりへ切り替える。
    void BeginRecovery();
    // 演出設定を読み込み、不足項目を既定値で補完する。
    void LoadSettings();
    // 実行時の状態を含めず、編集した設定だけを保存する。
    void SaveSettings();
    // 速度・時間の不正値を補正し、ゼロ除算や進行不能を防ぐ。
    void NormalizeSettings();

    GameObjectManager& manager;
    nlohmann::json settings;
    std::string status;
    std::weak_ptr<GameObject> requestedPlayer;
    std::shared_ptr<GameObject> player;
    std::shared_ptr<CameraComponent> camera;
    // Collider配列はComponent型で保持されるため、共通の抑制状態として退避する。
    std::vector<std::pair<std::weak_ptr<Component>, bool>> colliderStates;
    DirectX::XMFLOAT3 startEye{};
    DirectX::XMFLOAT3 startFocus{};
    DirectX::XMFLOAT3 endEye{};
    DirectX::XMFLOAT3 endFocus{};
    DirectX::XMFLOAT3 cameraUp{0.0f, 1.0f, 0.0f};
    Phase phase = Phase::DYING;
    float elapsed = 0.0f;
    float deathFinishedTime = -1.0f;
    float fadeAlpha = 0.0f;
    bool active = false;
    MenuPresentation continueMenu;
    bool returningToTitle = false;
    bool previousPlayerLock = false;
    bool previousAILock = false;
    bool previousGameplayLock = false;
    bool previousPresentation = false;
    bool previousBattlePause = false;
    bool previousCursorLock = false;
    bool previousInputCapture = false;
};

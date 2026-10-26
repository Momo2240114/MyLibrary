#pragma once

#include "Engine/Graphics/Resource/Animation/ModelAnimator.h"
#include "Engine/GamePlay/Transform/Transform.h"
#include <nlohmann/json.hpp>
#include <memory>
#include <vector>

class GameObjectManager;
class CameraComponent;
class SpriteRenderSystem;
class ParticleComponent;
class MeshVFXComponent;

// 人型ボスの決着を通常の戦闘更新から分離し、撮影とタイトル復帰まで管理する。
class BossFinishDirector
{
public:
    explicit BossFinishDirector(GameObjectManager& manager_);
    ~BossFinishDirector();
    // 生のフレーム時間で進行し、trueの間はManager側の通常更新を停止する。
    bool Update(float rawDeltaTime_);
    bool IsActive() const { return active; }
    bool IsPreview() const { return preview; }
    bool IsPaused() const { return paused; }
    float GetTime() const { return elapsed; }
    float GetCaptureTime() const;
    void Begin(bool preview_);
    // プレビュー前のTransform・ポーズを戻し、戦闘再開時の位置飛びを防ぐ。
    void Stop();
    void SetPaused(bool paused_) { paused = paused_; }
    void Seek(float time_);
    void RequestCapture();
    void SetShowFixed(bool fixed_) { showFixed = fixed_; }
    bool IsShowingFixed() const { return showFixed; }
    std::shared_ptr<CameraComponent> GetCamera() const { return camera; }
    const std::vector<std::shared_ptr<GameObject>>& GetEffects() const { return effects; }
    bool ShouldRenderActor(const std::shared_ptr<GameObject>& object_) const;
    // PostEffect後、UI描画前の出力だけを複製し、同じ画像をSDRの2D経路で表示する。
    void RenderOutput(SpriteRenderSystem& sprites_, ID3D11RenderTargetView* target_);
    ID3D11ShaderResourceView* GetCapture() const { return capturedView.Get(); }
    DirectX::XMFLOAT4 GetCaptureUv() const { return captureUv; }
    float GetAspect() const;
    DirectX::XMFLOAT4 GetBackgroundColor() const;
    void CaptureEditorCamera();
    bool Save();
    bool Load();
    // 本番の既定プリセットを残したまま、演出案を別ファイルで保存・読込する。
    bool SavePreset(const std::string& path_);
    bool LoadPreset(const std::string& path_);
    // 専用Animatorが実際に評価したState尺と時刻をEditorへ公開する。
    struct PreviewActorInfo
    {
        std::string clipName;
        float duration = 0.0f;
        float sampleTime = 0.0f;
        bool ready = false;
    };
    PreviewActorInfo GetPreviewActorInfo(bool boss_) const;
    float GetImpactCueTime() const;
    // プレビューだけに再生倍率と撮影用フリーカメラを適用する。
    void SetPreviewPlaybackRate(float rate_);
    void SetPreviewCameraOverride(bool enabled_) { previewCameraOverride = enabled_ && preview; }
    nlohmann::json& GetSettings() { return settings; }
    const std::string& GetStatus() const { return status; }

private:
    // 通常Animatorへ触れず、イベントを取り除いたコピーを専用Animatorで評価する。
    struct ActorPose
    {
        std::shared_ptr<GameObject> object;
        AnimState state;
        ModelAnimator animator;
        bool ready = false;
    };
    // 装備のAttachment更新で変化する行列も含め、編集前の状態を記録する。
    struct SavedActor
    {
        std::shared_ptr<GameObject> object;
        Transform transform;
        std::vector<GltfModelCommponennt::NodePose> poses;
        bool visible = true;
        std::vector<std::pair<std::weak_ptr<Component>,bool>> colliderStates;
    };
    void PrepareActor(ActorPose& actor_, const char* key_);
    void SampleActor(ActorPose& actor_, float time_);
    // 命中ポーズの剣と敵の胴体から、接触する踏み込み先を準備する。
    void PrepareImpact();
    // アニメーション内の命中時刻を演出時計へ変換する。
    float GetImpactTime() const;
    // 演出設定に従い、敵または接触点を基準にエフェクト位置を求める。
    DirectX::XMFLOAT3 GetEffectPosition(const nlohmann::json& config_) const;
    void Evaluate(float time_, float deltaTime_);
    void CreateEffects();
    void AdvanceEffects(float from_, float to_);
    void PrepareSprites();
    bool Capture(ID3D11RenderTargetView* target_);
    void DrawSprite(SpriteRenderSystem& sprites_, const std::shared_ptr<GameObject>& object_,
        float x_, float y_, float width_, float height_, const DirectX::XMFLOAT4& color_);
    void Normalize();

    GameObjectManager& manager;
    nlohmann::json settings;
    std::string status;
    std::vector<float> previousBgmVolumes;
    std::vector<SavedActor> savedActors;
    std::vector<std::shared_ptr<GameObject>> effects;
    std::shared_ptr<CameraComponent> camera;
    ActorPose player;
    ActorPose boss;
    // 命中前の構えと死亡リアクションを別の状態として評価する。
    ActorPose bossReaction;
    std::shared_ptr<GameObject> imageSprite;
    std::shared_ptr<GameObject> solidSprite;
    std::shared_ptr<GameObject> clearSprite;
    std::shared_ptr<GameObject> promptSprite;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> capturedTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> capturedView;
    DirectX::XMFLOAT4 captureUv{0, 0, 1, 1};
    bool active = false;
    bool preview = false;
    bool paused = false;
    bool showFixed = false;
    bool captureRequested = false;
    bool inputArmed = false;
    bool returning = false;
    bool titleRequested = false;
    bool previewCameraOverride = false;
    float previewPlaybackRate = 1.0f;
    float elapsed = 0.0f;
    float returnElapsed = 0.0f;
    float effectsTime = 0.0f;
    // 命中位置と成否はプレビューでも同じポーズから再計算する。
    DirectX::XMFLOAT3 impactPosition{};
    DirectX::XMFLOAT3 alignedEndPosition{};
    bool impactReady = false;
    bool impactConfirmed = false;
    // 本番の撃破受付は演出内の構図診断と分離し、骨・剣の配置でクリア進行を失わない。
    bool finishAuthorized = false;
    bool completionCommitted = false;
    bool previousPlayerLock = false;
    bool previousAILock = false;
    bool previousPresentation = false;
    bool previousGameplayLock = false;
};

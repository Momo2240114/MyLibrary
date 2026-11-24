#pragma once

#include "GameModeManager.h"
#include "Engine/Graphics/Resource/Animation/ModelAnimator.h"
#include "Engine/GamePlay/Transform/Transform.h"
#include <nlohmann/json.hpp>
#include <memory>
#include <span>
#include <vector>

class GameObjectManager;
class GraphicsDeviceManager;
class CameraComponent;
class SpriteRenderSystem;
class ParticleComponent;
class MeshVFXComponent;
class TextureMaskTransition;

// 人型ボスの決着を通常の戦闘更新から分離し、撮影とタイトル復帰まで管理する。
class BossFinishDirector
{
public:
    enum class Ending { TRUE_END, BAD_END };

    struct EndingCue
    {
        const char* label;
        float start;
        float end;
    };

    explicit BossFinishDirector(GameObjectManager& manager_);
    ~BossFinishDirector();
    // 生のフレーム時間で進行し、trueの間はManager側の通常更新を停止する。
    bool Update(float rawDeltaTime_);

    bool IsActive() const
    {
        return active;
    }

    bool IsPreview() const
    {
        return preview;
    }

    bool IsPaused() const
    {
        return paused;
    }

    float GetTime() const
    {
        return elapsed;
    }

    float GetCaptureTime() const;
    void Begin(bool preview_, Ending previewEnding_ = Ending::TRUE_END);
    // プレビュー前のTransform・ポーズを戻し、戦闘再開時の位置飛びを防ぐ。
    void Stop();

    void SetPaused(bool paused_)
    {
        paused = paused_;
    }

    void Seek(float time_);
    void RequestCapture();

    void SetShowFixed(bool fixed_)
    {
        showFixed = fixed_;
    }

    bool IsShowingFixed() const
    {
        return showFixed;
    }

    std::shared_ptr<CameraComponent> GetCamera() const
    {
        return camera;
    }

    const std::vector<std::shared_ptr<GameObject>>& GetEffects() const
    {
        return effects;
    }

    bool ShouldRenderActor(const std::shared_ptr<GameObject>& object_) const;
    bool IsBadEndingImpactShot() const;

    bool IsBadEnding() const
    {
        return ending == Ending::BAD_END;
    }

    float GetEndingBranchTime() const;
    float GetClearTime() const;
    float GetPreviewEndTime(Ending ending_) const;
    std::span<const EndingCue> GetBadEndingCues() const;
    std::span<const EndingCue> GetTrueEndingCues() const;
    void ApplyPostEffects(GraphicsDeviceManager& graphics_) const;
    // PostEffect後、UI描画前の出力だけを複製し、同じ画像をSDRの2D経路で表示する。
    void RenderOutput(SpriteRenderSystem& sprites_, ID3D11RenderTargetView* target_, bool resultPreview_ = false);
    void UpdateResultPreview(float deltaTime_);
    void DrawResultPreviewControls();

    ID3D11ShaderResourceView* GetCapture() const
    {
        return capturedView.Get();
    }

    DirectX::XMFLOAT4 GetCaptureUv() const
    {
        return captureUv;
    }

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

    void SetPreviewCameraOverride(bool enabled_)
    {
        previewCameraOverride = enabled_ && preview;
    }

    nlohmann::json& GetSettings()
    {
        return settings;
    }

    const std::string& GetStatus() const
    {
        return status;
    }

private:
    // 通常Animatorへ触れず、イベントを取り除いたコピーを専用Animatorで評価する。
    struct ActorPose
    {
        std::shared_ptr<GameObject> object;
        AnimState state;
        ModelAnimator animator;
        bool ready = false;
        // シークで変化しない骨格の水平基準を、連続するクリップ間でも共有する。
        DirectX::XMFLOAT3 rootReferencePosition{};
        bool hasRootReference = false;
        bool constrainHorizontal = true;
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
    void PrepareActor(ActorPose& actor_, const std::shared_ptr<GameObject>& object_, AnimState state_,
        const std::string& animation_, bool loop_);
    void SampleActor(ActorPose& actor_, float time_);
    // 親ノードの移動も含め、骨格の水平位置を演出開始時の基準へ戻す。
    void ConstrainActorPose(ActorPose& actor_, std::vector<GltfModelCommponennt::NodePose>& poses_);
    // 配置を動かさず、走り攻撃・ダウンから拘束開始のポーズへ補間する。
    void BlendExecutionPose(ActorPose& source_, ActorPose& execution_, float sourceTime_, float blend_);
    // 拘束と叩きつけの同期再生を準備する。
    void PrepareExecution();
    // ステージ中央を演出の配置基準として取得する。
    DirectX::XMFLOAT3 GetStageAnchor() const;
    // アニメーション内の命中時刻を演出時計へ変換する。
    float GetImpactTime() const;
    // 演出設定に従い、敵または接触点を基準にエフェクト位置を求める。
    DirectX::XMFLOAT3 GetEffectPosition(const nlohmann::json& config_) const;
    void Evaluate(float time_, float deltaTime_);
    void CreateEffects();
    void AdvanceEffects(float from_, float to_);
    void BeginTrueEnding();
    void ResetTrueEnding();
    void AdvanceTrueEnding(float time_);
    void UpdateTrueEnding(float previousTime_, float time_);
    void BeginBadEnding();
    void ResetBadEnding();
    void AdvanceBadEnding(float time_);
    void UpdateBadEnding(float previousTime_, float time_);
    void ApplyBadEndingMaterials();
    void SetEndingCamera(Vector3 eye_, Vector3 focus_, float fov_, float roll_, float time_, float shake_);
    void RenderBadEnding(SpriteRenderSystem& sprites_, ID3D11RenderTargetView* target_, float width_, float height_);
    float GetResultStartTime(Ending ending_) const;
    void RenderResult(SpriteRenderSystem& sprites_, float left_, float top_, float width_, float height_, float held_);
    void CompleteFinish();
    void ReturnToTitle();
    void PrepareSprites(const GameModeManager::BattleResult* previewResult_ = nullptr);
    void RefreshResultSprites(const GameModeManager::BattleResult* result_, bool sampleRank_);
    bool Capture(ID3D11RenderTargetView* target_);
    void DrawSprite(SpriteRenderSystem& sprites_, const std::shared_ptr<GameObject>& object_,
        float x_, float y_, float width_, float height_, const DirectX::XMFLOAT4& color_);
    void Normalize();
    bool ReloadResultRanks();

    GameObjectManager& manager;
    nlohmann::json settings;
    std::string status;
    std::vector<float> previousBgmVolumes;
    std::vector<SavedActor> savedActors;
    std::vector<std::shared_ptr<GameObject>> effects;
    std::shared_ptr<CameraComponent> camera;
    ActorPose player;
    ActorPose boss;
    // 拘束する側と受ける側のクリップを同じ時刻で評価する。
    ActorPose playerExecution;
    ActorPose bossReaction;
    std::shared_ptr<GameObject> imageSprite;
    std::shared_ptr<GameObject> solidSprite;
    std::shared_ptr<GameObject> clearSprite;
    std::shared_ptr<GameObject> promptSprite;
    std::shared_ptr<GameObject> resultLabelsSprite;
    std::shared_ptr<GameObject> resultValuesSprite;
    std::shared_ptr<GameObject> resultRanksSprite;
    std::shared_ptr<GameObject> overallLabelSprite;
    std::shared_ptr<GameObject> overallRankSprite;
    std::shared_ptr<GameObject> rankBurstSprite;
    char overallRank = '\0';
    GameModeManager::BattleResult resultPreviewValues;
    float resultPreviewAge = 0.0f;
    bool resultPreviewReady = false;
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
    // 叩きつけの基準位置と、同期再生の準備状態を保持する。
    DirectX::XMFLOAT3 impactPosition{};
    bool impactReady = false;
    bool impactConfirmed = false;
    // 本番の撃破受付は演出クリップの準備状態と分離し、クリア進行を維持する。
    bool finishAuthorized = false;
    bool completionCommitted = false;
    bool previousPlayerLock = false;
    bool previousAILock = false;
    bool previousPresentation = false;
    bool previousGameplayLock = false;

    Ending ending = Ending::TRUE_END;
    float trueEndingTime = -1.0f;
    DirectX::XMFLOAT3 trueEndingPosition{};
    DirectX::XMFLOAT3 trueEndingBeamOrigin{};
    ActorPose trueEndingIdle;
    ActorPose trueEndingAwaken;
    ActorPose trueEndingSweep;
    ActorPose trueEndingStrike;
    std::shared_ptr<GameObject> trueEndingMud;
    std::shared_ptr<GameObject> trueEndingHandLight;
    std::shared_ptr<GameObject> trueEndingAura;
    std::shared_ptr<GameObject> trueEndingBladeLight;
    std::shared_ptr<GameObject> trueEndingArc;
    std::shared_ptr<GameObject> trueEndingBeam;
    std::shared_ptr<GameObject> trueEndingPillar;

    float badEndingTime = -1.0f;
    DirectX::XMFLOAT3 badEndingPosition{};
    DirectX::XMFLOAT3 badEndingCameraEye{};
    DirectX::XMFLOAT3 badEndingCameraFocus{};
    DirectX::XMFLOAT3 badEndingSpearContact{};
    DirectX::XMFLOAT3 badEndingFallPosition{};
    ActorPose badEndingCounter;
    ActorPose badEndingKneel;
    ActorPose badEndingPierced;
    ActorPose badEndingRise;
    std::shared_ptr<GameObject> badEndingMud;
    std::shared_ptr<GameObject> badEndingSpear;
    std::shared_ptr<GameObject> badEndingParticles;
    std::shared_ptr<GameObject> flashbackSprite;
    std::unique_ptr<TextureMaskTransition> badEndingWipe;
    std::vector<std::pair<std::weak_ptr<Component>, bool>> badEndingMaterialStates;
    bool previousSilhouette = false;
    DirectX::XMFLOAT4 previousSilhouetteColor{};
};

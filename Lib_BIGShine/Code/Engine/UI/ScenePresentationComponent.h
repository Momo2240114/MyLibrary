#pragma once
#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/Graphics/RenderSystem/BloomSystem.h"
#include "Engine/Graphics/RenderSystem/VignetteSystem.h"

class CameraComponent;

// JSONで指定したSceneのカメラ、ロゴ、装備同期と表示演出を管理する。
class ScenePresentationComponent : public Component
{
public:
    // JSON復元で使用するComponent名を返す。
    const std::string& GetName() const override;
    // 指定された音声とカーソルの初期状態を準備する。
    void Start() override;
    // Scene終了時に音声とカーソルの一時状態を解放する。
    void OnDestroy() override;
    // Scene内の参照先へカメラ、ロゴ、装備の演出を反映する。
    void Update(float deltaTime_) override;
    // 描画中だけPostEffectを差し替え、同時に準備している次Sceneへ設定を持ち越さない。
    void BeginRender();
    // 描画前の共有PostEffect設定へ戻す。
    void EndRender();
    // JSONで指定した既存Cameraを取得する。
    std::shared_ptr<CameraComponent> GetCamera() const;
    // Scene描画を初期化する背景色を取得する。
    DirectX::XMFLOAT4 GetClearColor() const;
    // 演出設定をInspectorと開発用設定Windowから編集する。
    void Gui() override;
    // 参照先と演出設定をScene Objectへ保存する。
    void Serialize(nlohmann::json& json_) const override;
    // Scene Objectの演出設定を復元する。
    void Deserialize(const nlohmann::json& json_) override;
private:
    // 保存時は設定だけを出力し、時間や外部リソースの所有権は保存しない。
    nlohmann::json settings = {{"Presentation", nlohmann::json::object()}};
    float elapsedTime = 0.0f;
    // JSONで指定したアニメーション候補の切り替え時刻と乱数状態を保持する。
    float animationSwitchTime = 0.0f;
    unsigned int animationChoice = 0;
    bool musicStarted = false;
    std::string playingMusicPath;
    bool previousCursorLock = false;
    bool cursorChanged = false;
    bool showSettings = false;
    bool rendering = false;
    BloomSystem::Parameters previousBloom;
    VignetteSystem::Parameters previousVignette;
};

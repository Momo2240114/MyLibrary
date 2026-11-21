#pragma once

#include "MeshVFXComponent.h"

class MaterialOverlayComponent : public MeshVFXComponent
{
public:
    MaterialOverlayComponent();

    const std::string& GetName() const override
    {
        static std::string name = "MaterialOverlayComponent";
        return name;
    }

    void Start() override;
    void Update(float deltaTime_) override;
    bool IsLoadReady() const override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    bool UsesOwnerModel() const override
    {
        return false;
    }

    // フェードアウト中も描画が必要なので、activeまたはweight残りで判定する。
    bool IsOverlayActive() const
    {
        return overlayActive || overlayWeight > 0.0f;
    }

    // MeshVFXRenderSystemへ渡す最終的な重み。
    float GetOverlayWeight() const;

    // Overlayをフェードイン開始する。
    void StartOverlay();

    // Overlayをフェードアウト開始する。
    void StopOverlay();

    // Overlay状態とweightを即座に初期化する。
    void ResetOverlay();

private:
    // ActionSequenceからStart/Stop/Resetを呼べるように登録する。
    void RegisterActions();

    // trueの間はfadeIn、falseになったらfadeOut方向へweightを動かす。
    bool overlayActive = false;

    // 0.0～1.0の表示重み。
    float overlayWeight = 0.0f;

    // Overlayの出入りにかける時間。
    float fadeInDuration = 0.08f;
    float fadeOutDuration = 0.25f;
};

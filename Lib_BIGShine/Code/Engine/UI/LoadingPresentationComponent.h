#pragma once
#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "LoadingPresentation.h"

// Managerが公開するロード状態だけを使い、Sceneに依存せず起動・ロードUIを表示する。
class LoadingPresentationComponent : public Component
{
public:
    const std::string& GetName() const override;
    void Start() override;
    void OnDestroy() override;
    void UpdateUnscaled(float deltaTime_) override;
    void RenderUI() override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

private:
    LoadingPresentation presentation;
    std::string brand = "AUTO";
    bool standalone = false;
};

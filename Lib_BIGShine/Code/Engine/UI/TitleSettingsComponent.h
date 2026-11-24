#pragma once

#include "Engine/Core/GameRuntimeSettings.h"
#include "MenuPresentation.h"
#include "Engine/GamePlay/Core/Component/ComponentBase.h"

class CameraComponent;

class TitleSettingsComponent : public Component
{
public:
    const std::string& GetName() const override;
    void Start() override;
    void Update(float deltaTime_) override;
    void OnDestroy() override;
    void RenderSettings(SpriteRenderSystem& sprites_);

    bool IsOpen() const
    {
        return open;
    }

    bool WantsMouseCapture() const
    {
        return open && mouseCameraTest;
    }

    std::shared_ptr<CameraComponent> GetPreviewCamera() const;

private:
    void Open();
    void Close(bool save_);
    void UpdateControls();

    GameRuntimeSettings::UserPreferences previousPreferences;
    std::shared_ptr<GameObject> previewCameraObject;
    MenuPresentation visuals;
    std::string numericInput;
    int selectedRow = 0;
    int editingRow = -1;
    int sliderDragRow = -1;
    bool replaceNumericInput = false;
    bool inputArmed = false;
    bool open = false;
    bool mouseCameraTest = false;
    bool saveFailed = false;
};

#include "PrecompiledHeader.h"
#include "TitleSettingsComponent.h"

#include "Engine/Audio/AudioManager.h"
#include "Engine/GamePlay/Camera/CameraComponent.h"
#include "Engine/GamePlay/Camera/ThirdPersonCameraComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Input/ControllerInput.h"
#include "Engine/GamePlay/Input/KeyInput.h"
#include "Engine/GamePlay/Input/MouseInput.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Graphics/RenderSystem/SpriteRenderSystem.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>

namespace
{
    struct SettingRow
    {
        const char* label;
        float GameRuntimeSettings::UserPreferences::* preference;
        int minimum;
        int maximum;
    };

    using Preferences = GameRuntimeSettings::UserPreferences;
    constexpr std::array<SettingRow, 7> ROWS{{
        {"明るさ", &Preferences::brightness, 50, 150},
        {"マスター音量", &Preferences::masterVolume, 0, 100},
        {"BGM 音量", &Preferences::bgmVolume, 0, 100},
        {"SE 音量", &Preferences::seVolume, 0, 100},
        {"マウス感度", &Preferences::mouseSensitivity, 10, 300},
        {"コントローラー感度", &Preferences::controllerSensitivity, 10, 300},
        {"振動の強さ", &Preferences::vibration, 0, 100}
    }};

    struct SettingsRect
    {
        float x;
        float y;
        float width;
        float height;
        bool Contains(const DirectX::XMFLOAT2& point_) const
        {
            return point_.x >= x && point_.x < x + width && point_.y >= y && point_.y < y + height;
        }
    };

    struct SettingsLayout
    {
        float scale;
        float left;
        float top;
    };

    constexpr SettingsRect SE_BUTTON{64, 536, 153, 40};
    constexpr SettingsRect VIBRATION_BUTTON{226, 536, 154, 40};
    constexpr SettingsRect SAVE_BUTTON{64, 606, 278, 44};
    constexpr SettingsRect CANCEL_BUTTON{353, 606, 215, 44};
    constexpr SettingsRect MOUSE_CAMERA_BUTTON{663, 603, 270, 48};

    float GetRowY(int index_)
    {
        return 122.0f + index_ * 54.0f + (index_ >= 4 ? 24.0f : 0.0f);
    }

    SettingsRect GetSliderRect(int index_)
    {
        return {263.0f, GetRowY(index_) + 4.0f, 213.0f, 36.0f};
    }

    SettingsRect GetNumberRect(int index_)
    {
        return {496.0f, GetRowY(index_) + 4.0f, 70.0f, 36.0f};
    }

    SettingsLayout GetSettingsLayout()
    {
        const auto& graphics_ = GraphicsDeviceManager::Instance();
        const float width_ = static_cast<float>((std::max)(graphics_.GetRenderWidth(), 1));
        const float height_ = static_cast<float>((std::max)(graphics_.GetRenderHeight(), 1));
        const float scale_ = (std::min)(width_ / 1280.0f, height_ / 720.0f);
        return {scale_, (width_ - 1280.0f * scale_) * 0.5f, (height_ - 720.0f * scale_) * 0.5f};
    }

    DirectX::XMFLOAT2 GetSettingsPointer(const SettingsLayout& layout_)
    {
        auto& graphics_ = GraphicsDeviceManager::Instance();
        const auto point_ = MouseInput::Instance().GetScreenPos();
        RECT rect_{};
        GetClientRect(graphics_.GetWindowHandle(), &rect_);
        const float scaleX_ = static_cast<float>(graphics_.GetRenderWidth()) / (std::max)(rect_.right, 1L);
        const float scaleY_ = static_cast<float>(graphics_.GetRenderHeight()) / (std::max)(rect_.bottom, 1L);
        return {(point_.x * scaleX_ - layout_.left) / layout_.scale, (point_.y * scaleY_ - layout_.top) / layout_.scale};
    }
}


const std::string& TitleSettingsComponent::GetName() const
{
    static const std::string NAME = "TitleSettingsComponent";
    return NAME;
}

void TitleSettingsComponent::Start()
{
    if (const auto owner_ = GetGameObject())
    {
        owner_->RegisterNamedGlobalAction("UI", "UI.OpenTitleSettings", [this]()
        {
            Open();
        });
    }
}

void TitleSettingsComponent::Open()
{
    if (open)
    {
        return;
    }
    previousPreferences = GameRuntimeSettings::GetUserPreferences();
    visuals.Initialize(GetManager());
    selectedRow = 0;
    editingRow = sliderDragRow = -1;
    inputArmed = false;
    open = true;
    mouseCameraTest = false;
    saveFailed = false;

    // 本編のTPS設定を読み、タイトルのキャラクターを追従対象にして同じ操作感を試す。
    std::ifstream file_("Data/Scenes/MainScene/Objects/MainCamera.json");
    if (!file_ || !GetManager().Find("TitlePlayer"))
    {
        return;
    }
    try
    {
        const auto profile_ = nlohmann::json::parse(file_);
        nlohmann::json cameraSettings_;
        nlohmann::json tpsSettings_;
        for (const auto& entry_ : profile_.at("Components"))
        {
            const auto type_ = entry_.value("TypeName", std::string());
            if (type_ == "CameraComponent")
            {
                cameraSettings_ = entry_.at("Data");
            }
            else if (type_ == "ThirdPersonCameraComponent")
            {
                tpsSettings_ = entry_.at("Data");
            }
        }
        if (!cameraSettings_.is_object() || !tpsSettings_.is_object())
        {
            return;
        }
        tpsSettings_["targetName"] = "TitlePlayer";
        auto object_ = std::make_shared<GameObject>("TitleSettingsCamera");
        object_->transform->gameObject = object_;
        object_->SetNoSaveObject();
        const auto camera_ = object_->AddComponent<CameraComponent>(false);
        camera_->Deserialize(cameraSettings_);
        const auto tps_ = object_->AddComponent<ThirdPersonCameraComponent>(false);
        tps_->Deserialize(tpsSettings_);
        object_->SetManager(&GetManager());
        tps_->Start();
        tps_->LateUpdate(0.0f);
        previewCameraObject = std::move(object_);
    }
    catch (const nlohmann::json::exception&)
    {
        previewCameraObject.reset();
    }
}

std::shared_ptr<CameraComponent> TitleSettingsComponent::GetPreviewCamera() const
{
    return open && previewCameraObject ? previewCameraObject->GetComponent<CameraComponent>() : nullptr;
}

void TitleSettingsComponent::Update(float deltaTime_)
{
    if (!open)
    {
        return;
    }
    const auto window_ = GraphicsDeviceManager::Instance().GetWindowHandle();
    const bool focused_ = GetForegroundWindow() == window_ && !IsIconic(window_);
    if (mouseCameraTest && (!focused_ || KeyInput::KeyPressedIn(VK_ESCAPE) ||
        ControllerInput::Instance().ButtonPressedIn(PadCode::B)))
    {
        mouseCameraTest = false;
        MouseInput::Instance().SetCursorLockEnabled(false);
        return;
    }
    if (focused_ && !mouseCameraTest)
    {
        UpdateControls();
    }
    else
    {
        sliderDragRow = -1;
    }
    if (const auto camera_ = GetPreviewCamera())
    {
        const auto& graphics_ = GraphicsDeviceManager::Instance();
        const float aspect_ = static_cast<float>((std::max)(graphics_.GetRenderWidth(), 1)) /
            (std::max)(graphics_.GetRenderHeight(), 1);
        camera_->SetPerspectiveFov(camera_->GetFovY(), aspect_, camera_->GetNear(), camera_->GetFar());
        auto projection_ = camera_->GetProjection();
        // 視点の回転や画角を変えず、注視点を設定パネルの右側に移す。
        projection_._31 += 0.43f * 1280.0f * GetSettingsLayout().scale / (std::max)(graphics_.GetRenderWidth(), 1);
        camera_->SetProjectionMatrix(projection_);
        if (focused_)
        {
            previewCameraObject->GetComponent<ThirdPersonCameraComponent>()->LateUpdate(std::clamp(deltaTime_, 0.0f, 0.1f));
        }
    }
}


void TitleSettingsComponent::UpdateControls()
{
    auto preferences_ = GameRuntimeSettings::GetUserPreferences();
    const auto layout_ = GetSettingsLayout();
    const auto point_ = GetSettingsPointer(layout_);
    const auto& mouse_ = MouseInput::Instance();
    const auto& pad_ = ControllerInput::Instance();
    const bool enter_ = KeyInput::KeyPressedIn(VK_RETURN);
    const bool escape_ = KeyInput::KeyPressedIn(VK_ESCAPE);
    if (!inputArmed)
    {
        inputArmed = !mouse_.LeftDown() && !KeyInput::KeyPressed(VK_RETURN) && !pad_.ButtonPressed(PadCode::A);
        return;
    }

    const auto currentPercent_ = [&](int index_)
    {
        return static_cast<int>(std::lround((preferences_.*ROWS[index_].preference) * 100.0f));
    };
    bool changed_ = false;
    const auto setPercent_ = [&](int index_, int value_)
    {
        const auto& row_ = ROWS[index_];
        const int clamped_ = std::clamp(value_, row_.minimum, row_.maximum);
        if (clamped_ != currentPercent_(index_))
        {
            preferences_.*row_.preference = clamped_ / 100.0f;
            changed_ = true;
        }
    };
    const auto commitNumber_ = [&]()
    {
        if (editingRow >= 0 && !numericInput.empty())
        {
            int value_ = 0;
            for (const char digit_ : numericInput)
            {
                value_ = value_ * 10 + digit_ - '0';
            }
            setPercent_(editingRow, value_);
        }
        editingRow = -1;
    };
    const auto beginNumber_ = [&](int index_)
    {
        selectedRow = editingRow = index_;
        numericInput = std::to_string(currentPercent_(index_));
        replaceNumericInput = true;
    };

    bool save_ = pad_.ButtonPressedIn(PadCode::START) || (enter_ && KeyInput::KeyPressed(VK_CONTROL));
    bool cancel_ = pad_.ButtonPressedIn(PadCode::B);
    bool playSe_ = pad_.ButtonPressedIn(PadCode::X);
    bool playRumble_ = pad_.ButtonPressedIn(PadCode::Y);
    if (editingRow >= 0)
    {
        if (escape_ || cancel_)
        {
            editingRow = -1;
            cancel_ = false;
        }
        else if (enter_)
        {
            commitNumber_();
        }
        else
        {
            if (KeyInput::KeyPressedIn(VK_BACK) && !numericInput.empty())
            {
                numericInput.pop_back();
                replaceNumericInput = false;
            }
            for (int digit_ = 0; digit_ < 10; ++digit_)
            {
                if (KeyInput::KeyPressedIn('0' + digit_) || KeyInput::KeyPressedIn(VK_NUMPAD0 + digit_))
                {
                    if (replaceNumericInput)
                    {
                        numericInput.clear();
                        replaceNumericInput = false;
                    }
                    if (numericInput.size() < 3)
                    {
                        numericInput.push_back(static_cast<char>('0' + digit_));
                    }
                }
            }
        }
    }
    else
    {
        constexpr int ROW_COUNT = static_cast<int>(ROWS.size());
        if (pad_.ButtonPressedIn(PadCode::UP) || KeyInput::KeyPressedIn(VK_UP))
        {
            selectedRow = (selectedRow + ROW_COUNT - 1) % ROW_COUNT;
        }
        else if (pad_.ButtonPressedIn(PadCode::DOWN) || KeyInput::KeyPressedIn(VK_DOWN))
        {
            selectedRow = (selectedRow + 1) % ROW_COUNT;
        }
        const bool right_ = pad_.ButtonPressedIn(PadCode::RIGHT) || KeyInput::KeyPressedIn(VK_RIGHT);
        const bool left_ = pad_.ButtonPressedIn(PadCode::LEFT) || KeyInput::KeyPressedIn(VK_LEFT);
        if (right_ != left_)
        {
            setPercent_(selectedRow, currentPercent_(selectedRow) + (right_ ? 5 : -5));
        }
        if (enter_ && !save_)
        {
            beginNumber_(selectedRow);
        }
        cancel_ |= escape_;
    }

    if (mouse_.LeftPressed())
    {
        // 別の欄やボタンへ進んだときは、入力済みの数字を先に確定する。
        if (editingRow < 0 || !GetNumberRect(editingRow).Contains(point_))
        {
            commitNumber_();
            for (int index_ = 0; index_ < static_cast<int>(ROWS.size()); ++index_)
            {
                if (GetNumberRect(index_).Contains(point_))
                {
                    beginNumber_(index_);
                    break;
                }
                if (GetSliderRect(index_).Contains(point_))
                {
                    selectedRow = sliderDragRow = index_;
                    break;
                }
            }
        }
        playSe_ |= SE_BUTTON.Contains(point_);
        playRumble_ |= VIBRATION_BUTTON.Contains(point_);
        save_ |= SAVE_BUTTON.Contains(point_);
        cancel_ |= CANCEL_BUTTON.Contains(point_);
        if (previewCameraObject && MOUSE_CAMERA_BUTTON.Contains(point_))
        {
            mouseCameraTest = true;
        }
    }
    if (!mouse_.LeftDown())
    {
        sliderDragRow = -1;
    }
    if (sliderDragRow >= 0)
    {
        const auto rect_ = GetSliderRect(sliderDragRow);
        const auto& row_ = ROWS[sliderDragRow];
        const float fraction_ = std::clamp((point_.x - rect_.x) / rect_.width, 0.0f, 1.0f);
        setPercent_(sliderDragRow, static_cast<int>(std::lround(std::lerp(
            static_cast<float>(row_.minimum), static_cast<float>(row_.maximum), fraction_))));
    }

    if (save_)
    {
        commitNumber_();
    }
    if (changed_)
    {
        GameRuntimeSettings::SetUserPreferences(preferences_);
        AudioManager::Instance().ApplyUserVolumes();
        if (preferences_.vibration <= 0.0f)
        {
            ControllerInput::Instance().StopRumble();
        }
    }
    if (playSe_)
    {
        AudioManager::Instance().PlaySE(SE_TITLESELECT);
    }
    if (playRumble_ && pad_.SupportsRumble())
    {
        ControllerInput::Instance().PlayRumble(0.62f, 0.46f, 0.26f, 10);
    }
    if (cancel_ || save_)
    {
        Close(save_ && !cancel_);
    }
}

void TitleSettingsComponent::RenderSettings(SpriteRenderSystem& sprites_)
{
    if (!open)
    {
        return;
    }
    auto& graphics_ = GraphicsDeviceManager::Instance();
    const auto layout_ = GetSettingsLayout();
    const auto point_ = GetSettingsPointer(layout_);
    const auto& preferences_ = GameRuntimeSettings::GetUserPreferences();
    const auto& pad_ = ControllerInput::Instance();
    constexpr DirectX::XMFLOAT4 INK{0.93f, 0.90f, 0.84f, 1};
    constexpr DirectX::XMFLOAT4 MUTED{0.62f, 0.59f, 0.61f, 1};
    constexpr DirectX::XMFLOAT4 GOLD{0.85f, 0.70f, 0.46f, 1};
    constexpr DirectX::XMFLOAT4 BORDER{0.33f, 0.27f, 0.27f, 1};
    const auto draw_ = [&](const std::string& key_, const SettingsRect& bounds_, const DirectX::XMFLOAT4& color_,
        const char* text_ = nullptr, float fontSize_ = 20.0f)
    {
        visuals.SubmitVisual(sprites_, key_.c_str(), layout_.left + bounds_.x * layout_.scale,
            layout_.top + bounds_.y * layout_.scale, bounds_.width * layout_.scale,
            bounds_.height * layout_.scale, color_, text_, fontSize_);
    };
    visuals.SubmitVisual(sprites_, "SettingsDim", 0.0f, 0.0f, static_cast<float>(graphics_.GetRenderWidth()),
        static_cast<float>(graphics_.GetRenderHeight()), {0.015f, 0.012f, 0.032f, 0.19f});
    draw_("SettingsPanel", {36, 26, 562, 668}, {0.045f, 0.039f, 0.054f, 0.97f});
    draw_("SettingsAccent", {36, 26, 562, 2}, GOLD);
    draw_("SettingsTitle", {58, 41, 144, 52}, INK, "設定", 42.0f);
    draw_("AudioGroup", {64, 94, 116, 25}, GOLD, "画面・音声", 18.0f);
    draw_("AudioDivider", {187, 107, 383, 1}, BORDER);
    draw_("InputGroup", {64, 335, 116, 25}, GOLD, "操作・振動", 18.0f);
    draw_("InputDivider", {187, 348, 383, 1}, BORDER);
    for (int index_ = 0; index_ < static_cast<int>(ROWS.size()); ++index_)
    {
        const auto& row_ = ROWS[index_];
        const float y_ = GetRowY(index_);
        const auto slider_ = GetSliderRect(index_);
        const auto valueBox_ = GetNumberRect(index_);
        const bool selected_ = selectedRow == index_;
        const bool editing_ = editingRow == index_;
        const bool hovered_ = !mouseCameraTest && (slider_.Contains(point_) || valueBox_.Contains(point_));
        const int value_ = static_cast<int>(std::lround((preferences_.*row_.preference) * 100.0f));
        const float fraction_ = std::clamp(static_cast<float>(value_ - row_.minimum) / (row_.maximum - row_.minimum), 0.0f, 1.0f);
        const std::string key_ = "Setting" + std::to_string(index_);
        const auto valueText_ = editing_ ? numericInput + "_" : std::to_string(value_) + "%";
        if (selected_ || hovered_)
        {
            draw_(key_ + "Highlight", {62, y_, 508, 44}, {0.105f, 0.085f, 0.10f, 1});
            draw_(key_ + "Marker", {62, y_ + 8, 2, 28}, GOLD);
        }
        draw_(key_ + "Label", {71, y_, 178, 44}, selected_ ? GOLD : INK, row_.label, 20.0f);
        draw_(key_ + "Track", {slider_.x, y_ + 20, slider_.width, 4}, BORDER);
        draw_(key_ + "Fill", {slider_.x, y_ + 20, slider_.width * fraction_, 4}, GOLD);
        draw_(key_ + "Thumb", {slider_.x + slider_.width * fraction_ - 4, y_ + 15, 8, 14}, selected_ ? INK : GOLD);
        draw_(key_ + "ValueFrame", valueBox_, editing_ ? GOLD : BORDER);
        draw_(key_ + "ValueBackground", {valueBox_.x + 1, valueBox_.y + 1, valueBox_.width - 2, valueBox_.height - 2},
            editing_ ? DirectX::XMFLOAT4{0.20f, 0.14f, 0.10f, 1} : DirectX::XMFLOAT4{0.07f, 0.061f, 0.076f, 1});
        draw_(key_ + "Value", valueBox_, editing_ ? GOLD : INK, valueText_.c_str(), 19.0f);
    }

    const auto button_ = [&](const char* key_, const SettingsRect& rect_, const char* text_, bool available_, bool primary_ = false)
    {
        const bool hovered_ = available_ && !mouseCameraTest && rect_.Contains(point_);
        draw_(std::string(key_) + "Frame", rect_, hovered_ || primary_ ? GOLD : BORDER);
        draw_(std::string(key_) + "Fill", {rect_.x + 1, rect_.y + 1, rect_.width - 2, rect_.height - 2},
            hovered_ ? DirectX::XMFLOAT4{0.24f, 0.17f, 0.12f, 1} : DirectX::XMFLOAT4{0.09f, 0.074f, 0.087f, 1});
        draw_(key_, rect_, available_ ? INK : MUTED, text_, 19.0f);
    };
    button_("SettingsSoundTest", SE_BUTTON, "SEを試聴", true);
    button_("SettingsVibrationTest", VIBRATION_BUTTON, "振動を試す", pad_.SupportsRumble());
    draw_("SettingsFooterRule", {64, 593, 504, 1}, BORDER);
    button_("SettingsSave", SAVE_BUTTON, "保存して戻る", true, true);
    button_("SettingsCancel", CANCEL_BUTTON, "キャンセル", true);
    draw_("SettingsHelp", {64, 653, 508, 25}, saveFailed ? DirectX::XMFLOAT4{1.0f, 0.54f, 0.44f, 1} : MUTED,
        saveFailed ? "設定を保存できませんでした" : "数値欄を選択して入力  /  Enterで確定", 16.0f);

    if (previewCameraObject)
    {
        draw_("CameraHeadingBack", {650, 42, 266, 44}, {0.045f, 0.039f, 0.054f, 0.9f});
        draw_("CameraHeading", {650, 42, 266, 44}, INK, "TPSカメラ試操作", 22.0f);
        draw_("CameraGuideBack", {650, 591, 565, 74}, {0.045f, 0.039f, 0.054f, 0.92f});
        button_("SettingsMouseCamera", MOUSE_CAMERA_BUTTON,
            mouseCameraTest ? "Esc / B・○：マウス解除" : "マウスで試す", !mouseCameraTest);
        draw_("CameraControllerGuide", {940, 603, 265, 48}, INK, "右スティック：視点操作", 18.0f);
    }
    else
    {
        draw_("CameraUnavailable", {650, 42, 530, 44}, MUTED, "TPSカメラを読み込めませんでした", 20.0f);
    }
    if (pad_.IsConnected())
    {
        draw_("SettingsPadHelp", {648, 674, 572, 21}, INK, pad_.IsPlayStation() ?
            "方向キー：調整  /  □：試聴  /  △：振動  /  OPTIONS：保存  /  ○：戻る" :
            "方向キー：調整  /  X：試聴  /  Y：振動  /  START：保存  /  B：戻る", 15.0f);
    }
    sprites_.RenderAll(graphics_.GetContext());
}

void TitleSettingsComponent::Close(bool save_)
{
    if (save_ && !GameRuntimeSettings::SaveUserPreferences())
    {
        saveFailed = true;
        return;
    }
    if (!save_)
    {
        GameRuntimeSettings::SetUserPreferences(previousPreferences);
        AudioManager::Instance().ApplyUserVolumes();
    }
    open = false;
    mouseCameraTest = false;
    MouseInput::Instance().SetCursorLockEnabled(false);
    ControllerInput::Instance().StopRumble();
    if (previewCameraObject)
    {
        previewCameraObject->SetManager(nullptr);
        previewCameraObject.reset();
    }
}

void TitleSettingsComponent::OnDestroy()
{
    if (open)
    {
        Close(false);
    }
}

#include "PrecompiledHeader.h"

#include "FovDirector.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Camera/CameraComponent.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/System/TimeManager.h"

#include <algorithm>
#include <imgui.h>

void FovDirector::Start()
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;

    const std::shared_ptr<CameraComponent> camera_ =
        owner_->GetComponent<CameraComponent>();

    if (camera_)
    {
        // シーン開始時のカメラ FOV を、演出終了後に戻す基準値として保持する。
        defaultFov = camera_->GetFovY();
    }

    const std::shared_ptr<EasingComponent> easing_ =
        owner_->GetOrAddComponent<EasingComponent>();

    if (!easing_) return;

    // ヒットストップやスロー演出中でも FOV 演出は止めたくないので UnscaledTime を使う。
    easing_->SetUseUnscaledTime(true);
    easingIndex = easing_->AddEasingAndGetIndex();

    ResetToDefault();
    RegisterActions();
}

void FovDirector::Update(float elapsedTime_)
{
    BS_PROFILE_SCOPE("FovDirector::Update");
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    const std::shared_ptr<EasingComponent> easing_ = owner_
        ? owner_->GetComponent<EasingComponent>()
        : nullptr;

    if (!easing_ || phase == PHASE::IDLE) return;
    const auto camera_ = owner_->GetComponent<CameraComponent>();
    if (!camera_ || !camera_->CanPlayEventAction())
    {
        // 優先度が戻ったカメラでは、進行中のFOV演出も終了する。
        ResetToDefault();
        return;
    }

    if (phase == PHASE::HOLD)
    {
        const std::shared_ptr<TimeManager> timeManager_ =
            owner_->GetManager().GetTimeManager();

        // HOLD 時間も TimeScale の影響を受けないよう RawDelta を優先する。
        holdElapsed += timeManager_
            ? timeManager_->GetLastRawDeltaTime()
            : elapsedTime_;

        if (holdElapsed >= holdDuration)
        {
            BeginTransitionToDefault();
        }

        return;
    }

    ApplyInterpolated(
        easing_->GetRatio(easingIndex),
        easing_->GetValue(easingIndex));

    if (!easing_->GetEndFlag(easingIndex)) return;

    if (phase == PHASE::TO_TARGET)
    {
        phase = PHASE::HOLD;
        holdElapsed = 0.0f;

        // Holdが0なら即戻す
        if (holdDuration <= 0.0f)
        {
            BeginTransitionToDefault();
        }
    }
    else if (phase == PHASE::TO_DEFAULT)
    {
        ResetToDefault();
    }
}

void FovDirector::PlayFov(const FovWayPoint& wayPoint_)
{
    PlayFov(wayPoint_, wayPoint_.easing);
}

void FovDirector::PlayFov(
    const FovWayPoint& wayPoint_,
    EasingType type_)
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;

    const std::shared_ptr<CameraComponent> camera_ =
        owner_->GetComponent<CameraComponent>();

    const std::shared_ptr<EasingComponent> easing_ =
        owner_->GetOrAddComponent<EasingComponent>();

    if (!camera_ || !easing_ || !camera_->CanPlayEventAction()) return;

    startFov = camera_->GetFovY();
    targetFov = wayPoint_.fov;

    // 負の時間設定は扱いづらいので、再生前に 0 以上へ丸める。
    holdDuration = (std::max)(wayPoint_.holdDuration, 0.0f);
    returnDuration = (std::max)(wayPoint_.returnDuration, 0.0f);

    peakRate = std::clamp(wayPoint_.peakRate, 0.0f, 1.0f);
    peakValue = wayPoint_.peakValue;

    activeEasing = type_ == EasingType::None
        ? EasingType::Lerp
        : type_;

    // EasingComponent には 0→1 の進行だけを持たせ、FOV 値への変換は ApplyInterpolated で行う。
    easing_->SetStartValue(easingIndex, 0.0f);
    easing_->PlayEasing(
        easingIndex,
        activeEasing,
        1.0f / (std::max)(wayPoint_.duration, 0.0001f),
        1.0f);

    phase = PHASE::TO_TARGET;
    holdElapsed = 0.0f;
}

void FovDirector::ApplyInterpolated(
    float ratio_,
    float easedRatio_)
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    const std::shared_ptr<CameraComponent> camera_ = owner_
        ? owner_->GetComponent<CameraComponent>()
        : nullptr;

    if (!camera_) return;

    const float ratioClamped_ = std::clamp(ratio_, 0.0f, 1.0f);
    const float easedRatioClamped_ = std::clamp(easedRatio_, 0.0f, 1.0f);

    float currentFov_ = targetFov;

    // Peak補間はTO_TARGET時だけ使う
    if (phase == PHASE::TO_TARGET && peakRate > 0.0f && peakRate < 1.0f)
    {
        if (ratioClamped_ < peakRate)
        {
            currentFov_ = Lerp(
                startFov,
                peakValue,
                ratioClamped_ / peakRate);
        }
        else
        {
            currentFov_ = Lerp(
                peakValue,
                targetFov,
                (ratioClamped_ - peakRate) / (1.0f - peakRate));
        }
    }
    else
    {
        currentFov_ = Lerp(startFov, targetFov, easedRatioClamped_);
    }

    camera_->SetFov(currentFov_);
}

void FovDirector::BeginTransitionToDefault()
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;

    const std::shared_ptr<CameraComponent> camera_ =
        owner_->GetComponent<CameraComponent>();

    const std::shared_ptr<EasingComponent> easing_ =
        owner_->GetComponent<EasingComponent>();

    if (!camera_ || !easing_)
    {
        ResetToDefault();
        return;
    }

    startFov = camera_->GetFovY();
    targetFov = defaultFov;

    // 戻りではPeak補間を使わない
    peakRate = 0.0f;
    peakValue = 0.0f;

    if (returnDuration <= 0.0f)
    {
        // 戻り時間が 0 の場合は補間スロットを回さず、即座に既定値へ戻す。
        ResetToDefault();
        return;
    }

    easing_->SetStartValue(easingIndex, 0.0f);
    easing_->PlayEasing(
        easingIndex,
        activeEasing,
        1.0f / (std::max)(returnDuration, 0.0001f),
        1.0f);

    phase = PHASE::TO_DEFAULT;
}

void FovDirector::ResetToDefault()
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    const std::shared_ptr<CameraComponent> camera_ = owner_
        ? owner_->GetComponent<CameraComponent>()
        : nullptr;

    if (camera_)
    {
        camera_->SetFov(defaultFov);
    }

    phase = PHASE::IDLE;
    holdElapsed = 0.0f;
}

float FovDirector::Lerp(
    float from_,
    float to_,
    float ratio_)
{
    const float ratioClamped_ = std::clamp(ratio_, 0.0f, 1.0f);
    return from_ + (to_ - from_) * ratioClamped_;
}

void FovDirector::Gui()
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    const std::shared_ptr<CameraComponent> camera_ = owner_
        ? owner_->GetComponent<CameraComponent>()
        : nullptr;

    if (!camera_)
    {
        ImGui::Text("CameraComponent not found!");
        return;
    }

    ImGui::DragFloat("Default FOV", &defaultFov, 0.5f, 1.0f, 170.0f);

    if (ImGui::Button("Capture Current FOV"))
    {
        CaptureCurrentState();
        RegisterActions();
    }

    ImGui::SameLine();

    if (ImGui::Button("Reset FOV"))
    {
        ResetToDefault();
    }

    ImGui::Separator();

    for (int index_ = 0; index_ < static_cast<int>(editorWaypoints.size()); ++index_)
    {
        ImGui::PushID(index_);

        FovWayPoint& wayPoint_ = editorWaypoints[index_];

        const std::string label_ =
            "[" + std::to_string(index_) + "] FOV Waypoint";

        if (ImGui::TreeNode(label_.c_str()))
        {
            ImGui::DragFloat("Target FOV", &wayPoint_.fov, 0.5f, 1.0f, 170.0f);
            ImGui::DragFloat("Time To Target", &wayPoint_.duration, 0.01f, 0.0f, 60.0f);
            ImGui::DragFloat("Hold", &wayPoint_.holdDuration, 0.01f, 0.0f, 60.0f);
            ImGui::DragFloat("Time To Default", &wayPoint_.returnDuration, 0.01f, 0.0f, 60.0f);

            if (ImGui::TreeNode("Special Peak Settings"))
            {
                ImGui::SliderFloat("Peak Rate", &wayPoint_.peakRate, 0.0f, 1.0f);
                ImGui::DragFloat("Peak Value", &wayPoint_.peakValue, 0.5f, 1.0f, 170.0f);
                ImGui::TreePop();
            }

            if (ImGui::Button("Play"))
            {
                PlayFov(wayPoint_);
            }

            ImGui::SameLine();

            if (ImGui::Button("Remove"))
            {
                // 削除後は Fov_番号 の対応が変わるため、Action を登録し直す。
                editorWaypoints.erase(editorWaypoints.begin() + index_--);
                RegisterActions();
                ImGui::TreePop();
                ImGui::PopID();
                continue;
            }

            ImGui::TreePop();
        }

        ImGui::PopID();
    }
}

void FovDirector::RegisterActions()
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;

    for (size_t index_ = 0; index_ < editorWaypoints.size(); ++index_)
    {
        // ActionSequence から "Camera/Fov_0" のように呼べるよう、番号付き Action を公開する。
        owner_->RegisterGlobalAction(
            "Camera",
            "Fov_" + std::to_string(index_),
            [this, index_]()
            {
                if (index_ < editorWaypoints.size())
                {
                    PlayFov(editorWaypoints[index_]);
                }
            });
    }
}

void FovDirector::Serialize(nlohmann::json& json_) const
{
    json_["DefaultFov"] = defaultFov;
    json_["FovWaypoints"] = nlohmann::json::array();

    for (const FovWayPoint& wayPoint_ : editorWaypoints)
    {
        json_["FovWaypoints"].push_back(
            {
                { "fov", wayPoint_.fov },
                { "dur", wayPoint_.duration },
                { "holdDur", wayPoint_.holdDuration },
                { "returnDur", wayPoint_.returnDuration },
                { "pr", wayPoint_.peakRate },
                { "pv", wayPoint_.peakValue },
                { "ease", static_cast<int>(wayPoint_.easing) }
            });
    }
}

void FovDirector::Deserialize(const nlohmann::json& json_)
{
    defaultFov = json_.value("DefaultFov", defaultFov);

    editorWaypoints.clear();

    if (json_.contains("FovWaypoints") && json_["FovWaypoints"].is_array())
    {
        for (const nlohmann::json& item_ : json_["FovWaypoints"])
        {
            FovWayPoint wayPoint_;

            wayPoint_.fov = item_.value("fov", 60.0f);

            // 既存データとの互換用にdur/pr/pvはそのまま読む
            wayPoint_.duration = item_.value("dur", 1.0f);
            wayPoint_.holdDuration = item_.value("holdDur", 0.0f);
            wayPoint_.returnDuration = item_.value("returnDur", 1.0f);
            wayPoint_.peakRate = item_.value("pr", 0.0f);
            wayPoint_.peakValue = item_.value("pv", 0.0f);

            wayPoint_.easing = static_cast<EasingType>(
                item_.value("ease", static_cast<int>(EasingType::Lerp)));

            editorWaypoints.push_back(wayPoint_);
        }
    }

    ResetToDefault();
    RegisterActions();
}

void FovDirector::CaptureCurrentState()
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    const std::shared_ptr<CameraComponent> camera_ = owner_
        ? owner_->GetComponent<CameraComponent>()
        : nullptr;

    if (!camera_) return;

    FovWayPoint wayPoint_;
    // 現在のカメラ FOV を WayPoint として取り込み、エディタ調整の初期値にする。
    wayPoint_.fov = camera_->GetFovY();
    wayPoint_.duration = 1.0f;
    wayPoint_.holdDuration = 0.0f;
    wayPoint_.returnDuration = 1.0f;
    wayPoint_.peakRate = 0.0f;
    wayPoint_.peakValue = 0.0f;
    wayPoint_.easing = EasingType::Lerp;

    editorWaypoints.push_back(wayPoint_);
}

void FovDirector::PlayFovByIndex(
    int index_,
    EasingType type_)
{
    if (index_ < 0 ||
        index_ >= static_cast<int>(editorWaypoints.size()))
    {
        return;
    }

    PlayFov(editorWaypoints[index_], type_);
}

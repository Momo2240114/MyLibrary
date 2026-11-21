#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/Math/EasingComponent.h"

#include <queue>
#include <functional>

#include "Engine/GamePlay/Transform/transformDirector.h"
#include "FovDirector.h"
#include "Engine/GamePlay/EventSystem/IActionProvider.h"


class CameraComponent;

class CameraDirector : public Component ,public  IActionProvider
{
public:
    const std::string& GetName() const override {
        static const std::string name = "CameraDirector";
        return name;
    }

    void Start() override;
    void Update(float et)override;
    void Gui() override;

    // 両方のDirectorに同時に命令を出す
    void PlayCombined(int index, EasingType transEase = EasingType::OutExpo, EasingType fovEase = EasingType::Lerp);

    // シリアライズ機能
    void Serialize(nlohmann::json & j) const;
    void Deserialize(const nlohmann::json & j);

    void RegisterActions();

private:
    struct CombinedWaypoint {
        int transformIdx; // TransformDirector内の何番目か
        int fovIdx;       // FovDirector内の何番目か

        // --- 追加：アクションごとの戻り設定 ---
        bool m_autoReturn = false;
        float m_waitTime = 0.0f;
    };

    std::shared_ptr<TransformDirector> transformDirector;
    std::shared_ptr<FovDirector> fovDirector;
    std::vector<CombinedWaypoint> combinedWaypoints;

    void CaptureCombinedWaypoint();

};
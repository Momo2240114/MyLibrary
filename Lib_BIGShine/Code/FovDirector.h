#pragma once

#include "ComponentBase.h"
#include "EasingComponent.h"

struct FovWayPoint {
    float fov;
    float duration = 1.0f;
    float peakRate = 0.0f;
    float peakValue = 0.0f;
};

class FovDirector : public Component {
public:
    const std::string& GetName() const override { static std::string n = "FovDirector"; return n; }

    void Start() override;

    void Update(float et) override;

    void PlayFov(const FovWayPoint& wp, EasingType type);

    void Gui() override;
    void Serialize(nlohmann::json& j) const;
    void Deserialize(const nlohmann::json& j);
    // --- êe(CameraDirector)Ç©ÇÁåƒÇŒÇÍÇÈÉwÉãÉpÅ[ä÷êî ---
    int GetCount() const { return (int)EditorWaypoints.size(); }

    void CaptureCurrentState();
    void PlayFovByIndex(int idx, EasingType type);
private:
    int easingIndex = -1;
    bool isMoving = false;
    float startFov = 60.0f;
    FovWayPoint targetData;
    std::vector<FovWayPoint> EditorWaypoints;
    float Lerp(float a, float b, float t) { return a + (b - a) * t; }
};
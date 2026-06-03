#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/Graphics/RenderSystem/BloomSystem.h"
#include "Engine/Math/EasingComponent.h"
#include "Engine/GamePlay/EventSystem/IActionProvider.h"

struct ColorFilterWayPoint {
    BloomSystem::CbColorFilter filter;
    float duration;
    EasingType easing;
};

class ColorFilterDirector : public Component, public IActionProvider
{
public:
    const std::string& GetName() const override { 
        static std::string n = "ColorFilterDirector";
        return n; 
    }

    void Start() override;
    void Update(float et) override;
    void PlayFilter(const ColorFilterWayPoint& wp);
    void Gui() override;
    void Serialize(nlohmann::json& j) const;
    void Deserialize(const nlohmann::json& j);

    void RegisterActions();

private:
    int easingIndex = -1;
    bool isChanging = false;

    // アニメーション用変数
    BloomSystem::CbColorFilter startParams;
    BloomSystem::CbColorFilter targetParams;

    std::vector<ColorFilterWayPoint> EditorWaypoints;

    // floatおよびDirectX::XMFLOAT3の補完ヘルパー
    float Lerp(float a, float b, float t) { return a + (b - a) * t; }
    DirectX::XMFLOAT3 LerpVec3(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b, float t) {
        return { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t };
    }
};
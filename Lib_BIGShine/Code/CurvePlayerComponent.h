#pragma once
#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "CurveAssetComponent.h"
#include <imgui.h>

class CurvePlayerComponent : public Component
{
public:
    float value = 0;
    float time = 0.0f;
    float speed = 1.0f;
    bool playing = true;

    std::weak_ptr<CurveAssetComponent> asset;

    const std::string& GetName() const override
    {
        static std::string n = "CurvePlayerComponent";
        return n;
    }

    void Start() override;
    void Update(float et) override;

    void ApplyValue(float v);

    void Gui() override
    {
        ImGui::Text("Curve Player Component");
        ImGui::Checkbox("Playing", &playing);
        ImGui::DragFloat("Time", &time, 0.01f);
        ImGui::DragFloat("Speed", &speed, 0.01f);
        ImGui::DragFloat("value", &value, 0.01f);

        if (auto a = asset.lock())
            ImGui::Text("Using Curve: %s", a->curveData.name.c_str());
    }
};

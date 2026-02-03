#pragma once
#include "ComponentBase.h"
#include "GameObject.h"
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

    void Start() override
    {
        if (auto obj = GetGameObject())
            asset = obj->GetComponent<CurveAssetComponent>();
    }

    void Update(float dt) override
    {
        auto a = asset.lock();
        if (!a || !playing) return;

        time += dt * speed;
        value = a->curveData.Evaluate(time);
        ApplyValue(value);
    }

    void ApplyValue(float v)
    {
        if (auto obj = GetGameObject())
        {
            auto t = obj->transform;
            t->localPosition.y = v; // ”CˆÓ—p“r‚É•ÏX‰Â”\
        }
    }

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

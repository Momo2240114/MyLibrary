#include "CurvePlayerComponent.h"

void CurvePlayerComponent::Start()
{
    if (auto obj = GetGameObject())
        asset = obj->GetOrAddComponent<CurveAssetComponent>();
}

void CurvePlayerComponent::Update(float et)
{
    auto a = asset.lock();
    if (!a || !playing) return;

    time += et * speed;
    value = a->curveData.Evaluate(time);
    ApplyValue(value);
}

void CurvePlayerComponent::ApplyValue(float v)
{
}
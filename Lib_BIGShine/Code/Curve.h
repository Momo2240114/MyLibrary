#pragma once
#include "CurveData.h"
#include <imgui.h>

class Curve
{
public:
    CurveData* data = nullptr;

    Curve() = default;
    Curve(CurveData* d) : data(d) {}

    float Evaluate(float t);
    void ShowEditorUI();

};


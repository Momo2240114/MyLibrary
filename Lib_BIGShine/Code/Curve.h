#pragma once
#include "CurveData.h"
#include <imgui.h>

class curvecomp
{
public:
    CurveData* data = nullptr;

    curvecomp() = default;
    curvecomp(CurveData* d) : data(d) {}

    float Evaluate(float t);
    void ShowEditorUI();

};


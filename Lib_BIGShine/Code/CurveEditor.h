#pragma once
#include "CurveData.h"



class CurveEditor
{
public:
    void Draw(CurveData& curve);
    CurveEditorMode mode = CurveEditorMode::FREE;

private:
    void DrawInterpolation(CurveData& curve);
    void DrawKeyList(CurveData& curve);  
    void DrawGraph(CurveData& curve);
    void DrawBezierEditor(CurveData& curve); // Bezier—p
};

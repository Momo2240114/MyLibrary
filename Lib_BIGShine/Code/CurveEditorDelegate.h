#include "CurveEditor.h"
#include "ImCurveEdit.h"

class CurveEditorDelegate : public ImCurveEdit::Delegate
{
public:
    CurveData* curve;

    CurveEditorDelegate(CurveData* c) : curve(c) {}

    virtual size_t GetCurveCount() override { return 1; }
    virtual bool IsVisible(size_t curveIndex) override { return true; }
    virtual ImCurveEdit::CurveType GetCurveType(size_t curveIndex) const override { return ImCurveEdit::CurveSmooth; }

    virtual ImVec2& GetMin() override
    {
        static ImVec2 min = { 0.f, -1.f };
        return min;
    }
    virtual ImVec2& GetMax() override
    {
        static ImVec2 max = { 5.f, 1.f };
        return max;
    }

    virtual size_t GetPointCount(size_t curveIndex) override { return curve ? curve->keys.size() : 0; }
    virtual uint32_t GetCurveColor(size_t curveIndex) override { return 0xFF00FFFF; }

    virtual ImVec2* GetPoints(size_t curveIndex) override
    {
        tempPoints.clear();
        for (auto& k : curve->keys)
            tempPoints.push_back(ImVec2(k.time, k.value));
        return tempPoints.data();
    }

    virtual int EditPoint(size_t curveIndex, int pointIndex, ImVec2 value) override
    {
        if (!curve) return pointIndex;
        curve->keys[pointIndex].time  = value.x;
        curve->keys[pointIndex].value = value.y;
        return pointIndex;
    }

    virtual void AddPoint(size_t curveIndex, ImVec2 value) override
    {
        if (!curve) return;
        curve->keys.push_back({ value.x, value.y, 0.f, 0.f });
    }

private:
    std::vector<ImVec2> tempPoints;
};
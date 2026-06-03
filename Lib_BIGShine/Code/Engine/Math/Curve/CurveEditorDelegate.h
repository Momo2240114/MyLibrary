//#include "CurveEditor.h"
//#include "ImCurveEdit.h"
//
//class CurveEditorDelegate : public ImCurveEdit::Delegate
//{
//public:
//    CurveData* curve;
//
//    CurveEditorDelegate(CurveData* c) : curve(c) {}
//
//    virtual size_t GetCurveCount() override { return 1; }
//    virtual bool IsVisible(size_t curveIndex) override { return true; }
//    virtual ImCurveEdit::CurveType GetCurveType(size_t curveIndex) const override { return ImCurveEdit::CurveSmooth; }
//
//    virtual ImVec2& GetMin() override
//    {
//        // yの最小を少し下げる（-1.0f -> -0.2f など）ことで、0のラインに打ったキーが見やすくなる
//        static ImVec2 min = { -0.1f, -0.2f };
//        return min;
//    }
//
//    virtual ImVec2& GetMax() override {
//        static ImVec2 max = { 1.0f, 2.0f }; // X(時間)を1.0、Y(速度)を2.0に固定
//        return max;
//    }
//
//    virtual size_t GetPointCount(size_t curveIndex) override { return curve ? curve->keys.size() : 0; }
//    virtual uint32_t GetCurveColor(size_t curveIndex) override { return 0xFF00FFFF; }
//
//    virtual ImVec2* GetPoints(size_t curveIndex) override
//    {
//        tempPoints.clear();
//        for (auto& k : curve->keys)
//            tempPoints.push_back(ImVec2(k.time, k.value));
//        return tempPoints.data();
//    }
//
//    virtual int EditPoint(size_t curveIndex, int pointIndex, ImVec2 value) override
//    {
//        if (!curve) return pointIndex;
//
//        // --- ここで制限をかける ---
//        // 時間を 0.0 ~ 1.0 の間に強制
//        value.x = std::clamp(value.x, 0.0f, 1.0f);
//        // 必要なら値(value.y)も 0.0 ~ 1.0 に制限（任意）
//        // value.y = std::clamp(value.y, 0.0f, 1.0f); 
//
//        curve->keys[pointIndex].time = value.x;
//        curve->keys[pointIndex].value = value.y;
//
//        // 時間順にソート（前後の入れ替わり対応）
//        // ※ImCurveEditの内部仕様に合わせ、新しいインデックスを返す必要があります
//        // 基本的な実装なら std::sort を呼んで pointIndex を再計算します
//        return pointIndex;
//    }
//    virtual void AddPoint(size_t curveIndex, ImVec2 value) override
//    {
//        if (!curve) return;
//        curve->keys.push_back({ value.x, value.y, 0.f, 0.f });
//    }
//
//private:
//    std::vector<ImVec2> tempPoints;
//};
//#pragma once
//#include <vector>
//#include <string>
//#include <algorithm>
//
//enum class CurveInterpolation
//{
//    Linear,
//    Smooth,
//    Constant
//};
//
//enum class CurveEditorMode {
//    FREE,   // 自由にカーブを作れる
//    BEZIER,  // 0..1 ベジェ用
//    Wip
//};
//
//struct CurveKey
//{
//    float time = 0;
//    float value = 0;
//    float inTangent = 0;   // 左ハンドル傾き
//    float outTangent = 0;  // 右ハンドル傾き
//};
//
//class CurveData
//{
//public:
//    std::string name = "NewCurve";
//    CurveInterpolation interpolation = CurveInterpolation::Smooth;
//    CurveEditorMode mode = CurveEditorMode::FREE;  // モード追加
//    std::vector<CurveKey> keys;
//
//    // Bezier 用
//    float P[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
//
//    //---------------------------------------------
//    // 時間に対して値を返す（Unity の AnimationCurve.Evaluate）
//    //---------------------------------------------
//    float Evaluate(float t) const
//    {
//        if (keys.empty()) return 0;
//
//        for (size_t i = 0; i < keys.size() - 1; ++i)
//        {
//            const auto& k0 = keys[i];
//            const auto& k1 = keys[i + 1];
//
//            if (t >= k0.time && t <= k1.time)
//            {
//                float dt = (t - k0.time) / (k1.time - k0.time);
//
//                switch (interpolation)
//                {
//                    case CurveInterpolation::Linear:
//                        return k0.value + (k1.value - k0.value) * dt;
//
//                    case CurveInterpolation::Constant:
//                        return k0.value;
//
//                    case CurveInterpolation::Smooth:
//                    default:
//                        return HermiteSafe(k0, k1, dt);
//                }
//            }
//        }
//        return keys.back().value;
//    }
//
//    float EvaluateNormalized(float tNormalized) const
//    {
//        if (keys.empty()) return 0;
//
//        // 正規化時間を実際の時間に変換
//        float start = keys.front().time;
//        float end = keys.back().time;
//        float t = start + (end - start) * tNormalized;
//
//        return Evaluate(t);
//    }
//
//private:
//    //---------------------------------------------
//    // Hermite補間による滑らかカーブ
//    //---------------------------------------------
//    float Hermite(const CurveKey& k0, const CurveKey& k1, float t) const
//    {
//        float t2 = t * t;
//        float t3 = t2 * t;
//
//        float h00 = 2*t3 - 3*t2 + 1;
//        float h10 = t3 - 2*t2 + t;
//        float h01 = -2*t3 + 3*t2;
//        float h11 = t3 - t2;
//
//        return h00 * k0.value +
//               h10 * k0.outTangent +
//               h01 * k1.value +
//               h11 * k1.inTangent;
//    }
//
//    float HermiteSafe(const CurveKey& k0, const CurveKey& k1, float t) const
//    {
//        float t2 = t * t;
//        float t3 = t2 * t;
//
//        float h00 = 2 * t3 - 3 * t2 + 1;
//        float h10 = t3 - 2 * t2 + t;
//        float h01 = -2 * t3 + 3 * t2;
//        float h11 = t3 - t2;
//
//        float value = h00 * k0.value +
//            h10 * k0.outTangent +
//            h01 * k1.value +
//            h11 * k1.inTangent;
//
//        // クランプしてオーバーシュートを防ぐ
//        float minVal = (std::min)(k0.value, k1.value);
//        float maxVal = (std::max)(k0.value, k1.value);
//        return (std::clamp)(value, minVal, maxVal);
//    }
//};

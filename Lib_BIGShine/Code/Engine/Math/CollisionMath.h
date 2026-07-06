#pragma once

#include "DirectXMath.h"
#include "Engine/Physics/ColliderComponent.h"
#include "Engine/Physics/Collision.h"

class CollisionMath {
public:
    // --- 補助関数 ---
    static float Clamp(float n, float min, float max) { return (std::max)(min, (std::min)(n, max)); }

    static DirectX::XMVECTOR ClosestPointOnSegment(DirectX::XMVECTOR P, DirectX::XMVECTOR A, DirectX::XMVECTOR B)
    {
        DirectX::XMVECTOR AB = DirectX::XMVectorSubtract(B, A);
        float t = DirectX::XMVectorGetX(DirectX::XMVector3Dot(DirectX::XMVectorSubtract(P, A), AB)) /
            DirectX::XMVectorGetX(DirectX::XMVector3Dot(AB, AB));
        return DirectX::XMVectorAdd(A, DirectX::XMVectorScale(AB, Clamp(t, 0.0f, 1.0f)));
    }

    // --- 1. 球 vs 球 ---
    static CollisionManifold SphereVsSphere(const Sphere& a, const Sphere& b) {
        CollisionManifold m;
        DirectX::XMVECTOR diff = DirectX::XMVectorSubtract(b.center, a.center);
        float dist = DirectX::XMVectorGetX(DirectX::XMVector3Length(diff));
        float radiusSum = a.radius + b.radius;
        if (dist < radiusSum) {
            m.isHit = true;
            m.depth = radiusSum - dist;
            m.normal = (dist > 0.0001f) ? DirectX::XMVector3Normalize(diff) : DirectX::XMVectorSet(0, 1, 0, 0);
        }
        return m;
    }

    // --- 2. OBB vs 球 ---
    static CollisionManifold OBBVsSphere(const OBB& a, const Sphere& b) {
        CollisionManifold m;
        // 箱のローカル空間に球の中心を変換
        DirectX::XMVECTOR d = DirectX::XMVectorSubtract(b.center, a.center);
        DirectX::XMVECTOR closest = a.center;
        for (int i = 0; i < 3; ++i) {
            float dist = DirectX::XMVectorGetX(DirectX::XMVector3Dot(d, a.axes[i]));
            dist = Clamp(dist, -a.halfSize[i], a.halfSize[i]);
            closest = DirectX::XMVectorAdd(closest, DirectX::XMVectorScale(a.axes[i], dist));
        }
        DirectX::XMVECTOR diff = DirectX::XMVectorSubtract(b.center, closest);
        float distSq = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(diff));
        if (distSq < b.radius * b.radius) {
            float dLen = sqrtf(distSq);
            m.isHit = true;
            m.depth = b.radius - dLen;
            m.normal = (dLen > 0.0001f) ? DirectX::XMVector3Normalize(diff) : DirectX::XMVectorSet(0, 1, 0, 0);
        }
        return m;
    }

    // --- 3. OBB vs OBB (分離軸定理 SAT) ---
    static CollisionManifold OBBVsOBB(const OBB& a, const OBB& b) {
        CollisionManifold m;
        m.depth = FLT_MAX;
        DirectX::XMVECTOR toCenter = DirectX::XMVectorSubtract(b.center, a.center);

        // 判定すべき15軸のリストを作成
        std::vector<DirectX::XMVECTOR> testAxes;
        for (int i = 0; i < 3; ++i) { testAxes.push_back(a.axes[i]); testAxes.push_back(b.axes[i]); }
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                testAxes.push_back(DirectX::XMVector3Cross(a.axes[i], b.axes[j]));
            }
        }

        for (auto& axis : testAxes) {
            float lenSq = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(axis));
            if (lenSq < 0.001f) continue;
            DirectX::XMVECTOR L = DirectX::XMVector3Normalize(axis);

            // 投影範囲の計算
            auto GetProjectedRadius = [](const OBB& obb, DirectX::XMVECTOR L) {
                float r = 0;
                for (int i = 0; i < 3; ++i)
                    r += obb.halfSize[i] * fabsf(DirectX::XMVectorGetX(DirectX::XMVector3Dot(obb.axes[i], L)));
                return r;
                };

            float ra = GetProjectedRadius(a, L);
            float rb = GetProjectedRadius(b, L);
            float d = fabsf(DirectX::XMVectorGetX(DirectX::XMVector3Dot(toCenter, L)));

            float overlap = ra + rb - d;
            if (overlap <= 0) return { false }; // 隙間があれば即終了

            if (overlap < m.depth) {
                m.depth = overlap;
                m.normal = L;
            }
        }
        m.isHit = true;
        if (DirectX::XMVectorGetX(DirectX::XMVector3Dot(toCenter, m.normal)) < 0)
            m.normal = DirectX::XMVectorNegate(m.normal);
        return m;
    }

    // --- 4. カプセル vs 球 ---
    static CollisionManifold CapsuleVsSphere(const Capsule& a, const Sphere& b) {
        Sphere tempSphere = { ClosestPointOnSegment(b.center, a.p0, a.p1), a.radius };
        return SphereVsSphere(tempSphere, b);
    }

    // --- 5. カプセル vs カプセル ---
    static CollisionManifold CapsuleVsCapsule(const Capsule& a, const Capsule& b) {
        using namespace DirectX;

        XMVECTOR u = XMVectorSubtract(a.p1, a.p0); // 線分Aの方向
        XMVECTOR v = XMVectorSubtract(b.p1, b.p0); // 線分Bの方向
        XMVECTOR w = XMVectorSubtract(a.p0, b.p0);

        float sc, tc;
        float a_val = XMVectorGetX(XMVector3Dot(u, u));
        float b_val = XMVectorGetX(XMVector3Dot(u, v));
        float c_val = XMVectorGetX(XMVector3Dot(v, v));
        float d_val = XMVectorGetX(XMVector3Dot(u, w));
        float e_val = XMVectorGetX(XMVector3Dot(v, w));
        float D = a_val * c_val - b_val * b_val;

        // パラメータ sc, tc を計算（線分上の位置 0.0~1.0）
        if (D < 0.0001f) { // 平行な場合
            sc = 0.0f;
            tc = (b_val > c_val ? d_val / b_val : e_val / c_val);
        }
        else {
            sc = (b_val * e_val - c_val * d_val) / D;
            tc = (a_val * e_val - b_val * d_val) / D;
        }

        sc = Clamp(sc, 0.0f, 1.0f);
        tc = Clamp(tc, 0.0f, 1.0f);

        // 最短点 P_a, P_b を確定
        XMVECTOR Pa = XMVectorAdd(a.p0, XMVectorScale(u, sc));
        XMVECTOR Pb = XMVectorAdd(b.p0, XMVectorScale(v, tc));

        // 球 vs 球として判定
        return SphereVsSphere({ Pa, a.radius }, { Pb, b.radius });
    }

    // --- 6. OBB vs カプセル ---
    static CollisionManifold OBBVsCapsule(const OBB& a, const Capsule& b) {
        using namespace DirectX;

        // 1. カプセルの線分をOBBのローカル空間に変換
        auto TransformToLocal = [&](XMVECTOR worldPos) {
            XMVECTOR rel = XMVectorSubtract(worldPos, a.center);
            return XMVectorSet(
                XMVectorGetX(XMVector3Dot(rel, a.axes[0])),
                XMVectorGetX(XMVector3Dot(rel, a.axes[1])),
                XMVectorGetX(XMVector3Dot(rel, a.axes[2])), 0);
            };

        XMVECTOR localP0 = TransformToLocal(b.p0);
        XMVECTOR localP1 = TransformToLocal(b.p1);

        // 2. ローカル空間(AABB)における線分上の最近傍点Qを求める
        // 線分を細かく分割して判定するか、厳密な最短距離計算を行います。
        // ここでは実用的な精度を出すため、線分上の最近傍点を探索。
        XMVECTOR bestQ = localP0;
        float minDistSq = FLT_MAX;

        // 線分を分割してAABBに最も近い点を探す（簡易実装：ステップ数を増やすと精度UP）
        const int SAMPLES = 4;
        for (int i = 0; i <= SAMPLES; ++i) {
            XMVECTOR P = XMVectorLerp(localP0, localP1, (float)i / SAMPLES);
            // AABB(中心0, halfSize)へのクランプ
            XMVECTOR Q = XMVectorSet(
                Clamp(XMVectorGetX(P), -a.halfSize[0], a.halfSize[0]),
                Clamp(XMVectorGetY(P), -a.halfSize[1], a.halfSize[1]),
                Clamp(XMVectorGetZ(P), -a.halfSize[2], a.halfSize[2]), 0);

            float dSq = XMVectorGetX(XMVector3LengthSq(XMVectorSubtract(P, Q)));
            if (dSq < minDistSq) { minDistSq = dSq; bestQ = Q; }
        }

        // 3. ローカルの最短点Qをワールドに戻す
        XMVECTOR worldQ = a.center;
        for (int i = 0; i < 3; ++i) worldQ = XMVectorAdd(worldQ, XMVectorScale(a.axes[i], XMVectorGetByIndex(bestQ, i)));

        // 4. カプセルの線分上でworldQに最も近い点Paを求め、球判定
        XMVECTOR Pa = ClosestPointOnSegment(worldQ, b.p0, b.p1);

        CollisionManifold m = SphereVsSphere({ worldQ, 0.0f }, { Pa, b.radius });


        // OBB側の厚みはないので、最後にカプセル半径分だけdepthを調整
        // (※厳密にはOBB内の最近傍点Qを球とする判定)
        return m;
    }
};
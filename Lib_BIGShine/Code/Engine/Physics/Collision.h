#pragma once

#include "DirectXMath.h"

class GameObject;

struct CollisionManifold {
    bool isHit = false;
    DirectX::XMVECTOR normal; // 押し出し方向 (Aから見たBへの方向)
    float depth = 0.0f;       // 重なり量
};

struct CollisionContact
{
    DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 normal = { 0.0f, 1.0f, 0.0f };
    bool isValid = false;
};

// 判定用の簡略化データ構造
struct Sphere
{
    DirectX::XMVECTOR center;
    float radius;
};

struct OBB
{
    DirectX::XMVECTOR center;
    DirectX::XMVECTOR axes[3];
    float halfSize[3];
};

struct Capsule
{
    DirectX::XMVECTOR p0;
    DirectX::XMVECTOR p1;
    float radius;
};
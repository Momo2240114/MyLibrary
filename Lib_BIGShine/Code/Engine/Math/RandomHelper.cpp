
#include"RandomHelper.h"
#include <cmath>
#include <DirectXMath.h>
#include <cmath>
float RandomRangeCreat(float min, float max)
{
	// 0.0～1.0の間までのランダム値
	float value = static_cast<float>(rand()) / RAND_MAX;

	// min～maxまでのランダム値に変換
	return min + (max - min) * value;
}

DirectX::XMFLOAT3 QuaternionToEuler(DirectX::XMVECTOR q)
{
    DirectX::XMFLOAT4 Q;
    DirectX::XMStoreFloat4(&Q, q);

    DirectX::XMFLOAT3 euler;

    // 1. Pitch (X軸) - 真ん中の回転なので asin で計算
    // Y -> X -> Z の順序では、X成分と、Y*Zの組み合わせで判定します
    double sinp = 2.0 * (Q.w * Q.x - Q.z * Q.y);

    // ジンバルロック対策 (±90度)
    if (std::abs(sinp) >= 1.0)
        euler.x = (float)std::copysign(DirectX::XM_PIDIV2, sinp);
    else
        euler.x = (float)std::asin(sinp);

    // 2. Yaw (Y軸) - 最初の回転
    double siny_cosr = 2.0 * (Q.w * Q.y + Q.x * Q.z);
    double cosy_cosr = 1.0 - 2.0 * (Q.x * Q.x + Q.y * Q.y);
    euler.y = (float)std::atan2(siny_cosr, cosy_cosr);

    // 3. Roll (Z軸) - 最後の回転
    double sinr_cosp = 2.0 * (Q.w * Q.z + Q.x * Q.y);
    double cosr_cosp = 1.0 - 2.0 * (Q.x * Q.x + Q.z * Q.z);
    euler.z = (float)std::atan2(sinr_cosp, cosr_cosp);

    return euler;
}


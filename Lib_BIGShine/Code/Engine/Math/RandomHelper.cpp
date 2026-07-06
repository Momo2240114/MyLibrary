#include "RandomHelper.h"

#include <cmath>
#include <DirectXMath.h>

float RandomRangeCreate(float min_, float max_)
{
    // randの結果を0.0～1.0へ正規化し、指定された範囲へ線形変換する。
    float normalizedValue_ = static_cast<float>(rand()) / RAND_MAX;
    return min_ + (max_ - min_) * normalizedValue_;
}

DirectX::XMFLOAT3 QuaternionToEuler(DirectX::XMVECTOR quaternion_)
{
    DirectX::XMFLOAT4 quaternionElements_;
    DirectX::XMStoreFloat4(&quaternionElements_, quaternion_);

    DirectX::XMFLOAT3 eulerAngles_;

    // Y-X-Z回転順の中央軸となるX回転を算出する。
    double sinPitch_ = 2.0 * (quaternionElements_.w * quaternionElements_.x
        - quaternionElements_.z * quaternionElements_.y);

    // 誤差でasinの定義域を超える場合は、ジンバルロック位置の±90度へ固定する。
    if (std::abs(sinPitch_) >= 1.0)
    {
        eulerAngles_.x = static_cast<float>(std::copysign(DirectX::XM_PIDIV2, sinPitch_));
    }
    else
    {
        eulerAngles_.x = static_cast<float>(std::asin(sinPitch_));
    }

    // 最初のY回転と最後のZ回転はatan2で象限を維持したまま算出する。
    double sinYawCosPitch_ = 2.0 * (quaternionElements_.w * quaternionElements_.y
        + quaternionElements_.x * quaternionElements_.z);
    double cosYawCosPitch_ = 1.0 - 2.0 * (quaternionElements_.x * quaternionElements_.x
        + quaternionElements_.y * quaternionElements_.y);
    eulerAngles_.y = static_cast<float>(std::atan2(sinYawCosPitch_, cosYawCosPitch_));

    double sinRollCosPitch_ = 2.0 * (quaternionElements_.w * quaternionElements_.z
        + quaternionElements_.x * quaternionElements_.y);
    double cosRollCosPitch_ = 1.0 - 2.0 * (quaternionElements_.x * quaternionElements_.x
        + quaternionElements_.z * quaternionElements_.z);
    eulerAngles_.z = static_cast<float>(std::atan2(sinRollCosPitch_, cosRollCosPitch_));

    return eulerAngles_;
}

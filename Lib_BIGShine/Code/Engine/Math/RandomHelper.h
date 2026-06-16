#pragma once

#include<stdlib.h>
#include"DirectXMath.h"

/// 指定範囲内の疑似乱数を生成する。
float RandomRangeCreate(float min_, float max_);

/// クォータニオンをY-X-Z回転順のオイラー角へ変換する。
DirectX::XMFLOAT3 QuaternionToEuler(DirectX::XMVECTOR quaternion_);

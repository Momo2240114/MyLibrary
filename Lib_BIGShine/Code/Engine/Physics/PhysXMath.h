#pragma once

#include "PhysXSystem.h"
#include <unordered_map>
#include <DirectXMath.h> 

using namespace physx;



inline DirectX::XMFLOAT3 QuaternionToEuler(const physx::PxQuat& q)
{
	DirectX::XMFLOAT3 euler;

	float ysqr = q.y * q.y;

	// roll(X)
	float t0 = +2.0f * (q.w * q.x + q.y * q.z);
	float t1 = +1.0f - 2.0f * (q.x * q.x + ysqr);
	euler.x = std::atan2(t0, t1);

	// pitch(Y)
	float t2 = +2.0f * (q.w * q.y - q.z * q.x);
	t2 = t2 > 1.0f ? 1.0f : t2;
	t2 = t2 < -1.0f ? -1.0f : t2;
	euler.y = std::asin(t2);

	// yaw(Z)
	float t3 = +2.0f * (q.w * q.z + q.x * q.y);
	float t4 = +1.0f - 2.0f * (ysqr + q.z * q.z);
	euler.z = std::atan2(t3, t4);

	return euler;
}


inline DirectX::XMFLOAT4X4 PxTransformToMatrix(const PxTransform& t)
{
	using namespace DirectX;

	// PhysX のクォータニオン → XMMatrix
	XMVECTOR rot = XMVectorSet(t.q.x, t.q.y, t.q.z, t.q.w);
	XMVECTOR pos = XMVectorSet(t.p.x, t.p.y, t.p.z, 1.0f);

	// 回転行列
	XMMATRIX mRot = XMMatrixRotationQuaternion(rot);

	// 平行移動
	XMMATRIX mTrans = XMMatrixTranslationFromVector(pos);

	// rot * trans
	XMMATRIX m = mRot * mTrans;

	XMFLOAT4X4 out;
	XMStoreFloat4x4(&out, m);
	return out;
}

inline physx::PxQuat EulerToPxQuat(const DirectX::XMFLOAT3& eulerDeg)
{
	using namespace DirectX;

	// degrees -> radians
	XMVECTOR eulerRad = XMVectorSet(
		XMConvertToRadians(eulerDeg.x),
		XMConvertToRadians(eulerDeg.y),
		XMConvertToRadians(eulerDeg.z),
		0
	);

	// X,Y,Z回転行列
	XMMATRIX rot = XMMatrixRotationRollPitchYawFromVector(eulerRad);

	// quaternion に変換
	XMVECTOR q = XMQuaternionRotationMatrix(rot);

	// PxQuat に変換
	DirectX::XMFLOAT4 qf;
	XMStoreFloat4(&qf, q);
	return physx::PxQuat(qf.x, qf.y, qf.z, qf.w);
}

inline DirectX::XMFLOAT3 PxVec3ToXMFLOAT3(const physx::PxVec3& v)
{
	return DirectX::XMFLOAT3(v.x, v.y, v.z);
}

#pragma once

#include<vector>
#include"Engine/GamePlay/Core/Object/GameObject.h"
#include<memory>

class RayCastManager
{
public:
	GameObject* RayCast(const Vector3& start, const Vector3& end, uint32_t layerMask, Vector3& hitPosition,Vector3& hitNormal);

	bool RayCast(const Vector3& start, const Vector3& end,GameObject* gameObject,Vector3& hitPosition,Vector3& hitNormal);

	bool IntersectSegmentAABB(const Vector3& start,const Vector3& end,const Vector3& boxMin,const Vector3& boxMax,float& t);

	void AddGameObject(std::shared_ptr<GameObject> gameObject); \


	void Destroy(GameObject* gameObject);

	void Test();

	void Clear();

private:
	std::vector<std::weak_ptr<GameObject>> m_pGameObjects;
	std::vector<int> m_WeakCounts;
};
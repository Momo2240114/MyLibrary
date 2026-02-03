#include "RayCastManager.h"

#include"BoxCollider.h"

GameObject* RayCastManager::RayCast(const Vector3& start,const Vector3& end ,uint32_t layerMask,Vector3& hitPosition,Vector3& hitNormal)
{
	float min = FLT_MAX;
	Vector3 hit = { 0,0,0 };
	Vector3 normal = {0,0,0};
	GameObject* resultGameObject = nullptr;
	//for (auto& gameObject : m_pGameObjects)
	for(int i = 0;i < m_pGameObjects.size();i++)
	{
		GameObject* g = m_pGameObjects[i].lock().get();
		GameObject::LayerType layer = m_pGameObjects[i].lock()->GetLayer();

		//m_pGameObjects[i].lock()->GetComponent<BoxCollider>()->m_DebugPrimitiveColor = {0,0,0,1};
		m_WeakCounts[i] = m_pGameObjects[i].use_count();
		//もしも同じレイヤーなら
		if ((uint32_t)layer & (uint32_t)layerMask)
		{
			 bool result = RayCast(start,end, m_pGameObjects[i].lock().get(),hitPosition,hitNormal);

			 if (result)
			 {
				 //長さを求めて最接近点だけを求める
				 Vector3 length = start - hitPosition;
				 float len = length.Length();
				 if (len < min)
				 {
					 min = len;
					 hit = hitPosition;
					 normal = {0,0,0};
					 resultGameObject = m_pGameObjects[i].lock().get();
				 }
			 }
		}
	}

	hitPosition = hit;
	hitNormal = normal;


	if (resultGameObject != nullptr)
	{
		resultGameObject->GetComponent<BoxCollider>()->m_DebugPrimitiveColor = { 1,0,1,1 };
	}
	return resultGameObject;
}

bool RayCastManager::RayCast(const Vector3& start, const Vector3& end, GameObject* gameObject ,Vector3& hitPosition,Vector3& hitNormal)
{
	bool hit = false;
	const Matrix& worldMatrix = gameObject->transform->GetWorldMatrix();

	//視点と終点からレイののベクトルと長さを求める
	Vector3 worldRayStart = start;
	Vector3 worldRayEnd = end;
	Vector3 worldRayVec = worldRayEnd - worldRayStart;
	float nearestDist = worldRayVec.Length();
	if (nearestDist <= 0.0f) return false;

	std::weak_ptr<BoxCollider> boxCollider = gameObject->GetComponent<BoxCollider>();
	if (boxCollider.lock() == nullptr)
	{
		return false;
	}

	//コリジョン内のすべてのメッシュと交差判定を行う(AABBのみ)
	OBB box = boxCollider.lock()->m_Obb;//OBBと書いているが回転しているオブジェクトに適応するつもりはない

	Vector3 center = box.center;
	Vector3 halfSize;
	halfSize.x = box.halfSize[0];
	halfSize.y = box.halfSize[1];
	halfSize.z = box.halfSize[2];

	Vector3 boxMin = center - halfSize;
	Vector3 boxMax = center + halfSize;
	
	Vector3 angle = gameObject->transform->GetWorldRotation();
	angle.x = DirectX::XMConvertToRadians(angle.x);
	angle.y = DirectX::XMConvertToRadians(angle.y);
	angle.z = DirectX::XMConvertToRadians(angle.z);

	Matrix rotation = DirectX::XMMatrixRotationRollPitchYaw(angle.x,angle.y,angle.z);

	boxMin = DirectX::XMVector3Transform(boxMin,rotation);
	boxMax = DirectX::XMVector3Transform(boxMax, rotation);

	float t;
	//RayCastが呼ばれる順序をきちんとしないと1フレームずれる
	bool result = IntersectSegmentAABB(start,end,boxMin,boxMax,t);
	if (result)
	{
		// 衝突点と法線を設定
		hitPosition = start + worldRayVec * t;
		hitNormal = { 0,0,0 };
	}
	return result;
}

bool RayCastManager::IntersectSegmentAABB(const Vector3& start, const Vector3& end, const Vector3& boxMin, const Vector3& boxMax,float& t)
{
	float t_min = 0.0f;
	float t_max = 1.0f;

	float startArray[] = { start.x,start.y,start.z };
	float endArray[] = { end.x,end.y,end.z };
	float minArray[] = { boxMin.x,boxMin.y,boxMin.z };
	float maxArray[] = { boxMax.x,boxMax.y,boxMax.z };

	for (int i = 0;i < 3;i++)
	{
		//boxのminとmaxが線分のどのくらいの比率の位置に存在するのかを求める
		float invD = 1.0f / (endArray[i] - startArray[i]);
		float t0 = (minArray[i] - startArray[i]) * invD;
		float t1 = (maxArray[i] - startArray[i]) * invD;

		//startのほうが値が大きくなった時逆にする
		if (invD < 0.0f) std::swap(t0, t1);

		//すべての軸のtが重なっているところがあるかどうか
		t_min = t_min < t0 ? t0: t_min;
		t_max = t_max < t1 ? t_max: t1;
			

		//範囲が逆転したら,その時点であったっていない
		if (t_max < t_min)
		{
			return false;
		}
		t = t_min;
	}

	return true;
}

void RayCastManager::AddGameObject(std::shared_ptr<GameObject> gameObject)
{
	m_pGameObjects.emplace_back(gameObject);
	m_WeakCounts.emplace_back();
}

void RayCastManager::Destroy(GameObject* gameObject)
{
	int index = -1;
	for (int i = 0; i < m_pGameObjects.size(); i++)
	{
		if (gameObject->GetName() == m_pGameObjects[i].lock()->GetName())
		{
			index = i;
		}
	}
	if (index == -1)
	{
		assert("indexがありません");
	}

	m_pGameObjects.erase(m_pGameObjects.begin() + index);
}

void RayCastManager::Test()
{
	int a = m_WeakCounts.size();
	int b = m_pGameObjects.size();
}

void RayCastManager::Clear()
{
	m_pGameObjects.clear();
}

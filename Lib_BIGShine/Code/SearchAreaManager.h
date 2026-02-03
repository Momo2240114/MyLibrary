#pragma once

#include<vector>
#include"Vector.h"
#include<memory>

class AreaComponent;
class GameObject;

class SearchAreaManager
{
public:
	void SetAreaComponent(AreaComponent* areaComponent);

	std::weak_ptr<GameObject> GetNearPoint(const Vector3& position);

private:
	std::vector<AreaComponent*> m_pAreaComponent;
};
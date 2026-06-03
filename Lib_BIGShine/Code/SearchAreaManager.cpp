#include "SearchAreaManager.h"

#include"Engine/GamePlay/Core/Component/AreaComponent.h"
#include"Engine/GamePlay/Core/Object/Gameobject.h"

void SearchAreaManager::SetAreaComponent(AreaComponent* areaComponent)
{
	//エリアコンポーネントを追加する
	m_pAreaComponent.emplace_back(areaComponent);
}

std::weak_ptr<GameObject> SearchAreaManager::GetNearPoint(const Vector3& position)
{
	float length = FLT_MAX;
	int index;
	int i = 0;
	for (AreaComponent* areaComponent : m_pAreaComponent)
	{
		Vector3 areaPosition = areaComponent->GetGameObject()->transform->GetWorldPosition();
		Vector3 lengthVec = position - areaPosition;
		if (lengthVec.Length() < length)
		{
			length = lengthVec.Length();
			index = i;
		}
		i++;
	}
	return m_pAreaComponent[index]->GetGameObject();
}

#pragma once


#include "Engine/GamePlay/Core/Component/ComponentBase.h"

enum class ItemType
{
	Gold,
};

class ItemComponent :public Component
{
public:
	void SetItem(ItemType itemType);

	void ItemGetEvent();

	const std::string& GetName() const
	{
		static std::string name = "ItemComponent";
		return name;
	}
private:
	ItemType m_ItemType;
};
#include "ItemComponent.h"

#include"Engine/GamePlay/EventSystem/EventManager.h"

void ItemComponent::SetItem(ItemType itemType)
{
	m_ItemType = itemType;
}

void ItemComponent::ItemGetEvent()
{
	//アイテムの種類によって効果をイベントを変える
	switch (m_ItemType)
	{
	case ItemType::Gold:
		//エネミーのサーチ範囲を上げる
		EventManager::Trigger("SearchUp");
		break;
	}
}

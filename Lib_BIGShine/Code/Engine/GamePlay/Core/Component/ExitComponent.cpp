
#include "ExitComponent.h"
#include"GameObjectManager.h"

#include"PlayerComponent.h"
#include"BoxCollider.h"
#include"ItemManagerComponent.h"
#include"NumberSwitcher.h"

void ExitComponent::Start()
{
	//プレイヤーコンポーネントを取得する
	m_pPlayerComponent = GameObjectManager::Instance().Find("MainCamera")->GetComponent<PlayerComponent>();

	//ボックスコライダーコンポーネントを追加する
	m_pBoxColliderComponent = GetGameObject()->GetOrAddComponent<BoxCollider>();

	//// シーン内に一つだけある想定
	auto itemManagerObj = GameObjectManager::Instance().Find("ItemManager"); // オブジェクト名は環境に合わせてください
	if (itemManagerObj)
	{
		auto itemMgr = itemManagerObj->GetComponent<ItemManagerComponent>();
		if (itemMgr)
		{
			// 生成された数＝クリア条件にする
			m_GoalConditions = itemMgr->GetActualSpawnedCount();
		}
	}
}

void ExitComponent::Update(float et)
{
	if (!m_pDigit10.lock())
	{
		// 10の位
		auto obj10 = GameObjectManager::Instance().Find("NumberUI");
		if (obj10) m_pDigit10 = obj10->GetComponent<NumberSwitcher>();
	}
	if (!m_pDigit01.lock())
	{
		// 1の位
		auto obj01 = GameObjectManager::Instance().Find("NumberUI_01");
		if (obj01) m_pDigit01 = obj01->GetComponent<NumberSwitcher>();
	}
	// Max表示用
	if (!m_pDigitMax10.lock()) {
		auto obj = GameObjectManager::Instance().Find("NumberUI_02");
		if (obj) m_pDigitMax10 = obj->GetComponent<NumberSwitcher>();
	}
	if (!m_pDigitMax01.lock()) {
		auto obj = GameObjectManager::Instance().Find("NumberUI_03");
		if (obj) m_pDigitMax01 = obj->GetComponent<NumberSwitcher>();
	}
	// 目標数(Max)の表示を更新
	int maxD10 = (m_GoalConditions / 10) % 10;
	int maxD01 = m_GoalConditions % 10;
	if (auto sM10 = m_pDigitMax10.lock()) sM10->SetDigit(maxD10);
	if (auto sM01 = m_pDigitMax01.lock()) sM01->SetDigit(maxD01);

	auto player = m_pPlayerComponent.lock();
	if (player)
	{
		// 1. 現在の所持数を取得
		int currentItemCount = player->GetHasItem();

		// 2. 2桁の表示を更新（0～99の範囲で計算）
		int d10 = (currentItemCount / 10) % 10; // 10の位
		int d01 = currentItemCount % 10;        // 1の位

		if (auto s10 = m_pDigit10.lock()) s10->SetDigit(d10);
		if (auto s01 = m_pDigit01.lock()) s01->SetDigit(d01);

		// プレイヤーが目標個数達成したとき
		if (currentItemCount >= m_GoalConditions)
		{
			// プレイヤーがゴールまで来たらゴールフラグを立てる
			for (std::shared_ptr<GameObject> gameObject : m_pBoxColliderComponent->currentTriggers)
			{
				if (gameObject->GetLayer() == GameObject::LayerType::Player)
				{
					if (!m_GoalFlag)
					{
						ActionRegistry::Instance().Call("GameClear");
					}
					m_GoalFlag = true;
				}
			}
		}
	}	float Count = 3.0;
	if (m_GoalFlag)
	{
		GoalTime += et;
		if (GoalTime > Count)
		{
			m_GoalFlag = false;
		}
	}
}

void ExitComponent::Gui()
{
	ImGui::Checkbox("Goal",&m_GoalFlag);
	ImGui::DragInt("GoalConditions", &m_GoalConditions);
}

bool ExitComponent::GetIsGoal()
{
	return m_GoalFlag;
}

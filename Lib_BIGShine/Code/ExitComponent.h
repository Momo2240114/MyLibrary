#pragma once


#include"Engine/GamePlay/Core/Component/ComponentBase.h"


class PlayerComponent;
class BoxCollider;
class NumberSwitcher;

class ExitComponent : public Component
{
public:

	void Start();

	void Update(float et);

	void Gui();

	bool GetIsGoal();

	const std::string& GetName() const override
	{
		static std::string name = "ExitComponent";
		return name;
	}

private:
	//プレイヤーコンポーネント
	std::weak_ptr<PlayerComponent> m_pPlayerComponent;

	//ボックスコライダー
	std::shared_ptr<BoxCollider> m_pBoxColliderComponent;

	// --- 追加: 数値制御用 ---
	std::weak_ptr<NumberSwitcher> m_pDigit10; // 10の位 (NumberUI)
	std::weak_ptr<NumberSwitcher> m_pDigit01; // 1の位 (NumberUI_01)

	// 目標数(Max)表示用
	std::weak_ptr<NumberSwitcher> m_pDigitMax10; // NumberUI_02
	std::weak_ptr<NumberSwitcher> m_pDigitMax01; // NumberUI_03

	//ゴール条件に必要なアイテム数
	int m_GoalConditions = 1;
	float GoalTime = 0.0f;
	bool m_GoalFlag = false;
};
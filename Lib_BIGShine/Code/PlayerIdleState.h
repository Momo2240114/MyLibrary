#pragma once

#include"State.h"
#include"PlayerComponent.h"

class PlayerIdleState :public State
{
public:
	PlayerIdleState(GameObject* gameObject) :State(gameObject){}

	~PlayerIdleState(){}

	//ステートに入った時のメソッド
	void Enter();

	void Execute(float et);

	void Exit();


	const char* GetName()const { return "PlayerIdle"; }

private:
	std::weak_ptr<PlayerComponent> m_pPlayerComponent;
	std::weak_ptr<MovementComponent> m_pMovementComponent;
	std::weak_ptr<EasingComponent> m_pEasingComponent;
};
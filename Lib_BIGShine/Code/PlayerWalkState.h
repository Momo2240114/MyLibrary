#pragma once
#include"State.h"
#include"PlayerComponent.h"
#include"MovementComponent.h"

class CameraComponent;

class PlayerWalkState :public State
{
public:
	PlayerWalkState(GameObject* gameObject) :State(gameObject) {}

	void Enter();

	void Execute(float et);

	void LateExecute(float et);

	void Exit();

	const char* GetName()const { return "PlayerWalk"; };
private:
	std::weak_ptr<PlayerComponent>m_pPlayerComponent;
	std::weak_ptr<MovementComponent> m_pMovementComponent;
	std::weak_ptr<EasingComponent> m_pEasingComponent;
	//std::weak_ptr<CameraComponent> m_pCameraComponent;
};
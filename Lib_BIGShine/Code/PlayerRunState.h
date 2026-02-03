#pragma once

#pragma once
#include"State.h"
#include<memory>

//#include"PlayerComponent.h"
class PlayerComponent;
class CameraComponent;
class EasingComponent;
class MovementComponent;

class PlayerRunState :public State
{
public:
	PlayerRunState(GameObject* gameObject) :State(gameObject) {}


	void Enter();

	void Execute(float et);

	void LateExecute(float et);

	void Exit();

	const char* GetName()const { return "PlayerRun"; };
private:
	std::weak_ptr<PlayerComponent>m_pPlayerComponent;
	std::weak_ptr<MovementComponent> m_pMovementComponent;
	std::weak_ptr<EasingComponent> m_pEasingComponent;
	std::weak_ptr<CameraComponent> m_pCameraComponent;

	float m_Fov;
};
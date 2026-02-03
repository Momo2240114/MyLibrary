#pragma once

#include"State.h"

class  CapsuleCollider;
class TransformDirector;
class MovementComponent;
class PlayerDeadState :public State
{
public:
	PlayerDeadState(GameObject* gameObject) :State(gameObject) {}

	~PlayerDeadState() {}


	void Enter();

	void Execute(float et);

	void Exit(){}

	const char* GetName()const {
		return "PlayerDeadState";
	}
private:
	std::weak_ptr<CapsuleCollider> m_pCapsuleCollider;
	std::weak_ptr<TransformDirector> m_pTransformDirector;
	std::weak_ptr<MovementComponent> m_pMovementComponent;
	int m_Step = 0;
};
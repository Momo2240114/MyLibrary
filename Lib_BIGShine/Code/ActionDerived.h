#pragma once

#include"ActionBase.h"

#include<memory>


class EnemyComponent;
class AgentComponent;
class MovementComponent;
class AreaComponent;
class AnimationController;
class CapsuleCollider;
class TransformDirector;

class IdleAction :public ActionBase
{
public:
	IdleAction(GameObject* gameObject) :ActionBase(gameObject) {}

	ActionBase::State Run(float et);
private:
	std::weak_ptr<EnemyComponent> m_pEnemyComponent;
	std::weak_ptr<AgentComponent> m_pAgentComponent;
	std::weak_ptr<AreaComponent> m_pAreaComponent;
	std::weak_ptr<AnimationController> m_pAnimationControllerComponent;
	float m_RunTimer= 0.0f;
};


class WanderAction :public ActionBase
{
public:
	WanderAction(GameObject* gameObject) :ActionBase(gameObject) {}


	ActionBase::State Run(float et);

private:
	std::weak_ptr<EnemyComponent> m_pEnemyComponent;
	std::weak_ptr<AgentComponent> m_pAgentComponent;
	std::weak_ptr<MovementComponent> m_pMovementComponent;
	std::weak_ptr<AnimationController> m_pAnimationControllerComponent;
};

class ChaseAction :public ActionBase
{
public:
	ChaseAction(GameObject* gameObject) :ActionBase(gameObject) {}

	ActionBase::State Run(float et);
private:
	std::weak_ptr<EnemyComponent> m_pEnemyComponent;
	std::weak_ptr<AgentComponent> m_pAgentComponent;
	std::weak_ptr<MovementComponent> m_pMovementComponent;
	std::weak_ptr<AnimationController> m_pAnimationControllerComponent;

	float m_RunTimer = 0.0f;

	bool m_NotSee = false;
};

class AttackAction:public ActionBase
{
public:
	AttackAction(GameObject* gameObject) :ActionBase(gameObject) {}

	ActionBase::State Run(float et);
private:
	std::weak_ptr<EnemyComponent> m_pEnemyComponent;
	std::weak_ptr<AnimationController> m_pAnimationControllerComponent;
	std::weak_ptr<AgentComponent> m_pAgentComponent;
	std::weak_ptr<MovementComponent> m_pMovementComponent;

	float m_AnimationTime = 1.5f;

	bool m_PlayerHitFlag = false;
};

class CautionAction :public ActionBase
{
public:
	CautionAction(GameObject* gameObject) :ActionBase(gameObject) {}
	ActionBase::State Run(float et);

private:
	std::weak_ptr<EnemyComponent> m_pEnemyComponent;
	std::weak_ptr<AnimationController> m_pAnimationControllerComponent;
	std::weak_ptr<AgentComponent> m_pAgentComponent;
	std::weak_ptr<MovementComponent> m_pMovementComponent;
};

class CheckPointMoveAction :public ActionBase
{
public:
	CheckPointMoveAction(GameObject* gameObject) :ActionBase(gameObject) {}
	ActionBase::State Run(float et);

private:
	std::weak_ptr<EnemyComponent> m_pEnemyComponent;
	std::weak_ptr<AnimationController> m_pAnimationControllerComponent;
	std::weak_ptr<AgentComponent> m_pAgentComponent;
	std::weak_ptr<MovementComponent> m_pMovementComponent;

	Vector3 m_NearSneakObjectPosition;
	std::weak_ptr<TransformDirector> m_pTransformDirector;
};


class PeekAction :public ActionBase
{
public:
	PeekAction(GameObject* gameObject) :ActionBase(gameObject) {}
	ActionBase::State Run(float et);

private:
	std::weak_ptr<EnemyComponent> m_pEnemyComponent;
	std::weak_ptr<AnimationController> m_pAnimationControllerComponent;
	std::weak_ptr<AgentComponent> m_pAgentComponent;
	std::weak_ptr<MovementComponent> m_pMovementComponent;

	std::weak_ptr<TransformDirector> m_pTransformDirector;

	float m_Time = 0.0f;

	int m_KillMotionStep = 0;
};

class KillMotionAction :public ActionBase
{
public:
	KillMotionAction(GameObject* gameObject) :ActionBase(gameObject) {}
	ActionBase::State Run(float et);

private:
	std::weak_ptr<EnemyComponent> m_pEnemyComponent;
	std::weak_ptr<AnimationController> m_pAnimationControllerComponent;
	std::weak_ptr<AgentComponent> m_pAgentComponent;
	std::weak_ptr<MovementComponent> m_pMovementComponent;
	std::weak_ptr<CapsuleCollider>m_pCapsuleColliderComponent;
	float m_Time = 0.0f;

	int m_KillMotionStep = 0;
};

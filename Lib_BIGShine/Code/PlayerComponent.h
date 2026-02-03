#pragma once

#include"ComponentBase.h"

#include"MovementComponent.h"
#include"EasingComponent.h"
#include"StateMachine.h"
#include"CapsuleCollider.h"

#include"Audio\Audio.h"

class LightComponent;
class CameraComponent;
class GameObject;

enum class PlayerStateType
{
	Default,
	Idle,
	Walk,
	Run,
	Sneak,
	Hide,
};

enum class PlayerEvent
{
	None, 
	Dead, 
	DeathProduction,
	Clear,
};

class PlayerComponent :public Component
{
public:
	void Start();


	void Update(float et);

	void LateUpdate(float et);

	void Gui();

	float  EnemyLength();

	int GetAreaIndex();
	
	void SetHidePosition(const Vector3& position)
	{
		m_HidePosition = position;
	}

	Vector3& GetHidePosition()
	{
		return m_HidePosition;
	}

	const std::string& GetName() const override
	{
		static std::string name = "Player";
		return name;
	}

	void SetPlayerState(PlayerStateType state);

	void SetEvent(PlayerEvent event)
	{
		m_PlayerEvent = event;
	}

	void IsMove();

	Vector3 Moving();

	StateMachine* GetStateMachine() { return m_pStateMachine.get();}

	PlayerStateType GetPlayerState() { return m_PlayerState; }

	void Collider();

	void IsTrigger(bool flag);

	// PlayerComponent.h
	bool IsSneaking() const;

	std::string GetStateName()
	{
		return m_pStateMachine->GetStateName();
	}
	PlayerStateType GetPlayseState()
	{
		return m_PlayerState;
	}

	int GetHasItem()
	{
		return m_hasItem;
	}

	void SetHideGameObject(std::weak_ptr<GameObject> gameObject)
	{
		m_pHideGameObject = gameObject;
	}

	std::weak_ptr<GameObject> GetHideGameObject()
	{
		return m_pHideGameObject;
	}

private:
	std::shared_ptr<MovementComponent> m_pMovementComponent;
	std::shared_ptr<EasingComponent> m_pEasingComponent;
	std::shared_ptr<CapsuleCollider> m_pCapsuleColliderComponent;

	std::shared_ptr<StateMachine> m_pStateMachine;
	std::shared_ptr<LightComponent>m_pLightComponent;

	std::weak_ptr<CameraComponent> m_pCameraComponent;
	std::weak_ptr<GameObject> m_pUIObject;

	PlayerStateType m_PlayerState = PlayerStateType::Idle;
	PlayerEvent m_PlayerEvent = PlayerEvent::None;

	std::weak_ptr<GameObject> m_pEnemy;
	bool m_pMoveFlag = false;

	bool m_RayHit = false;

	std::string hitGameObjectName;

	int m_hasItem = 0;

	float m_Fov = 0.0f;

	Vector3 m_HidePosition;

	std::weak_ptr<GameObject> m_pHideGameObject;

	//êSîèâπ
	//AudioSource* m_pHeartBeatSound;

	float m_Time = 0.0f;
	float m_Diration = 0.1f;
	float m_BeatRadius = 20.0f;
};
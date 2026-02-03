#pragma once

#include"ComponentBase.h"
#include"Vector.h"


enum class RotationType
{
	Turn,
	LookAtVelocity, 
};

class MovementComponent :public Component
{
public:
	void Start();

	void Update(float et);

	void Move(float et);

	void ZeroDirection();

	void Turn(float et);

	void Gui()override;

	void LookAtVelocity(float et);

	float GetMoveSpeed()
	{
		return m_MoveSpeed;
	}

	void SetTurnSpeed(float speed)
	{
		m_TurnSpeed = speed;
	}

	void AddDirection(const Vector3& direction);
	void SetDirection(const Vector3& direction);

	void AddAngle(const Vector3& angle);

	//移動速度を代入する
	void SetMoveSpeed(float moveSpeed)
	{
		m_MoveSpeed = moveSpeed;
	}

	void SetRotationType(RotationType rotationType)
	{
		m_RotationType = rotationType;
	}

	const Vector3& GetDirection()
	{
		return m_Direction;
	}

	const std::string& GetName() const override
	{
		static std::string name = "Movement";
		return name;
	}
private:
	float m_MoveSpeed = 5.0f;
	float m_TurnSpeed = 100.0f;

	Vector3 m_Direction = {0,0,0};
	Vector3 m_Angle = { 0,0,0 };

	RotationType m_RotationType;

	Vector3 offsetRotation{ 0,0,0};
	Vector3 angle = { 0,0,0 };

	bool m_IsPitchLimit = true;            // 制限をかけるか
	float m_MinPitch = -45.0f;             // 下方向の限界（度数法）
	float m_MaxPitch = 45.0f;              // 上方向の限界（度数法）
};
#pragma once


#include "Engine/GamePlay/Core/Component/ComponentBase.h"

//イージングの種類
enum class EasingType
{
	None,
	Lerp,
	OutLerp,
	OutQuint,
	InExpo,
	OutExpo,
	None1,
	OutBounds,
};

class Easing
{
public:
	void Update(float et);

	void Lerp(float t);

	void OutLerp(float t);

	void GetLerpTime();

	void EaseOutQuint(float t);
	void GetOutQuintTime();

	void EaseInExpo(float t);
	void GetInExpoTime();

	void EaseOutExpo(float t);
	void GetOutExpoTime();
	void EaseOutBounce(float t);

	void SetValue(float value)
	{
		m_Value = value;
	}

	bool GetEndFlag()
	{
		return m_EasingEndFlag;
	}

	void StartEasing(EasingType easingType, float timeSpeed, float target);

	float GetValue()
	{
		return m_Value;
	}

	float GetRatio()
	{
		return m_Ratio;
	}

	void Gui();

private:

	EasingType m_EasingType;
	float m_Time;
	float m_Ratio = 0.0f;
	float m_StartValue = 0.0f;

	float m_TimeSpeed;

	float m_Target = 0.0f;

	float m_Value = 0.0f;

	bool m_EasingEndFlag = true;
};


class EasingComponent :public Component
{
public:

	void Update(float et);

	void AddEasing();
	int AddEasingAndGetIndex();

	void RemoveEasing(int index);

	const std::string& GetName() const override
	{
		static std::string name = "Easing";
		return name;
	}

	void PlayEasing(int index, EasingType easingType, float timeSpeed, float target);

	void SetStartValue(int index,float value);

	float GetValue(int index);
	float GetRatio(int index);

	bool GetEndFlag(int index)
	{
		return m_Easings[index].GetEndFlag();
	}

	void Gui();

	void SetUseUnscaledTime(bool enabled_)
	{
		useUnscaledTime = enabled_;
	}

private:
	std::vector<Easing> m_Easings;
	bool useUnscaledTime = false;
};



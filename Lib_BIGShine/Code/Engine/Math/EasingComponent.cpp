#include "PrecompiledHeader.h"

#include "EasingComponent.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/System/TimeManager.h"

#include <algorithm>

void EasingComponent::Update(float et_)
{
	//イージングの更新処理
	float updateTime_ = et_;
	if (useUnscaledTime)
	{
		const std::shared_ptr<GameObject> owner_ = GetGameObject();
		if (owner_)
		{
			const std::shared_ptr<TimeManager> timeManager_ = owner_->GetManager().GetTimeManager();
			if (timeManager_) updateTime_ = timeManager_->GetLastRawDeltaTime();
		}
	}
	for (size_t index_ = 0; index_ < m_Easings.size(); ++index_)
	{
		m_Easings[index_].Update(updateTime_);
	}
}

void EasingComponent::AddEasing()
{
	m_Easings.emplace_back();
}

int EasingComponent::AddEasingAndGetIndex()
{
	m_Easings.emplace_back();
	return static_cast<int>(m_Easings.size()) - 1;
}

void EasingComponent::RemoveEasing(int index)
{
	if (m_Easings.size() < index)
	{
		assert(L"範囲外アクセス");
	}

	//特定のインデックスを削除する
	m_Easings.erase(m_Easings.begin()+index);
}

void EasingComponent::PlayEasing(int index,EasingType easingType,float timeSpeed,float target)
{
	if (m_Easings.size() < index)
	{
		assert(L"範囲外アクセス");
	}
	m_Easings[index].StartEasing(easingType, timeSpeed, target);
}

void EasingComponent::SetStartValue(int index,float value)
{
	if (m_Easings.size() < index)
	{
		assert(L"範囲外アクセス");
	}
	return m_Easings[index].SetValue(value);
}

float EasingComponent::GetValue(int index)
{
	if (m_Easings.size() < index)
	{
		assert(L"範囲外アクセス");
	}
	return m_Easings[index].GetValue();
}

float EasingComponent::GetRatio(int index)
{
	if (m_Easings.size() < index)
	{
		assert(L"範囲外アクセス");
	}
	return m_Easings[index].GetRatio();
}


void EasingComponent::Gui()
{
	for (int i = 0;i < m_Easings.size();i++)
	{
		m_Easings[i].Gui();
	}
}



void Easing::Update(float et)
{
	//イージングが終了したのならば
	if (m_EasingEndFlag)return;

	switch (m_EasingType)
	{
	case EasingType::Lerp:
		m_Time += et * m_TimeSpeed;
		Lerp(m_Time);
		break;
	case EasingType::OutLerp:
		m_Time += et * m_TimeSpeed;
		Lerp(m_Time);
		break;
	case EasingType::OutQuint:
		m_Time += et * m_TimeSpeed;
		EaseOutQuint(m_Time);
		break;
	case EasingType::InExpo:
		m_Time += et * m_TimeSpeed;
		EaseInExpo(m_Time);
		break;
	case EasingType::OutExpo:
		m_Time += et * m_TimeSpeed;
		EaseOutExpo(m_Time);
		break;
	case EasingType::OutBounds:
		m_Time += et * m_TimeSpeed;
		EaseOutBounce(m_Time);
			break;
	case EasingType::None:
		//m_Time = 0.0f;
		break;
	}
	//差分
	m_Value = std::lerp(m_StartValue, m_Target, m_Ratio);


	//イージング処理を終了する
	if (m_Time >= 1.0f)
	{
		m_EasingEndFlag = true;
		m_Time = 1.0f;
	}
	if (m_Time <= 0.0f)
	{
		m_EasingEndFlag = true;
		m_Time = 0.0f;
	}
}


void Easing::Lerp(float t)
{
	m_Ratio = t;
}

void Easing::OutLerp(float t)
{
	m_Ratio = -t;
}

void Easing::GetLerpTime()
{
	m_Time = m_Ratio;
}

void Easing::EaseOutQuint(float t)
{
	// t を 0.0 ～ 1.0 の範囲にクランプ（安全のため）
	if (t < 0.0f) t = 0.0f;
	if (t > 1.0f) t = 1.0f;

	// 数式: 1 - (1 - t)^5
	float invT = 1.0f - t;
	m_Ratio = 1.0f - (invT * invT * invT * invT * invT);

}

void Easing::GetOutQuintTime()
{
	// 数式: t = 1 - (1 - ratio)^(1/5)
	// std::pow(x, 0.2f) は 5乗根を意味します
	m_Time = 1.0f - std::pow(1.0f - m_Ratio, 0.2f);
}

void Easing::EaseInExpo(float t)
{
	// 範囲外をクランプ
	t = std::clamp(t, 0.0f, 1.0f);

	// tが0のときは2のマイナス10乗（約0.00097）という微小な値が出るのを防ぐため0を返す
	if (t == 0.0f)
	{
		m_Ratio = 0.0f;
		return;
	}

	// 数式: 2^(10 * (t - 1))
	m_Ratio = std::pow(2.0f, 10.0f * (t - 1.0f));
}

void Easing::GetInExpoTime()
{
	// 範囲外をクランプ
	float ratio = std::clamp(m_Ratio, 0.0f, 1.0f);

	// ratioが0のときは特別処理
	if (ratio == 0.0f)
	{
		m_Time = 0.0f;
		return;
	}


	m_Time = std::log(ratio) / (10.0f * std::log(2.0f)) + 1.0f;
}

void Easing::EaseOutExpo(float t)
{
	// 範囲外をクランプ
	t = std::clamp(t, 0.0f, 1.0f);

	// tが1のときは1 - 2^(-10)（約0.999）という値になるのを防ぐため、きっちり1を返す
	if (t == 1.0f)
	{
		m_Ratio = 1.0f;
		return;
	}

	// 数式: 1 - 2^(-10 * t)
	m_Ratio = 1.0f - std::pow(2.0f, -10.0f * t);
}

void Easing::GetOutExpoTime()
{
	// ratioが1のときは特別処理
	if (m_Ratio == 1.0f)
	{
		m_Time = 1.0f;
		return;
	}

	m_Time = -std::log(1.0f - m_Ratio) / (10.0f * std::log(2.0f));
}

void Easing::EaseOutBounce(float x) 
{
	const float n1 = 7.5625f;
	const float d1 = 2.75f;

	if (x < 1.0f / d1) {
		m_Ratio =  n1 * x * x;
	}
	else if (x < 2.0f / d1) {
		x -= 1.5f / d1;
		m_Ratio = n1 * x * x + 0.75f;
	}
	else if (x < 2.5f / d1) {
		x -= 2.25f / d1;
		m_Ratio = n1 * x * x + 0.9375f;
	}
	else {
		x -= 2.625f / d1;
		m_Ratio =  n1 * x * x + 0.984375f;
	}
}

void Easing::StartEasing(EasingType easingType, float timeSpeed, float target)
{
	m_Time = 0.0f;
	m_EasingEndFlag = false;

	//目標の値を設定する
	m_Target = target;

	//比率を初期化
	m_Ratio = 0.0f;

	//イージングの種類を設定する
	m_EasingType = easingType;

	//イージングの速度を設定
	m_TimeSpeed = timeSpeed;

	//開始時の最小値を設定する
	m_StartValue = m_Value;


}

void Easing::Gui()
{
	ImGui::InputFloat("Ratio", &m_Ratio);
	ImGui::InputFloat("Time", &m_Time);
	ImGui::InputFloat("StartValue", &m_StartValue);
}

#pragma once

#include"Engine/GamePlay/Core/Component/ComponentBase.h"

class AreaComponent :public Component
{
public:
	const std::string& GetName() const override
	{ 
		static std::string ret = "AreaComponent";
		return ret;
	}

	void Start();

	void Update(float et);

	const Vector3& GetPosition()
	{
		return m_Position;
	}

	const float GetRadius()
	{
		return m_Radius;
	}

	void Gui();

	// BoxCollider.h 内
	void Serialize(nlohmann::json& j) const override {
		j["Radius"] =  m_Radius ;
	}

	void Deserialize(const nlohmann::json& j) override {
		if (j.contains("Radius")) {
			m_Radius = j["Radius"];
		}
	}


private:
	Vector3 m_Position;
	Vector3 m_Rotation;
	Vector3 m_Scale;
	float m_Radius = 5.0f;

	float m_Top = 0.0f;
	float m_Bottom = 0.0f;
	float m_Right = 0.0f;
	float m_Left  = 0.0f;
};
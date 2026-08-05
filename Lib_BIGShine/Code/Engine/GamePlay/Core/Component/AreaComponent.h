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

	// Transformなどから現在位置を取り込み、Area判定に使う範囲情報を更新する。
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

	// AreaComponentは半径だけを保存し、位置や姿勢はTransform側の情報から復元する。
	void Serialize(nlohmann::json& j) const override {
		j["Radius"] =  m_Radius ;
	}

	void Deserialize(const nlohmann::json& j) override {
		// 古いデータや未設定データでも読み込めるよう、Radiusがある場合だけ上書きする。
		if (j.contains("Radius")) {
			m_Radius = j["Radius"];
		}
	}


private:
	// Areaの中心・姿勢・大きさ。主な判定半径はm_Radiusを使い、矩形値は必要に応じてGui/判定へ展開する。
	Vector3 m_Position;
	Vector3 m_Rotation;
	Vector3 m_Scale;
	float m_Radius = 5.0f;

	float m_Top = 0.0f;
	float m_Bottom = 0.0f;
	float m_Right = 0.0f;
	float m_Left  = 0.0f;
};

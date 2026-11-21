#pragma once


#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/Math/Vector.h"

class InputComponent :public Component
{
public:
	const std::string& GetName() const override
	{
		static std::string name = "Input";
		return name;
	}

	// GameObjectに付ける入力処理の入口。現状は派生/拡張用の空Component。
	void Update(float deltaTime);
};

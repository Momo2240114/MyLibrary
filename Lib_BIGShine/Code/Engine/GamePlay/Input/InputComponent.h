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
	void Update(float deltaTime);
};
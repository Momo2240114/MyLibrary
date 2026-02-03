#pragma once

#include"GameObject.h"

class State
{
public:
	State(GameObject*gameObject):owner(gameObject){}
	virtual ~State(){}

	virtual void Enter() = 0;

	virtual void LateExecute(float et){}

	virtual void Execute(float elapsedTime) = 0;

	virtual void Exit() = 0;

	virtual const char* GetName()const = 0;

protected:
	GameObject* owner;
};
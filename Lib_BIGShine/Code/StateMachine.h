#pragma once

#include"State.h"

#include <unordered_map>
#include<string>

class StateMachine
{
public:

	//コンストラクタ
	StateMachine(){}

	//デストラクタ
	~StateMachine();

	void Update(float et);
	
	void LateUpdate(float et);

	//ステート変更
	void ChangeState(const std::string name);

	void RegisterState(State* state);

	//ステート名取得
	const char* GetStateName()const { return currentState->GetName(); }
	State* GetState()const { return currentState; }


private:
	//現在のステート
	State* currentState = nullptr;

	//各ステート保持する配列
	std::unordered_map<std::string, State*> statePool;
};
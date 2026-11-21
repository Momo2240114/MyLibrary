#pragma once

#include"State.h"

#include <unordered_map>
#include<string>

// 文字列名で登録したStateを切り替えながら実行する汎用StateMachine。
// 古いPlayer用Stateや簡易的な行動制御で、Enter/Execute/Exitの流れを共通化する。
class StateMachine
{
public:
	StateMachine()
	{
	}

	// 登録済みStateを破棄する。
	~StateMachine();

	// 現在のStateの通常更新を実行する。
	void Update(float et);

	// 現在のStateの後段更新を実行する。
	void LateUpdate(float et);

	// 指定名のStateへ遷移し、ExitとEnterを呼び分ける。
	void ChangeState(const std::string name);

	// StateMachineが所有・更新するStateを登録する。
	void RegisterState(State* state);

	// 現在実行中のState名を返す。
	const char* GetStateName()const
	{
		return currentState->GetName();
	}

	State* GetState()const
	{
		return currentState;
	}

private:
	// 現在実行中のState。
	State* currentState = nullptr;

	// State名からState実体を引くための登録テーブル。
	std::unordered_map<std::string, State*> statePool;
};

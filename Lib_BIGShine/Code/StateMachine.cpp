#include "StateMachine.h"

StateMachine::~StateMachine()
{
	for (auto& pair : statePool)
	{
		delete pair.second;
		pair.second = nullptr;
	}
	statePool.clear();
}

void StateMachine::Update(float et)
{
	//現在のステートを実行
	if (currentState)
	{
		currentState->Execute(et);
	}
}

void StateMachine::LateUpdate(float et)
{
	//現在のステートを実行
	if (currentState)
	{
		currentState->LateExecute(et);
	}
}

void StateMachine::ChangeState(const std::string name)
{
	if (currentState != nullptr)
	{
		currentState->Exit();
	}
	auto it = statePool.find(name);
	if (it != statePool.end())
	{
		currentState = it->second;
		currentState->Enter();
	}

}

void StateMachine::RegisterState(State*state)
{
	statePool.insert(std::make_pair(state->GetName(),state));
}

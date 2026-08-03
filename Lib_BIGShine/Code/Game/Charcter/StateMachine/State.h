#pragma once

#include"Engine/GamePlay/Core/Object/GameObject.h"

// GameObjectをOwnerとして持つ、汎用StateMachine用の状態基底クラス。
// 派生Stateは開始、更新、終了の各タイミングで具体的な挙動を実装する。
class State
{
public:
	State(GameObject*gameObject):owner(gameObject){}
	virtual ~State(){}

	// Stateへ入った瞬間の初期化処理。
	virtual void Enter() = 0;

	// 通常更新後に必要な追従処理。不要なStateは未実装のままにできる。
	virtual void LateExecute(float et){}

	// State中の毎フレーム処理。
	virtual void Execute(float elapsedTime) = 0;

	// Stateから抜ける直前の後始末。
	virtual void Exit() = 0;

	// StateMachineが遷移先を識別するための名前を返す。
	virtual const char* GetName()const = 0;

protected:
	// このStateが操作するGameObject。
	GameObject* owner;
};

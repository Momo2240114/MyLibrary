#pragma once

#include"GameObject.h"


//行動処理基底クラス
class ActionBase
{
public:
	ActionBase(GameObject* gameObject):m_pOwner(gameObject){}

	enum class State
	{
		Run,//実行中
		Failed,//実行失敗
		Complete,//実行成功
	};

	virtual void Start() {};
	//実行処理(純粋仮想関数)
	virtual ActionBase::State Run(float et) = 0;

protected:
	GameObject* m_pOwner;
	int step = 0;

	bool m_ActionStart = true;
};
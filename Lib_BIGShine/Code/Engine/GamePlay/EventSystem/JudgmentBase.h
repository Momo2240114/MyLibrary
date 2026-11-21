#pragma once

class GameObject;
#include<memory>

// StateやActionを実行してよいか判断するための基底クラス。
class JudgmentBase
{
public:
	// 判定対象のGameObjectを保持し、派生JudgmentがComponent参照を取り出せるようにする。
	JudgmentBase(GameObject* gameObject): m_pOwner(gameObject)
	{
	}

	// trueを返した場合、対応する行動や遷移を実行する。
	virtual bool Judgment() = 0;

protected:
	// 所有者は外部で管理されるため、ここでは参照だけを保持する。
	GameObject* m_pOwner;
};

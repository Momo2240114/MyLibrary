#pragma once

class GameObject;
#include<memory>

//実行判定
class JudgmentBase
{
public:
	JudgmentBase(GameObject* gameObject): m_pOwner(gameObject){}
	virtual bool Judgment() = 0;
protected:
	GameObject* m_pOwner;
};
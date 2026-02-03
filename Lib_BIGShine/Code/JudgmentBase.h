#pragma once

class GameObject;
#include<memory>

//é¿çsîªíË
class JudgmentBase
{
public:
	JudgmentBase(GameObject* gameObject): m_pOwner(gameObject){}
	virtual bool Judgment() = 0;
protected:
	GameObject* m_pOwner;
};
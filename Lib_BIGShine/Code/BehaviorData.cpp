#include "BehaviorData.h"

NodeBase* BehaviorData::PopSequenceNode()
{
	//空ならNULL
	if (m_pSequenceStack.empty() != 0)
	{
		return nullptr;
	}

	//一番上のデータを取得する
	NodeBase* node = m_pSequenceStack.top();


	if (node != nullptr)
	{
		//取り出したデータを削除
		m_pSequenceStack.pop();
	}
	return node;
}

int BehaviorData::GetSequenceStep(std::string name)
{
	//保存されていなければstepの番号が0の要素数をnameにする
	if (m_RunSequenceStepMap.count(name) == 0)
	{
		m_RunSequenceStepMap.insert(std::make_pair(name, 0));
	}
	return m_RunSequenceStepMap.at(name);
}

void BehaviorData::SetSequenceStep(std::string name, int step)
{
	m_RunSequenceStepMap.at(name) = step;
}

void BehaviorData::Init()
{
	m_RunSequenceStepMap.clear();
	while (m_pSequenceStack.size() > 0)
	{
		m_pSequenceStack.pop();
	}
}

#include "BehaviorTree.h"

#include"BehaviorData.h"
#include"NodeBase.h"

BehaviorTree::~BehaviorTree()
{
	NodeAllClear(m_pRoot);
}

NodeBase* BehaviorTree::ActiveNodeInference(BehaviorData* data)
{
	//データをリセットして開始
	data->Init();
	return m_pRoot->Inference(data);
}

NodeBase* BehaviorTree::SequenceBack(NodeBase* sequenceNode, BehaviorData* data)
{
	return sequenceNode->Inference(data);
}

void BehaviorTree::AddNode(std::string parentName, std::string entryName, int priority, SelectRule selectRule, JudgmentBase* judgment, ActionBase* action)
{
	if (parentName != "")
	{
		//親ノードをRootから全て検索する
		NodeBase* parentNode = m_pRoot->SearchNode(parentName);

		//親ノードがいたら親ノードの子ノードとして追加する
		if (parentNode != nullptr)
		{
			NodeBase* addNode = new NodeBase(entryName, parentNode, priority, selectRule, judgment, action);
			parentNode ->AddChild(addNode);;
		}
	}
	//親ノードがないければ
	else
	{
		if (m_pRoot == nullptr)
		{
			m_pRoot = new NodeBase(entryName, nullptr, priority, selectRule, judgment, action);
		}
	}

}

NodeBase* BehaviorTree::Run(NodeBase* actionNode, BehaviorData* data, float et)
{
	//ノード実行
	ActionBase::State state = actionNode->Run(et);

	//正常終了
	if (state == ActionBase::State::Complete)
	{
		NodeBase* sequenceNode = data->PopSequenceNode();

		//途中じゃないなら終了
		if (sequenceNode == nullptr)
		{
			return nullptr;
		}
		//途中なら
		else
		{
			return SequenceBack(sequenceNode,data);
		}
	}
	else if (state == ActionBase::State::Failed)
	{
		//失敗は終了
		return nullptr;
	}

	//現状維持
	return actionNode;
}

void BehaviorTree::NodeAllClear(NodeBase* delNode)
{
	size_t count = delNode->children.size();
	if (count > 0)
	{
		for (NodeBase* node : delNode->children)
		{
			NodeAllClear(node);
		}
		delete delNode;
	}
	else
	{
		delete delNode;
	}
}

#pragma once
#include<string>


class ActionBase;
class JudgmentBase;
class NodeBase;
class BehaviorData;
class GameObject;


class BehaviorTree
{
public:

	//選択ルール
	enum class SelectRule
	{
		None,//末端ノード用
		Priority,//優先順位
		Sequence,//シーケンス
		SequentialLooping,//シーケンスシャルルーピング
		Random,//ランダム
	};

public:
	BehaviorTree():m_pRoot(nullptr),m_pOwner(nullptr){}
	BehaviorTree(GameObject* gameObject) :m_pRoot(nullptr), m_pOwner(gameObject) {};
	~BehaviorTree();

	//実行ノードを推論する
	NodeBase* ActiveNodeInference(BehaviorData* data);

	//シーケンスノードから推論開始
	NodeBase* SequenceBack(NodeBase* sequenceNode,BehaviorData* data);

	//ノード追加
	void AddNode(std::string parentName,std::string entryName,int priority,SelectRule selectRule,JudgmentBase* judgment,ActionBase* action);

	//実行
	NodeBase* Run(NodeBase* actionNode, BehaviorData* data,float et);

private:
	//ノード全削除
	void NodeAllClear(NodeBase* delNode);

private:
	//ルートノード
	NodeBase* m_pRoot;
	GameObject* m_pOwner;
};
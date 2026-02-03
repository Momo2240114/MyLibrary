#pragma once

#include<vector>
#include<stack>
#include<map>
#include<string>

#include"BehaviorTree.h"

class NodeBase;


class BehaviorData
{
public:

	//コンストラクタ
	BehaviorData() { Init(); }

	void PushSequenceNode(NodeBase* node) { m_pSequenceStack.push(node); }

	//シーケンスノードポップ
	NodeBase* PopSequenceNode();

	//シーケンスステップのゲッター
	int GetSequenceStep(std::string name);

	//シーケンスステップのセッター
	void SetSequenceStep(std::string name, int step);

	//初期化
	void Init();

private:

	//これは入れ子構造例えば中間ノードのシーケンスの中にさらに中間ノードのシーケンスとアクションノードがあったりしたときに使う
	std::stack<NodeBase*>m_pSequenceStack;//実行する中間ノードをスタック
	std::map<std::string, int> m_RunSequenceStepMap;
};
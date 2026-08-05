#pragma once

#include"JudgmentBase.h"
class AgentComponent;
class EnemyComponent;

class WanderJudgment :public JudgmentBase
{
public:
	WanderJudgment(GameObject* gameObject) :JudgmentBase(gameObject){}
	// 巡回行動へ入れる状態かをAgentComponentの状態から判定する。
	bool Judgment();

private:
	// 判定時に必要な移動/探索情報を保持するComponent参照。
	std::weak_ptr<AgentComponent> m_pAgentComponent;
};

class ChaseJudgment :public JudgmentBase
{
public:
	ChaseJudgment(GameObject*gameObject):JudgmentBase(gameObject){}
	// 敵が追跡状態へ移るべきかをEnemyComponentの検知情報から判定する。
	bool Judgment();

private:
	// Player検知や敵状態を確認するためのComponent参照。
	std::weak_ptr<EnemyComponent> m_pEnemyComponent;
};


class AttackJudgment :public JudgmentBase
{
public:
	AttackJudgment(GameObject* gameObject) :JudgmentBase(gameObject) {}
	// 攻撃可能距離や現在状態を見て、攻撃Actionへ移れるかを判定する。
	bool Judgment();

private:
	// 攻撃判定に必要なEnemyComponentを弱参照で保持する。
	std::weak_ptr<EnemyComponent> m_pEnemyComponent;
};

class CautionJudgment :public JudgmentBase
{
public:
	CautionJudgment(GameObject* gameObject) :JudgmentBase(gameObject) {}

	// 警戒状態へ入るべきかを、発見度や記憶中のPlayer位置から判定する。
	bool Judgment();

private:
	// 警戒状態の条件確認に使う敵情報。
	std::weak_ptr<EnemyComponent> m_pEnemyComponent;
};



class CheckJudgment :public JudgmentBase
{
public:
	CheckJudgment(GameObject* gameObject) :JudgmentBase(gameObject) {}

	// 気になる地点を確認する行動へ移れるかを判定する。
	bool Judgment();

private:
	// 確認行動の発生条件を読むための敵情報。
	std::weak_ptr<EnemyComponent> m_pEnemyComponent;
};


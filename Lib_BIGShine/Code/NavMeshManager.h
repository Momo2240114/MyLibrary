#pragma once

#include <DetourNavMesh.h>
#include <DetourCrowd.h>
#include <Recast.h>
#include "DetourNavMeshBuilder.h"

#include"ComponentBase.h"
#include "GlTFModel.h"

#include"NavMeshRenderer.h"
#include"AgentComponent.h"


struct Box
{
	//最小点と最大点
	Vector3 m_Max;
	Vector3 m_Min;

	std::vector<Vector3> m_BoxVertices;
	std::vector<float> m_BoxIndices;
};


class NaviMeshManager
{
public:
	void Initialize();

	void SetModel(GltfModelCommponennt* modelComponent);

	void SetBoxCollider(GameObject* gameObject);

	void Build();

	void Update(float et);

	void Render();

	void Clean();

	void Clear();

	void Gui();

	void Reset(int index);

	//const Vector3& SetTargetPosition(int index,Vector3& position,float radius);

	const Vector3& SetRandomPosition(int index, Vector3& position, float radius);

	const Vector3& SetTargetPosition(int index, Vector3& position);

	//目標のスピードを指定する
	void RequestMoveVelocity(int index,float speed);

	const dtCrowdAgent* GetAgent(int index)
	{
		return m_pCrowd->getAgent(index);
	}
	dtCrowdAgent* GetEditableAgent(int index)
	{
		return m_pCrowd->getEditableAgent(index);
	}

	//パラメータの更新
	void ParamUpdate(int index, dtCrowdAgentParams* param)
	{
		m_pCrowd->updateAgentParameters(index,param);
	}

	//ナビメッシュ取得
	dtNavMesh* GetNavMesh() { return m_pNavMesh; }

	//ナビメッシュクエリ取得
	dtNavMeshQuery* GetNavMeshQuery() { return m_pNavQuery; }

	//詳細ポリゴンメッシュ取得
	rcPolyMeshDetail* GetPolyMeshDetail() { return m_pPolyMeshDetail; }

	//群衆
	dtCrowd* GetCrowd() { return m_pCrowd; }

	int AddAgent(float* p, dtCrowdAgentParams* ap);

	void AddAgentComponent(AgentComponent* agent)
	{
		m_pAgentComponents.emplace_back(agent);
	}

	//std::vector<Vector3>& GetPosition()
	//{
	//	return m_Positions;
	//}
private:
	//ナビメッシュをするモデル
	std::vector<GltfModelCommponennt*> m_pModelComponents;

	//std::vector<Vector3> m_Positions;


	rcConfig				m_Config;

	rcHeightfield* m_pHeightField = nullptr;
	rcCompactHeightfield* m_pCompactHeightField = nullptr;
	rcContourSet* m_pContourSet = nullptr;
	rcPolyMesh* m_pPolyMesh = nullptr;
	rcPolyMeshDetail* m_pPolyMeshDetail = nullptr;

	//ナビ
	dtNavMesh* m_pNavMesh = nullptr;

	//経路探索
	dtNavMeshQuery* m_pNavQuery = nullptr;

	//エージェント
	dtCrowd* m_pCrowd = nullptr;

	rcContext m_Context;

	float m_CellSize = 0.3f;
	float m_CellHeight = 0.2f;
	float m_AgentHeight = 2.0f;
	float m_AgentRadius = 0.6f;
	float m_AgentMaxClimb = 0.9f;
	float m_AgentMaxSlope = 45.0f;
	float m_RegionMinSize = 8;
	float m_RegionMergeSize = 20;
	float m_EdgeMaxLen = 12.0f;
	float m_EdgeMaxError = 1.3f;
	float m_VertsPerPoly = 6.0f;
	float m_DetailSampleDist = 6.0f;
	float m_DetailSampleMaxError = 1.0f;


	Vector3 m_TargetPosition = {0,0,0};

	//std::unique_ptr<NavMeshRenderer> m_pNaviMeshRenderer;

	std::vector<AgentComponent*> m_pAgentComponents;

	std::vector<Matrix> m_pBoxColliderMatrixs;

	std::vector<Box> m_BoxColliderInfos;

	int m_Index = 0;

	Vector3 m_Pos;

	//エージェントのステート
	dtStatus m_Status;

};
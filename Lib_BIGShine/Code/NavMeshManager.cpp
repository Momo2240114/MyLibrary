

#include"NavMeshManager.h"
#include"Vector.h"

#include "misc.h"
#include"GameObjectManager.h"

#include"ShapeRenderer.h"


#include"RandomHelper.h"
#include"BoxCollider.h"
#include"GraphicsDeviceManager.h"
float MyRandomFunc()
{
	return (float)rand() / (float)RAND_MAX;
}


void NaviMeshManager::Initialize()
{
	//NaviMeshのデバッグ用レンダラー
	//m_pNaviMeshRenderer = std::make_unique<NavMeshRenderer>();
}

void NaviMeshManager::SetModel(GltfModelCommponennt* modelComponent)
{
	GltfModelCommponennt*& model = m_pModelComponents.emplace_back();
	model = modelComponent;

	//ここでワールド座標を計算しておく(これをしないとBuildの時にワールド座標が適応されない)
	model->GetGameObject()->UpdateTransform();

	//今までの処理ではUpdateTarnsformをよんだあとに呼ばれないといけない処理を
	//GameObjectManagerのfor分で呼ばれているためWorldTransoformが1フレームずれてしまう問題が起きる!(直すのは厳しい)
	model->Update(1);
}

void NaviMeshManager::SetBoxCollider(GameObject* gameObject)
{
	// コンポーネントからBoxColliderを取得
	auto boxCollider = gameObject->GetComponent<BoxCollider>();
	if (!boxCollider) return;

	// 最新のトランスフォームを反映
	gameObject->UpdateTransform();
	// 1フレームずれ対策（既存の仕様を維持）
	gameObject->Update(1);

	using namespace DirectX;

	// 1. 基本となるオブジェクトのワールド行列を取得
	XMMATRIX world = gameObject->transform->GetWorldMatrix();

	// 2. コライダーの halfSize をスケールとして適用
	// boxVerticesが -1.0 ～ 1.0 なので、halfSizeを掛けると正しい半径になる
	XMFLOAT3 hs = boxCollider->halfSize;
	XMMATRIX mScale = XMMatrixScaling(hs.x * 2, hs.y * 2, hs.z * 2);

	// 3. コライダーの offset を適用
	XMFLOAT3 off = boxCollider->offset;
	XMMATRIX mOffset = XMMatrixTranslation(off.x, off.y, off.z);

	// 4. 行列を合成 (コライダー内スケーリング -> コライダー内オフセット -> オブジェクトのワールド変換)
	// これにより、オブジェクトの回転や親のスケールも正しく適用される
	XMMATRIX finalMat = mScale * mOffset * world;
	Matrix& mat = m_pBoxColliderMatrixs.emplace_back();
	XMStoreFloat4x4(&mat, finalMat);

	//Matrix& mat = m_pBoxColliderMatrixs.emplace_back();
	//mat = world;

	//m_BoxColliderInfos.emplace_back();
}

void NaviMeshManager::Build()
{
	Clean();


	//モデルから頂点抽出
	std::vector<Vector3> vertices;
	std::vector<int> indices;
	Vector3 boundsMin, boundsMax;

	//ナビメッシュをするすべてのモデル
	for (const GltfModelCommponennt* model : m_pModelComponents)
	{
		//モデルのサブメッシュ分for分を回す
		for (const GltfModelCommponennt::Mesh& mesh : model->GetMeshes())
		{
			std::vector<Vector3> m_Positions;


			//ワールド座標の頂点を取得する
			for (const GltfModelCommponennt::Vertex& vertex : mesh.vertices)
			{
				DirectX::XMVECTOR V = DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&vertex.position), DirectX::XMLoadFloat4x4(&mesh.node->worldTransform));
				m_Positions.emplace_back(V);
			}

			//トライアングルリスト最適化(重複する頂点を削除)
			std::map<Vector3, int> cache;
			for (int i = 0; i < mesh.indices.size(); i++)
			{
				uint32_t index = mesh.indices[i];

				//頂点数
				int n = static_cast<uint32_t>(vertices.size());

				//頂点データ
				const DirectX::XMFLOAT3& v = m_Positions.at(index);

				auto it = cache.find(v);


				if (it == cache.end())
				{
					cache[v] = n;
					it = cache.find(v);

					vertices.emplace_back(v);
				}

				//頂点の番号を取得する
				n = it->second;

				//頂点の番号を保存する
				indices.emplace_back(n);
			}
		}
	}

	//Boxを作成
	DirectX::XMFLOAT3 positions[8] =
	{
		// top
		{ -1.0f,  1.0f, -1.0f},
		{  1.0f,  1.0f, -1.0f},
		{  1.0f,  1.0f,  1.0f},
		{ -1.0f,  1.0f,  1.0f},
		// bottom
		{ -1.0f, -1.0f, -1.0f},
		{  1.0f, -1.0f, -1.0f},
		{  1.0f, -1.0f,  1.0f},
		{ -1.0f, -1.0f,  1.0f},
	};

	std::vector<Vector3> boxVertices;

	std::vector<uint32_t> boxIndices;
	boxIndices.resize(36);
	// top
	boxVertices.emplace_back(positions[0]);
	boxVertices.emplace_back(positions[1]);
	boxVertices.emplace_back(positions[2]);
	boxVertices.emplace_back(positions[3]);
	boxVertices.emplace_back(positions[4]);
	boxVertices.emplace_back(positions[5]);
	boxVertices.emplace_back(positions[6]);
	boxVertices.emplace_back(positions[7]);

	boxIndices = {
		// 上面 (Top: 0,1,2,3) - 上から見て反時計回り
		3, 2, 1,
		3, 1, 0,

		// 底面 (Bottom: 4,5,6,7) - 下から見て反時計回り
		4, 5, 6,
		4, 6, 7,

		// 前面 (Front: 0,1,4,5) - 前から見て反時計回り
		0, 4, 5,
		0, 5, 1,

		// 背面 (Back: 2,3,6,7) - 後ろから見て反時計回り
		2, 6, 7,
		2, 7, 3,

		// 左側面 (Left: 0,3,4,7) - 左から見て反時計回り
		3, 7, 4,
		3, 4, 0,

		// 右側面 (Right: 1,2,5,6) - 右から見て反時計回り
		1, 5, 6,
		1, 6, 2
	};

	//int index = 0;
	//BoxCollider用
	for (Matrix& mat : m_pBoxColliderMatrixs)
	{
		std::vector<Vector3> positions;
		for (int i = 0; i < boxVertices.size(); i++)
		{
			//ワールド座標にした頂点を保存する
			Vector3 p = positions.emplace_back(DirectX::XMVector3Transform(boxVertices[i], mat));
			//m_BoxColliderInfos[index].m_BoxVertices.emplace_back(p);
		}

		//トライアングルリスト最適化(重複する頂点を削除)
		std::map<Vector3, int> cache;
		for (int i = 0; i < boxIndices.size(); i++)
		{
			uint32_t index = boxIndices[i];

			//頂点数
			int n = static_cast<uint32_t>(vertices.size());

			//頂点データ
			const DirectX::XMFLOAT3& v = positions.at(index);

			auto it = cache.find(v);


			if (it == cache.end())
			{
				cache[v] = n;
				it = cache.find(v);

				vertices.emplace_back(v);
			}

			//頂点の番号を取得する
			n = it->second;

			//頂点の番号を保存する
			indices.emplace_back(n);
		}
		//index++;
	}

	//バウンディングボックス計算
	//渡されたMesh出たーを覆うAABVBを作る
	rcCalcBounds(&vertices[0].x, static_cast<int>(vertices.size()), &boundsMin.x, &boundsMax.x);

	//ナビメッシュ
	{
		bool result;

		//初期化
		memset(&m_Config, 0, sizeof(m_Config));

		//先ほど求めたAABBをさらに細分化
		m_Config.cs = m_CellSize;
		m_Config.ch = m_CellHeight;

		//移動できる角度
		m_Config.walkableSlopeAngle = m_AgentMaxSlope;

		//プレイヤーが通れるボクセル数
		m_Config.walkableHeight = (int)ceilf(m_AgentHeight / m_Config.ch);

		//乗り越えられる降りれるボクセル数
		m_Config.walkableClimb = (int)floorf(m_AgentMaxClimb / m_Config.ch);

		//壁からどのくらいのボクセル離れるか
		m_Config.walkableRadius = (int)ceilf(m_AgentRadius / m_Config.cs);

		//何マス分の誤差を許すのか
		m_Config.maxSimplificationError = m_EdgeMaxError;

		//引かれた直線がどのくらいになると頂点を追加するのか
		m_Config.maxEdgeLen = (int)(m_EdgeMaxLen / m_CellSize);

		//リージョンの面積が指定よりも小さければ削除
		m_Config.minRegionArea = (int)rcSqr(m_RegionMinSize);

		//リージョンの面積が指定よりも小さければ結合
		m_Config.mergeRegionArea = (int)rcSqr(m_RegionMergeSize);

		//ポリゴンを作る時の最大頂点
		m_Config.maxVertsPerPoly = (int)m_VertsPerPoly;

		//垂直方向の直線は何マス分の誤差を許すのか
		m_Config.detailSampleMaxError = m_CellHeight * m_DetailSampleMaxError;

		//垂直方向の直線の長さの最大ボクセル数
		m_Config.detailSampleDist = m_DetailSampleDist < 0.9f ? 0 : m_CellSize * m_DetailSampleDist;

		//ステージの最小値と最大値をコピー
		rcVcopy(m_Config.bmin, &boundsMin.x);
		rcVcopy(m_Config.bmax, &boundsMax.x);

		//AABBの広さをボクセル単位に置き換える
		rcCalcGridSize(m_Config.bmin, m_Config.bmax, m_Config.cs, &m_Config.width, &m_Config.height);

		//ボクセルデータを保存するメモリ領域を確保する
		m_pHeightField = rcAllocHeightfield();
		_ASSERT_EXPR_A(m_pHeightField, "buildNavigation: Out of memory 'solid'.");

		//確保したメモリ領域を分割する
		result = rcCreateHeightfield(&m_Context, *m_pHeightField, m_Config.width, m_Config.height, m_Config.bmin, m_Config.bmax, m_Config.cs, m_Config.ch);
		_ASSERT_EXPR_A(result, "buildNavigation: Could not create solid heightfield.");

		//ポリゴン数を計算する
		int triCount = static_cast<int>(indices.size() / 3);

		//ポリゴンをチェックするメモ
		std::unique_ptr<u_char[]> triareas = std::make_unique<u_char[]>(triCount);
		_ASSERT_EXPR_A(triareas, "buildNavigation: Out of memory 'm_triareas");

		//すべて初期化
		memset(triareas.get(), 0, triCount * sizeof(u_char));

		//頂点情報の先頭アドレス
		float* verts = &vertices[0].x;

		//インデックス情報の先頭アドレス
		int* tris = indices.data();

		//頂点数
		int vertCount = static_cast<int>(vertices.size());

		//歩ける角度のポリゴン化を確認する
		rcMarkWalkableTriangles(&m_Context, m_Config.walkableSlopeAngle, verts, vertCount, indices.data(), triCount, triareas.get());

		//先ほどのポリゴンをもとにボクセル化させる(歩ける場所と歩けない場所を管理する)
		result = rcRasterizeTriangles(&m_Context, verts, vertCount, tris, triareas.get(), triCount, *m_pHeightField, m_Config.walkableClimb);
		_ASSERT_EXPR_A(result, "buildNavigation: Could not rasterize triangles.");

		//フィルタリング
		//歩ける段差だった場合そのボクセルを歩ける領域と指定しなおす
		rcFilterLowHangingWalkableObstacles(&m_Context, m_Config.walkableClimb, *m_pHeightField);

		//通れるボクセルの隣がClimpを超えていたらそこと隣接するボクセルは歩けない領域と指定しなおす
		rcFilterLedgeSpans(&m_Context, m_Config.walkableHeight, m_Config.walkableClimb, *m_pHeightField);

		//プレイヤーが通れない高さの天井画あるところは歩けない領域に指定しなおす
		rcFilterWalkableLowHeightSpans(&m_Context, m_Config.walkableHeight, *m_pHeightField);

		//コンパクト化 
		//床の情報だけに絞り込んだメモリ領域を使う
		m_pCompactHeightField = rcAllocCompactHeightfield();
		_ASSERT_EXPR_A(m_pCompactHeightField, "buildNavigation: Out of memory 'chf'.");

		//厚みの情報を捨て,各セルの床の高さと天井までの空きだけのリスト(スパン)にする
		result = rcBuildCompactHeightfield(&m_Context, m_Config.walkableHeight, m_Config.walkableClimb, *m_pHeightField, *m_pCompactHeightField);
		_ASSERT_EXPR_A(result, "buildNavigation: Could not build compact data.");

		//キャラクターの半径分だけ壁際や崖際を削る
		result = rcErodeWalkableArea(&m_Context, m_Config.walkableRadius, *m_pCompactHeightField);
		_ASSERT_EXPR_A(result, "buildNavigation: Could not erode.");

		//地域分けの準備をする(壁からの距離を計算)
		result = rcBuildDistanceField(&m_Context, *m_pCompactHeightField);
		_ASSERT_EXPR_A(result, "buildNavigation: Could not build distance field.");

		//地域分け(リージョン化)バラバラの床をグループ化
		result = rcBuildRegions(&m_Context, *m_pCompactHeightField, 0, m_Config.minRegionArea, m_Config.mergeRegionArea);

		//縁取り グループの境界を線にする

		// 1. 抽出した「輪郭（境界線）」を格納するメモリを確保
		m_pContourSet = rcAllocContourSet();
		_ASSERT_EXPR_A(m_pContourSet, "buildNavigation: Out of memory 'cset'.");

		// 2. ボクセルのギザギザな縁をなぞり、指定の誤差範囲で「直線」に簡略化する
		result = rcBuildContours(&m_Context, *m_pCompactHeightField, m_Config.maxSimplificationError, m_Config.maxEdgeLen, *m_pContourSet);
		_ASSERT_EXPR_A(result, "buildNavigation: Could not create contours.");

		//面データを保存するメモリを確保
		m_pPolyMesh = rcAllocPolyMesh();
		_ASSERT_EXPR_A(m_pPolyMesh, "buildNavigation: Out of memory 'pmesh'.");

		//先ほど作った境界線やポリゴンの頂点数に合うようにポリゴンを作成
		result = rcBuildPolyMesh(&m_Context, *m_pContourSet, m_Config.maxVertsPerPoly, *m_pPolyMesh);
		_ASSERT_EXPR_A(result, "buildNavigation: Could not triangulate contours.");

		//詳細メッシュを保存するメモリを確保する
		m_pPolyMeshDetail = rcAllocPolyMeshDetail();
		_ASSERT_EXPR_A(m_pPolyMeshDetail, "buildNavigation: Out of memory 'pmdtl'.");

		//詳細メッシュを指定範囲の誤差範囲で直線に簡略化する
		result = rcBuildPolyMeshDetail(&m_Context, *m_pPolyMesh, *m_pCompactHeightField, m_Config.detailSampleDist, m_Config.detailSampleMaxError, *m_pPolyMeshDetail);

		//通行フラグの設定
		// 基本の通行許可フラグ（1）を割り当てる。これによりAIがここを通れるようになる。
		for (int i = 0; i < m_pPolyMesh->npolys; i++)
		{
			if (m_pPolyMesh->areas[i] == RC_WALKABLE_AREA)
			{
				m_pPolyMesh->flags[i] = 1;
			}

		}

		dtNavMeshCreateParams params;
		memset(&params, 0, sizeof(params));

		//ナビメッシュの頂点データ
		params.verts = m_pPolyMesh->verts;

		//ナビメッシュの頂点数
		params.vertCount = m_pPolyMesh->nverts;

		//ポリゴンの接続情報(インデックス + 隣接ポリゴンID)
		params.polys = m_pPolyMesh->polys;

		//生成された多角形タイルの数
		params.polyCount = m_pPolyMesh->npolys;

		//エリアIDのリスト
		params.polyAreas = m_pPolyMesh->areas;

		//通行許可フラグ
		params.polyFlags = m_pPolyMesh->flags;

		//ポリゴンの頂点数
		params.nvp = m_pPolyMesh->nvp;

		//各ポリゴンがどの詳細データを参照するのかを記した目次
		params.detailMeshes = m_pPolyMeshDetail->meshes;

		//頂点情報
		params.detailVerts = m_pPolyMeshDetail->verts;

		//頂点数
		params.detailVertsCount = m_pPolyMeshDetail->nverts;

		//インデックス
		params.detailTris = m_pPolyMeshDetail->tris;

		//三角形の数
		params.detailTriCount = m_pPolyMeshDetail->ntris;

		//ショートカット移動
		//スタート地点とゴール地点の座標リスト
		params.offMeshConVerts = nullptr;

		//スタート地点の反tネイの大きさ
		params.offMeshConRad = nullptr;

		//一方向,往復
		params.offMeshConDir = nullptr;

		//そのルートの名前
		params.offMeshConAreas = nullptr;

		//通行許可のフラグ
		params.offMeshConFlags = nullptr;

		//それぞれのショートカットにIDをつける
		params.offMeshConUserID = nullptr;

		//ショートカット数
		params.offMeshConCount = 0;


		//キャラクターの身長
		params.walkableHeight = m_AgentHeight;

		//キャラクターの半径
		params.walkableRadius = m_AgentRadius;

		//キャラクターの超えられる段差
		params.walkableClimb = m_AgentMaxClimb;

		//ステージ全体を包むAABBをコピーする
		rcVcopy(params.bmin, m_pPolyMesh->bmin);
		rcVcopy(params.bmax, m_pPolyMesh->bmax);


		//すべての頂点データをfloatではなくてボクセル単位で持たせているため必要
		//ボクセルのサイズ
		params.cs = m_Config.cs;
		params.ch = m_Config.ch;

		//BV-Tree（境界ボリュームツリー）の有効化
		params.buildBvTree = true;

		//params(頂点情報やボクセルサイズなど)を,Detourが読み込める独自のバイナリ形式に変換する
		unsigned char* navData = 0;
		int navDataSize = 0;
		result = dtCreateNavMeshData(&params, &navData, &navDataSize);
		_ASSERT_EXPR_A(result, "Could not build Detour navmesh");

		//ナビメッシュデータ
		m_pNavMesh = dtAllocNavMesh();
		_ASSERT_EXPR_A(m_pNavMesh, "Could not create Detour navmesh");

		dtStatus status;

		//ナビデータを作成する
		status = m_pNavMesh->init(navData, navDataSize, DT_TILE_FREE_DATA);
		_ASSERT_EXPR_A(dtStatusSucceed(status), "Could not init Detour navmesh");

		//ルートを計算するナビシステム
		m_pNavQuery = dtAllocNavMeshQuery();
		_ASSERT_EXPR_A(m_pNavMesh, "Could not create Detour navmesh query");

		status = m_pNavQuery->init(m_pNavMesh, 2048);
		_ASSERT_EXPR_A(dtStatusSucceed(status), "Could not init Detour navmesh query");

		//群衆,エージェントとのシステム
		m_pCrowd = dtAllocCrowd();
		_ASSERT_EXPR_A(m_pCrowd, "Could not create Detour crowd");

		result = m_pCrowd->init(128, m_AgentRadius, m_pNavMesh);
		_ASSERT_EXPR_A(result, "Could not init Detour crowd");

		//dtCrowdAgentParams ap;
		//memset(&ap, 0, sizeof(ap));

		////キャラクターの半径
		//ap.radius = 0.5f;

		////キャラクターの高さ
		//ap.height = 1.0f;

		////どれだけ早くトピスピードに達するのか
		//ap.maxAcceleration = 8.0f;

		////キャラクターの最大速度
		//ap.maxSpeed = 3.5;

		////他キャラを察知する範囲
		//ap.collisionQueryRange = ap.radius * 12.0f;

		////経路をショートカットできるのかを見る範囲
		//ap.pathOptimizationRange = ap.radius * 30.0f;

		////AIの賢さ
		//ap.updateFlags = 0;
		//ap.updateFlags |= DT_CROWD_ANTICIPATE_TURNS;
		//ap.updateFlags |= DT_CROWD_OPTIMIZE_VIS;
		//ap.updateFlags |= DT_CROWD_OPTIMIZE_TOPO;
		//ap.updateFlags |= DT_CROWD_OBSTACLE_AVOIDANCE;
		//ap.updateFlags |= DT_CROWD_SEPARATION;

		////回避パターンのプリセット
		//ap.obstacleAvoidanceType = 3;
		//
		////他のキャラと離れようとする強さ
		//ap.separationWeight = 2.0f;

		//float p[3] = { 0,0,0 };

		//////キャラクターを追加する
		//m_pCrowd->addAgent(p, &ap);
	}
}

void NaviMeshManager::Update(float et)
{
	if (m_pModelComponents.empty())return;


	for (int i = 0; i < m_pAgentComponents.size(); i++)
	{
		Vector3 currentPosition;
		currentPosition.x = m_pCrowd->getAgent(m_pAgentComponents[i]->GetAgentIndex())->npos[0];
		currentPosition.y = m_pCrowd->getAgent(m_pAgentComponents[i]->GetAgentIndex())->npos[1];
		currentPosition.z = m_pCrowd->getAgent(m_pAgentComponents[i]->GetAgentIndex())->npos[2];

		m_pAgentComponents[i]->SetCurrentPositiion(currentPosition);
	}

	m_pCrowd->update(et, nullptr);
}

void NaviMeshManager::Render()
{
	//m_pNaviMeshRenderer->Render();
	//for (Box& box : m_BoxColliderInfos)
	//{
	//	GraphicsDeviceManager::Instance().GetContext() ->Draw():
	//}

}

void NaviMeshManager::Clean()
{
	//地形をボクセル化したデータ
	rcFreeHeightField(m_pHeightField);
	m_pHeightField = nullptr;

	//コンパクト化したデータ
	rcFreeCompactHeightfield(m_pCompactHeightField);
	m_pCompactHeightField = nullptr;
	//コンパクト化したデータの輪郭のデータ
	rcFreeContourSet(m_pContourSet);
	m_pContourSet = nullptr;
	//縁をポリゴンに変更したメッシュデータ
	rcFreePolyMesh(m_pPolyMesh);
	m_pPolyMesh = nullptr;
	//ポリゴンに高さ情報(凹凸)を追加した詳細データ
	rcFreePolyMeshDetail(m_pPolyMeshDetail);
	m_pPolyMeshDetail = nullptr;

	//NavMeshデータ
	dtFreeNavMesh(m_pNavMesh);
	m_pNavMesh = nullptr;

	//経路探索などの計算をするクラス
	dtFreeNavMeshQuery(m_pNavQuery);
	m_pNavQuery = nullptr;

	////群衆シュミレーション用のマネージャー
	dtFreeCrowd(m_pCrowd);
	m_pCrowd = nullptr;
}

void NaviMeshManager::Clear()
{
	if (!m_pModelComponents.empty())
	{
		m_pModelComponents.clear();
	}

	if (!m_pAgentComponents.empty())
	{
		m_pAgentComponents.clear();
	}
	if (!m_pBoxColliderMatrixs.empty())
	{
		m_pBoxColliderMatrixs.clear();
	}
}

void NaviMeshManager::Gui()
{
	ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImVec2(300, 350), ImGuiCond_Once);

	if (ImGui::Begin("NavMesh", nullptr, ImGuiWindowFlags_None))
	{
		ImGui::DragFloat("cellSize", &m_CellSize, 0.01f, 0.01f, 10.0f, "%.3f");
		ImGui::DragFloat("cellHeight", &m_CellHeight, 0.01f, 0.01f, 10.0f, "%.3f");
		ImGui::DragFloat("agentHeight", &m_AgentHeight, 0.01f, 0.01f, 10.0f, "%.3f");
		ImGui::DragFloat("agentRadius", &m_AgentRadius, 0.01f, 0.01f, 10.0f, "%.3f");
		ImGui::DragFloat("agentMaxClimb", &m_AgentMaxClimb, 0.01f, 0.01f, 10.0f, "%.3f");
		ImGui::DragFloat("agentMaxSlope", &m_AgentMaxSlope, 0.0f, 1.0f, 90.0f, "%.3f");
		ImGui::DragFloat("regionMinSize", &m_RegionMinSize, 0.1f, 0.1f, 100.0f, "%.3f");
		ImGui::DragFloat("regionMergeSize", &m_RegionMergeSize, 0.1f, 0.1f, 100.0f, "%.3f");
		ImGui::DragFloat("edgeMaxLen", &m_EdgeMaxLen, 0.1f, 0.1f, 100.0f, "%.3f");
		ImGui::DragFloat("edgeMaxError", &m_EdgeMaxError, 0.1f, 0.1f, 100.0f, "%.3f");
		ImGui::DragFloat("vertsPerPoly", &m_VertsPerPoly, 0.1f, 0.1f, 100.0f, "%.3f");
		ImGui::DragFloat("detailSampleDist", &m_DetailSampleDist, 0.1f, 0.1f, 100.0f, "%.3f");
		ImGui::DragFloat("detailSampleMaxError", &m_DetailSampleMaxError, 0.1f, 0.1f, 100.0f, "%.3f");
		if (ImGui::Button("Build"))
		{
			Build();
		}

		ImGui::InputFloat3("TargetPos", &m_Pos.x);
	}
	ImGui::End();
}

//const Vector3& NaviMeshManager::SetTargetPosition(int index, Vector3& position,float radius)
//{
//	//ポリゴンID
//	dtPolyRef polyRef;
//
//	//フィルターを取得する(全ての通路を同じコストで動く)
//	const dtQueryFilter* filter = m_pCrowd->getFilter(0);
//
//	//hitPositionからポリゴンを検索する範囲
//	//const float* halfExtents = m_pCrowd->getQueryExtents();
//	float customExtents[3] = { 10.0f, 1.0f, 10.0f };
//
//	//ターゲットポジションを代入する
//	//m_TargetPosition = m_Pos;
//
//
//	//移動するポリゴンを取得する
//	dtStatus status = m_pNavQuery->findNearestPoly(&position.x, customExtents, filter, &polyRef, &m_Pos.x);
//
//	if (dtStatusFailed(status) || polyRef == 0)
//	{
//		assert(L"ポリゴン発見できなかった");
//	}
//
//	//const int MAX_POLYS = 128;        // 最大128個まで覚えられる
//	//dtPolyRef resultPolys[MAX_POLYS]; // ポリゴンIDを入れる配列
//	//int resultCount;
//
//	////指定範囲内のポリゴンをすべて取得する
//	//m_pNavQuery->findPolysAroundCircle(
//	//	polyRef,
//	//	&position.x,
//	//	radius,
//	//	filter,
//	//	resultPolys,    // 結果はここに入る
//	//	NULL,           // 親ポリゴン参照（通常はNULLでOK）
//	//	NULL,           // コスト（通常はNULLでOK）
//	//	&resultCount,   // 「〇個見つかったよ」とここに書き込まれる
//	//	MAX_POLYS       // 箱の限界サイズ
//	//);
//
//	//int random = RandomRangeCreat(0, resultCount - 1);
//
//
//	// 3. 選んだ床の中で「具体的な座標」を1つ決める！
//	// ★これが欲しかった処理です！
//	//m_pNavQuery->findRandomPoint(filter, MyRandomFunc, &resultPolys[random], &targetPos.x);
//	//m_pNavQuery->closestPointOnPoly(resultPolys[random],&position.x, &targetPos.x,NULL);
//	//targetPos.y = 0;
//
//	Vector3 targetPos;
//	dtPolyRef randomPolyRef;
//	status = m_pNavQuery->findRandomPointAroundCircle(
//		polyRef,           // 中心のポリゴン
//		&position.x,       // 中心座標
//		radius,            // 半径
//		filter,            // フィルター
//		MyRandomFunc,      // ランダム関数
//		&randomPolyRef,    // 結果のポリゴンID
//		&targetPos.x       // 結果の座標
//	);
//
//	//目的地を設定する
//	bool request = m_pCrowd->requestMoveTarget(index,randomPolyRef, &targetPos.x);
//	assert(request,L"リクエスト失敗");
//
//	position = m_Pos;
//	return targetPos;
//	//return m_Pos;
//}

void NaviMeshManager::Reset(int index)
{
	m_pCrowd->resetMoveTarget(index);
	//m_pNavQuery->moveAlongSurface

}

const Vector3& NaviMeshManager::SetRandomPosition(int index, Vector3& position, float radius)
{
	//ポリゴンID
	dtPolyRef polyRef;

	//フィルターを取得する(全ての通路を同じコストで動く)
	const dtQueryFilter* filter = m_pCrowd->getFilter(0);

	//hitPositionからポリゴンを検索する範囲
	//const float* halfExtents = m_pCrowd->getQueryExtents();
	float customExtents[3] = { 10.0f, 1.0f, 10.0f };

	//ターゲットポジションを代入する
	//m_TargetPosition = m_Pos;


	//移動するポリゴンを取得する
	dtStatus status = m_pNavQuery->findNearestPoly(&position.x, customExtents, filter, &polyRef, &m_Pos.x);

	if (dtStatusFailed(status) || polyRef == 0)
	{
		assert(L"ポリゴン発見できなかった");
	}

	//const int MAX_POLYS = 128;        // 最大128個まで覚えられる
	//dtPolyRef resultPolys[MAX_POLYS]; // ポリゴンIDを入れる配列
	//int resultCount;

	////指定範囲内のポリゴンをすべて取得する
	//m_pNavQuery->findPolysAroundCircle(
	//	polyRef,
	//	&position.x,
	//	radius,
	//	filter,
	//	resultPolys,    // 結果はここに入る
	//	NULL,           // 親ポリゴン参照（通常はNULLでOK）
	//	NULL,           // コスト（通常はNULLでOK）
	//	&resultCount,   // 「〇個見つかったよ」とここに書き込まれる
	//	MAX_POLYS       // 箱の限界サイズ
	//);

	//int random = RandomRangeCreat(0, resultCount - 1);


	// 3. 選んだ床の中で「具体的な座標」を1つ決める！
	// ★これが欲しかった処理です！
	//m_pNavQuery->findRandomPoint(filter, MyRandomFunc, &resultPolys[random], &targetPos.x);
	//m_pNavQuery->closestPointOnPoly(resultPolys[random],&position.x, &targetPos.x,NULL);
	//targetPos.y = 0;

	Vector3 targetPos;
	dtPolyRef randomPolyRef;
	status = m_pNavQuery->findRandomPointAroundCircle(
		polyRef,           // 中心のポリゴン
		&position.x,       // 中心座標
		radius,            // 半径
		filter,            // フィルター
		MyRandomFunc,      // ランダム関数
		&randomPolyRef,    // 結果のポリゴンID
		&targetPos.x       // 結果の座標
	);

	//目的地を設定する
	bool request = m_pCrowd->requestMoveTarget(index, randomPolyRef, &targetPos.x);
	assert(request, L"リクエスト失敗");

	position = m_Pos;
	return targetPos;
	//return m_Pos;
}

const Vector3& NaviMeshManager::SetTargetPosition(int index, Vector3& position)
{
	//ポリゴンID
	dtPolyRef polyRef;

	//フィルターを取得する(全ての通路を同じコストで動く)
	const dtQueryFilter* filter = m_pCrowd->getFilter(0);

	//hitPositionからポリゴンを検索する範囲
	//const float* halfExtents = m_pCrowd->getQueryExtents();
	float customExtents[3] = { 10.0f, 2.0f, 10.0f };

	float pos[3] = { position.x,position.y,position.z };
	//ターゲットポジションを代入する
	//m_TargetPosition = m_Pos;


	//移動するポリゴンを取得する
	m_Status = m_pNavQuery->findNearestPoly(&pos[0], customExtents, filter, &polyRef, &m_Pos.x);

	if (dtStatusFailed(m_Status) || polyRef == 0)
	{
		assert(L"ポリゴン発見できなかった");
	}

	//目的地を設定する
	bool request = m_pCrowd->requestMoveTarget(index, polyRef, &m_Pos.x);
	assert(request, L"リクエスト失敗");

	return m_Pos;
}

//進むスピードを設定する
void NaviMeshManager::RequestMoveVelocity(int index, float speed)
{
	bool request = m_pCrowd->requestMoveVelocity(index, &speed);
	assert(request, L"リクエスト失敗");
}

int NaviMeshManager::AddAgent(float* p, dtCrowdAgentParams* ap)
{
	//ここでm_Index++にしていたせいでリセットしたときにindexが1で始まってしまっていたのが原因
	m_Index = m_pCrowd->addAgent(p, ap);
	return m_Index;
}



//const Vector3& NaviMeshManager::GetAgentPosition(int index)
//{
//	Vector3 currentPosition;
//	currentPosition.x = m_pCrowd->getAgent(index)->npos[0];
//	currentPosition.x = m_pCrowd->getAgent(index)->npos[1];
//	currentPosition.x = m_pCrowd->getAgent(index)->npos[2];
//
//	for(int i = 0;i < )
//	{
//
//	}
//}

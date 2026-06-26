#include "ShapeRenderer.h"
#include "Engine/Common/misc.h"
#include "Engine/Graphics/Core/CPUResourceLoader.h"


void ShapeRenderSystem::Initialize(ID3D11Device* device)
{
	// 入力レイアウト
	D3D11_INPUT_ELEMENT_DESC inputElementDesc[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	// 頂点シェーダー
	CPUResourceLoader::LoadVertexShader(
		device,
		"Shader/ShapeRendererVS.cso",
		inputElementDesc,
		_countof(inputElementDesc),
		inputLayout.GetAddressOf(),
		vertexShader.GetAddressOf());

	// ピクセルシェーダー
	CPUResourceLoader::LoadPixelShader(
		device,
		"Shader/ShapeRendererPS.cso",
		pixelShader.GetAddressOf());

	// 定数バッファ
	CPUResourceLoader::CreateConstantBuffer(
		device,
		sizeof(CbMesh),
		constantBuffer.GetAddressOf());

	// 箱メッシュ生成
	CreateBoxMesh(device, 1.0f, 1.0f, 1.0f);
	CreateUnitBoxMesh(device);

	// 球メッシュ生成
	CreateSphereMesh(device, 1.0f, 32);

	// 半球メッシュ生成
	CreateHalfSphereMesh(device, 1.0f, 32);

	// 円柱メッシュ生成
	CreateCylinderMesh(device, 1.0f, 1.0f, -0.5f, 1.0f, 64);

	// 骨メッシュ生成
	CreateBoneMesh(device, 1.0f);
}


// 箱描画
void ShapeRenderSystem::DrawBox(
	const DirectX::XMFLOAT3& position,
	const DirectX::XMFLOAT3& angle,
	const DirectX::XMFLOAT3& size,
	const DirectX::XMFLOAT4& color)
{
	Instance& instance = instances.emplace_back();
	instance.mesh = &boxMesh;
	instance.color = color;

	DirectX::XMMATRIX S = DirectX::XMMatrixScaling(size.x, size.y, size.z);
	DirectX::XMMATRIX R = DirectX::XMMatrixRotationRollPitchYaw(angle.x, angle.y, angle.z);
	DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(position.x, position.y, position.z);
	DirectX::XMStoreFloat4x4(&instance.worldTransform, S * R * T);
}

// 球描画
void ShapeRenderSystem::DrawSphere(
	const DirectX::XMFLOAT3& position,
	float radius,
	const DirectX::XMFLOAT4& color)
{
	Instance& instance = instances.emplace_back();
	instance.mesh = &sphereMesh;
	instance.color = color;

	DirectX::XMMATRIX S = DirectX::XMMatrixScaling(radius, radius, radius);
	DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(position.x, position.y, position.z);
	DirectX::XMStoreFloat4x4(&instance.worldTransform, S * T);
}

// カプセル描画
void ShapeRenderSystem::DrawCapsule(
	const DirectX::XMFLOAT4X4& transform,
	float radius,
	float height,
	const DirectX::XMFLOAT4& color)
{
	DirectX::XMMATRIX Transform = DirectX::XMLoadFloat4x4(&transform);

	// 上半球：回転・平行移動を含めたワールド行列を作る
	{
		Instance& instance = instances.emplace_back();
		instance.mesh = &halfSphereMesh;
		instance.color = color;

		// 半球はスケール * Transform（回転含む）で変換し、その上で上方向へオフセット
		DirectX::XMMATRIX S = DirectX::XMMatrixScaling(radius, radius, radius);

		// 上半球の中心をローカル空間で (0, +height/2, 0) に移動してから Transform を適用
		DirectX::XMVECTOR localPos = DirectX::XMVectorSet(0.0f, height * 0.5f, 0.0f, 1.0f);
		DirectX::XMVECTOR worldPos = DirectX::XMVector3Transform(localPos, Transform);

		DirectX::XMMATRIX World = S * Transform;
		// 上書きで正しいワールド位置セット（Transform の回転・スケールは保持）
		World.r[3] = DirectX::XMVectorSetW(worldPos, 1.0f);

		DirectX::XMStoreFloat4x4(&instance.worldTransform, World);
	}

	// 円柱：Transform の向きに沿って高さを伸ばす
	{
		Instance& instance = instances.emplace_back();
		instance.mesh = &cylinderMesh;
		instance.color = color;

		// 円柱メッシュは Y 軸方向に 1.0 の長さを想定して作っている（CreateCylinderMesh の start/height に依存）
		// ここでは Transform の X/Y/Z 軸ベクトルを radius/height スケールして World 行列を作る
		DirectX::XMMATRIX World;
		World.r[0] = DirectX::XMVectorScale(DirectX::XMVector3Normalize(Transform.r[0]), radius);
		World.r[1] = DirectX::XMVectorScale(DirectX::XMVector3Normalize(Transform.r[1]), height);
		World.r[2] = DirectX::XMVectorScale(DirectX::XMVector3Normalize(Transform.r[2]), radius);
		World.r[3] = Transform.r[3]; // 位置は Transform の原点（円柱は原点中心から伸びる仕様）
		DirectX::XMStoreFloat4x4(&instance.worldTransform, World);
	}

	// 下半球
	{
		Instance& instance = instances.emplace_back();
		instance.mesh = &halfSphereMesh;
		instance.color = color;

		// 下半球は上半球と同じ方法だが回転 180deg（X 軸）を追加して使う
		DirectX::XMMATRIX S = DirectX::XMMatrixScaling(radius, radius, radius);
		DirectX::XMMATRIX R = DirectX::XMMatrixRotationX(DirectX::XM_PI);
		DirectX::XMVECTOR localPos = DirectX::XMVectorSet(0.0f, -height * 0.5f, 0.0f, 1.0f);
		// 先に回転を合成、次に Transform を適用
		DirectX::XMMATRIX Combined = R * Transform;
		DirectX::XMVECTOR worldPos = DirectX::XMVector3Transform(localPos, Transform);

		DirectX::XMMATRIX World = S * Combined;
		World.r[3] = DirectX::XMVectorSetW(worldPos, 1.0f);
		DirectX::XMStoreFloat4x4(&instance.worldTransform, World);
	}
}

void ShapeRenderSystem::DrawCylinder(
	const DirectX::XMFLOAT4X4& transform,
	float radius,
	float height,
	const DirectX::XMFLOAT4& color)
{
	Instance& instance = instances.emplace_back();

	DirectX::XMMATRIX Transform = DirectX::XMLoadFloat4x4(&transform);

	instance.mesh = &cylinderMesh;
	instance.color = color;

	// 円柱メッシュは Y 軸方向に 1.0 の長さを想定して作っている（CreateCylinderMesh の start/height に依存）
	// ここでは Transform の X/Y/Z 軸ベクトルを radius/height スケールして World 行列を作る
	DirectX::XMMATRIX World;
	World.r[0] = DirectX::XMVectorScale(DirectX::XMVector3Normalize(Transform.r[0]), radius);
	World.r[1] = DirectX::XMVectorScale(DirectX::XMVector3Normalize(Transform.r[1]), height);
	World.r[2] = DirectX::XMVectorScale(DirectX::XMVector3Normalize(Transform.r[2]), radius);
	World.r[3] = Transform.r[3]; // 位置は Transform の原点（円柱は原点中心から伸びる仕様）
	DirectX::XMStoreFloat4x4(&instance.worldTransform, World);
}


// 骨描画
void ShapeRenderSystem::DrawBone(
	const DirectX::XMFLOAT4X4& transform,
	float length,
	const DirectX::XMFLOAT4& color)
{
	Instance& instance = instances.emplace_back();
	instance.mesh = &boneMesh;
	instance.color = color;

	DirectX::XMMATRIX W = DirectX::XMLoadFloat4x4(&transform);
	W.r[0] = DirectX::XMVectorScale(DirectX::XMVector3Normalize(W.r[0]), length);
	W.r[1] = DirectX::XMVectorScale(DirectX::XMVector3Normalize(W.r[1]), length);
	W.r[2] = DirectX::XMVectorScale(DirectX::XMVector3Normalize(W.r[2]), length);
	DirectX::XMStoreFloat4x4(&instance.worldTransform, W);
}

// メッシュ生成
void ShapeRenderSystem::CreateMesh(ID3D11Device* device, const std::vector<DirectX::XMFLOAT3>& vertices, Mesh& mesh)
{
	D3D11_BUFFER_DESC desc = {};
	desc.ByteWidth = static_cast<UINT>(sizeof(DirectX::XMFLOAT3) * vertices.size());
	desc.Usage = D3D11_USAGE_IMMUTABLE;
	desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;
	desc.StructureByteStride = 0;
	D3D11_SUBRESOURCE_DATA subresourceData = {};
	subresourceData.pSysMem = vertices.data();
	subresourceData.SysMemPitch = 0;
	subresourceData.SysMemSlicePitch = 0;

	HRESULT hr = device->CreateBuffer(&desc, &subresourceData, mesh.vertexBuffer.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

	mesh.vertexCount = static_cast<UINT>(vertices.size());
}

// 箱メッシュ作成
void ShapeRenderSystem::CreateBoxMesh(ID3D11Device* device, float width, float height, float depth)
{
	DirectX::XMFLOAT3 positions[8] =
	{
		// top
		{ -width,  height, -depth},
		{  width,  height, -depth},
		{  width,  height,  depth},
		{ -width,  height,  depth},
		// bottom
		{ -width, -height, -depth},
		{  width, -height, -depth},
		{  width, -height,  depth},
		{ -width, -height,  depth},
	};

	std::vector<DirectX::XMFLOAT3> vertices;
	vertices.reserve(32); // ← reserve に変更（resize はダメ）

	// top
	vertices.emplace_back(positions[0]);
	vertices.emplace_back(positions[1]);
	vertices.emplace_back(positions[1]);
	vertices.emplace_back(positions[2]);
	vertices.emplace_back(positions[2]);
	vertices.emplace_back(positions[3]);
	vertices.emplace_back(positions[3]);
	vertices.emplace_back(positions[0]);
	// bottom
	vertices.emplace_back(positions[4]);
	vertices.emplace_back(positions[5]);
	vertices.emplace_back(positions[5]);
	vertices.emplace_back(positions[6]);
	vertices.emplace_back(positions[6]);
	vertices.emplace_back(positions[7]);
	vertices.emplace_back(positions[7]);
	vertices.emplace_back(positions[4]);
	// side
	vertices.emplace_back(positions[0]);
	vertices.emplace_back(positions[4]);
	vertices.emplace_back(positions[1]);
	vertices.emplace_back(positions[5]);
	vertices.emplace_back(positions[2]);
	vertices.emplace_back(positions[6]);
	vertices.emplace_back(positions[3]);
	vertices.emplace_back(positions[7]);


	// メッシュ生成
	CreateMesh(device, vertices, boxMesh);
}

// 球メッシュ作成
void ShapeRenderSystem::CreateSphereMesh(ID3D11Device* device, float radius, int subdivisions)
{
	float step = DirectX::XM_2PI / subdivisions;

	std::vector<DirectX::XMFLOAT3> vertices;

	// XZ平面
	for (int i = 0; i < subdivisions; ++i)
	{
		for (int j = 0; j < 2; ++j)
		{
			float theta = step * ((i + j) % subdivisions);

			DirectX::XMFLOAT3& p = vertices.emplace_back();
			p.x = sinf(theta) * radius;
			p.y = 0.0f;
			p.z = cosf(theta) * radius;
		}
	}
	// XY平面
	for (int i = 0; i < subdivisions; ++i)
	{
		for (int j = 0; j < 2; ++j)
		{
			float theta = step * ((i + j) % subdivisions);

			DirectX::XMFLOAT3& p = vertices.emplace_back();
			p.x = sinf(theta) * radius;
			p.y = cosf(theta) * radius;
			p.z = 0.0f;
		}
	}
	// YZ平面
	for (int i = 0; i < subdivisions; ++i)
	{
		for (int j = 0; j < 2; ++j)
		{
			float theta = step * ((i + j) % subdivisions);

			DirectX::XMFLOAT3& p = vertices.emplace_back();
			p.x = 0.0f;
			p.y = sinf(theta) * radius;
			p.z = cosf(theta) * radius;
		}
	}

	// メッシュ生成
	CreateMesh(device, vertices, sphereMesh);
}

// 半球メッシュ作成
void ShapeRenderSystem::CreateHalfSphereMesh(ID3D11Device* device, float radius, int subdivisions)
{
	std::vector<DirectX::XMFLOAT3> vertices;

	float theta_step = DirectX::XM_2PI / subdivisions;

	// XZ平面
	for (int i = 0; i < subdivisions; ++i)
	{
		for (int j = 0; j < 2; ++j)
		{
			float theta = theta_step * ((i + j) % subdivisions);

			DirectX::XMFLOAT3& v = vertices.emplace_back();

			v.x = sinf(theta) * radius;
			v.y = 0.0f;
			v.z = cosf(theta) * radius;
		}
	}
	// XY平面
	for (int i = 0; i < subdivisions / 2; ++i)
	{
		for (int j = 0; j < 2; ++j)
		{
			float theta = theta_step * ((i + j) % subdivisions) - DirectX::XM_PIDIV2;

			DirectX::XMFLOAT3& v = vertices.emplace_back();

			v.x = sinf(theta) * radius;
			v.y = cosf(theta) * radius;
			v.z = 0.0f;
		}
	}
	// YZ平面
	for (int i = 0; i < subdivisions / 2; ++i)
	{
		for (int j = 0; j < 2; ++j)
		{
			float theta = theta_step * ((i + j) % subdivisions);

			DirectX::XMFLOAT3& v = vertices.emplace_back();

			v.x = 0.0f;
			v.y = sinf(theta) * radius;
			v.z = cosf(theta) * radius;
		}
	}

	// メッシュ生成
	CreateMesh(device, vertices, halfSphereMesh);
}

// 円柱
void ShapeRenderSystem::CreateCylinderMesh(ID3D11Device* device, float radius1, float radius2, float start, float height, int subdivisions)
{
	std::vector<DirectX::XMFLOAT3> vertices;
	vertices.reserve(static_cast<size_t>(subdivisions) * 4 + 8);

	const float thetaStep_ = DirectX::XM_2PI / static_cast<float>(subdivisions);
	const float end_ = start + height;

	for (int index_ = 0; index_ < subdivisions; ++index_)
	{
		const float currentTheta_ = thetaStep_ * static_cast<float>(index_);
		const float nextTheta_ = thetaStep_ * static_cast<float>((index_ + 1) % subdivisions);

		const DirectX::XMFLOAT3 bottomCurrent_ = {
			sinf(currentTheta_) * radius1,
			start,
			cosf(currentTheta_) * radius1
		};
		const DirectX::XMFLOAT3 bottomNext_ = {
			sinf(nextTheta_) * radius1,
			start,
			cosf(nextTheta_) * radius1
		};
		const DirectX::XMFLOAT3 topCurrent_ = {
			sinf(currentTheta_) * radius2,
			end_,
			cosf(currentTheta_) * radius2
		};
		const DirectX::XMFLOAT3 topNext_ = {
			sinf(nextTheta_) * radius2,
			end_,
			cosf(nextTheta_) * radius2
		};

		vertices.emplace_back(bottomCurrent_);
		vertices.emplace_back(bottomNext_);
		vertices.emplace_back(topCurrent_);
		vertices.emplace_back(topNext_);
	}

	for (int guideIndex_ = 0; guideIndex_ < 4; ++guideIndex_)
	{
		const float theta_ = DirectX::XM_PIDIV2 * static_cast<float>(guideIndex_);
		vertices.emplace_back(DirectX::XMFLOAT3(
			sinf(theta_) * radius1,
			start,
			cosf(theta_) * radius1));
		vertices.emplace_back(DirectX::XMFLOAT3(
			sinf(theta_) * radius2,
			end_,
			cosf(theta_) * radius2));
	}

	// メッシュ生成
	CreateMesh(device, vertices, cylinderMesh);
}

// 骨メッシュ作成
void ShapeRenderSystem::CreateBoneMesh(ID3D11Device* device, float length)
{
	float width = length * 0.25f;
	DirectX::XMFLOAT3 positions[8] =
	{
		{ -0.00f,  0.00f,  0.00f},
		{  width,  0.00f,  width},
		{  0.00f,  0.00f,  length},
		{ -width,  0.00f,  width},
		{  0.00f,  width,  width},
		{  0.00f, -width,  width},
	};

	std::vector<DirectX::XMFLOAT3> vertices;
	vertices.reserve(24);

	// xz
	vertices.emplace_back(positions[0]);
	vertices.emplace_back(positions[1]);
	vertices.emplace_back(positions[1]);
	vertices.emplace_back(positions[2]);
	vertices.emplace_back(positions[2]);
	vertices.emplace_back(positions[3]);
	vertices.emplace_back(positions[3]);
	vertices.emplace_back(positions[0]);
	// yz
	vertices.emplace_back(positions[0]);
	vertices.emplace_back(positions[4]);
	vertices.emplace_back(positions[4]);
	vertices.emplace_back(positions[2]);
	vertices.emplace_back(positions[2]);
	vertices.emplace_back(positions[5]);
	vertices.emplace_back(positions[5]);
	vertices.emplace_back(positions[0]);
	// xy
	vertices.emplace_back(positions[1]);
	vertices.emplace_back(positions[4]);
	vertices.emplace_back(positions[4]);
	vertices.emplace_back(positions[3]);
	vertices.emplace_back(positions[3]);
	vertices.emplace_back(positions[5]);
	vertices.emplace_back(positions[5]);
	vertices.emplace_back(positions[1]);

	// メッシュ生成
	CreateMesh(device, vertices, boneMesh);
}

// 描画実行
void ShapeRenderSystem::Render(
	ID3D11DeviceContext* dc,
	const DirectX::XMFLOAT4X4& view,
	const DirectX::XMFLOAT4X4& projection)
{
	// シェーダー設定
	dc->VSSetShader(vertexShader.Get(), nullptr, 0);
	dc->PSSetShader(pixelShader.Get(), nullptr, 0);
	dc->IASetInputLayout(inputLayout.Get());

	// 定数バッファ設定
	dc->VSSetConstantBuffers(0, 1, constantBuffer.GetAddressOf());

	// ビュープロジェクション行列作成
	DirectX::XMMATRIX V = DirectX::XMLoadFloat4x4(&view);
	DirectX::XMMATRIX P = DirectX::XMLoadFloat4x4(&projection);
	DirectX::XMMATRIX VP = V * P;

	// プリミティブ設定
	UINT stride = sizeof(DirectX::XMFLOAT3);
	UINT offset = 0;
	dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

	for (const Instance& instance : instances)
	{
		// 頂点バッファ設定
		dc->IASetVertexBuffers(0, 1, instance.mesh->vertexBuffer.GetAddressOf(), &stride, &offset);

		// ワールドビュープロジェクション行列作成
		DirectX::XMMATRIX W = DirectX::XMLoadFloat4x4(&instance.worldTransform);
		DirectX::XMMATRIX WVP = W * VP;

		// 定数バッファ更新
		CbMesh cbMesh;
		DirectX::XMStoreFloat4x4(&cbMesh.worldViewProjection, WVP);
		cbMesh.color = instance.color;

		dc->UpdateSubresource(constantBuffer.Get(), 0, 0, &cbMesh, 0, 0);

		// 描画
		dc->Draw(instance.mesh->vertexCount, 0);
	}
	instances.clear();
}

void ShapeRenderSystem::CreateUnitBoxMesh(ID3D11Device* device)
{
	// 一辺1.0 (中心から±0.5) の頂点座標
	float h = 0.5f;
	DirectX::XMFLOAT3 p[8] = {
		{-h,  h, -h}, { h,  h, -h}, { h,  h,  h}, {-h,  h,  h}, // 上面 (0-3)
		{-h, -h, -h}, { h, -h, -h}, { h, -h,  h}, {-h, -h,  h}  // 下面 (4-7)
	};

	// 6面 × 2枚の三角形 × 3頂点 = 36頂点 (TRIANGLELIST)
	std::vector<DirectX::XMFLOAT3> v = {
		p[0],p[1],p[2], p[0],p[2],p[3], // 上
		p[7],p[6],p[5], p[7],p[5],p[4], // 下
		p[0],p[4],p[5], p[0],p[5],p[1], // 前
		p[2],p[6],p[7], p[2],p[7],p[3], // 後
		p[3],p[7],p[4], p[3],p[4],p[0], // 左
		p[1],p[5],p[6], p[1],p[6],p[2]  // 右
	};

	// 既存の CreateMesh 関数を使って unitBoxMesh に生成
	CreateMesh(device, v, unitBoxMesh);
}
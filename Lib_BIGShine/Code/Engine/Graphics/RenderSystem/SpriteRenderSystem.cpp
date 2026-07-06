#include "SpriteRenderSystem.h"
#include "Engine/Graphics/Core/CPUResourceLoader.h"

void SpriteRenderSystem::Finalize()
{
    vertexBuffer.Reset();
    indexBuffer.Reset();
    constantBuffer.Reset();
    samplerState.Reset();
    for (auto& s : shaders) {
        s.second.vertexShader.Reset();
        s.second.pixelShader.Reset();
        s.second.inputLayout.Reset();
    }
}

void SpriteRenderSystem::Initialize(ID3D11Device* device_, ID3D11DeviceContext* context_)
{
    // --- 頂点レイアウト定義（SpriteVertex 構造に合わせる） ---
    D3D11_INPUT_ELEMENT_DESC inputElementDesc[] =
    {
        // SemanticName, SemanticIndex, Format, Slot, AlignedByteOffset, InputSlotClass, InstanceDataStepRate
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    UINT numElements = _countof(inputElementDesc); // または ARRAYSIZE(inputElementDesc)

    // --- シェーダ作成（CreateVSFromCSO / CreatePSFromCSO をラッパーとして使用） ---
    // ここでは shaders マップに SpriteShader を格納する想定
    CreateShader(device_, SpriteShaderID::Default, "Shader/sprite_VS.cso", "Shader/sprite_PS.cso", inputElementDesc, numElements);
    //CreateShader(device_, SpriteShaderID::Dissolve, "dissolve_VS.cso", "dissolve_PS.cso", inputElementDesc, numElements);

    // --- 定数バッファやサンプラ作成（例） ---
    D3D11_BUFFER_DESC cbd{};
    cbd.Usage = D3D11_USAGE_DEFAULT;
    cbd.ByteWidth = sizeof(ConstantBuffer); // 定義しておくこと
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    device_->CreateBuffer(&cbd, nullptr, constantBuffer.GetAddressOf());

    D3D11_SAMPLER_DESC sd{};
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    device_->CreateSamplerState(&sd, samplerState.GetAddressOf());

    // --- 頂点バッファ作成 ---
    SpriteVertex vertices[4] =
    {
        { {-0.5f, -0.5f, 0.f}, {1,1,1,1}, {0,1} },
        { {-0.5f,  0.5f, 0.f}, {1,1,1,1}, {0,0} },
        { { 0.5f,  0.5f, 0.f}, {1,1,1,1}, {1,0} },
        { { 0.5f, -0.5f, 0.f}, {1,1,1,1}, {1,1} }
    };

    D3D11_BUFFER_DESC bd{};
    bd.Usage = D3D11_USAGE_DYNAMIC;             // ← 動的
    bd.ByteWidth = sizeof(vertices);
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE; // ← 書き込み可能
    bd.MiscFlags = 0;
    bd.StructureByteStride = sizeof(SpriteVertex);

    D3D11_SUBRESOURCE_DATA initData{};
    initData.pSysMem = vertices;

    device_->CreateBuffer(&bd, &initData, vertexBuffer.GetAddressOf());

    // --- インデックスバッファ作成 ---
    unsigned short indices[6] = { 0,1,2, 0,2,3 };
    bd.ByteWidth = sizeof(indices);
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    initData.pSysMem = indices;

    device_->CreateBuffer(&bd, &initData, indexBuffer.GetAddressOf());
}

void SpriteRenderSystem::RenderAll(ID3D11DeviceContext* context_)
{
    // ビューポート取得
    D3D11_VIEWPORT viewport_{};
    UINT num_viewports_{ 1 };
    context_->RSGetViewports(&num_viewports_, &viewport_);
    float screenWidth_ = viewport_.Width;
    float screenHeight_ = viewport_.Height;

    // 頂点バッファ設定
    UINT stride_ = sizeof(SpriteVertex);
    UINT offset_ = 0;
    context_->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride_, &offset_);
    context_->IASetIndexBuffer(indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // インデックス設定
    context_->IASetIndexBuffer(indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // --- 各スプライト描画ループ ---
    for (auto& info_ : drawList)
    {
        auto sprite_ = info_.sprite;
        if (!sprite_ || !sprite_->IsVisible())
            continue;

        auto t_ = sprite_->GetTransform();
        if (!t_)
            continue;

        DirectX::XMFLOAT3 tPos_ = t_->GetWorldPosition();
        DirectX::XMFLOAT2 dSize_ = sprite_->GetSize(); // 描画サイズ

        // --- ピボットによるオフセット計算 ---
        // pivotOffset は「Transform位置から見た左上頂点の相対位置」
        DirectX::XMFLOAT2 pivotOffset = { 0.0f, 0.0f };

        switch (sprite_->GetPivot())
        {
        case PosOfsset::TOP_L:   // 左上基準：画像は右下に広がる
            pivotOffset = { 0.0f, 0.0f };
            break;
        case PosOfsset::MID:     // 中央基準：画像は上下左右均等に広がる
            pivotOffset = { -dSize_.x * 0.5f, -dSize_.y * 0.5f };
            break;
        case PosOfsset::UNDER_M: // 中央下基準：画像は上に伸び、左右均等
            pivotOffset = { -dSize_.x * 0.5f, -dSize_.y };
            break;
        }

        // ローカル座標での4頂点（ピボットを 0,0 とした位置）
        // pivotOffset を起点にサイズ分広げる
        DirectX::XMFLOAT2 positions_[4] =
        {
            { pivotOffset.x,            pivotOffset.y + dSize_.y }, // 左下
            { pivotOffset.x,            pivotOffset.y },            // 左上
            { pivotOffset.x + dSize_.x, pivotOffset.y },            // 右上
            { pivotOffset.x + dSize_.x, pivotOffset.y + dSize_.y }, // 右下
        };

        // Z回転角を取得（度→ラジアン）
        float theta_ = DirectX::XMConvertToRadians(t_->GetWorldRotation().z);
        DirectX::XMMATRIX rotMat = DirectX::XMMatrixRotationZ(theta_);

        // 4頂点を「ピボット(0,0)」を中心に回転させ、最後にワールド座標(tPos_)を足す
        for (auto& p : positions_)
        {
            DirectX::XMVECTOR pos = DirectX::XMVectorSet(p.x, p.y, 0.0f, 1.0f);
            pos = DirectX::XMVector3Transform(pos, rotMat);

            DirectX::XMFLOAT3 result;
            DirectX::XMStoreFloat3(&result, pos);

            // 回転後の座標 + Transformのワールド座標
            p.x = result.x + tPos_.x;
            p.y = result.y + tPos_.y;
        }

        // スクリーン→NDC
        for (auto& p : positions_)
        {
            p.x = 2.0f * p.x / screenWidth_ - 1.0f;
            p.y = 1.0f - 2.0f * p.y / screenHeight_;
        }

        // 切り抜き範囲（ピクセル単位）
        DirectX::XMFLOAT2 catPos_ = sprite_->GetCatPos();   // 切り抜き開始位置（左上）
        DirectX::XMFLOAT2 catSize_ = sprite_->GetCatSize();  // 切り抜く範囲サイズ

        // テクスチャサイズ
        DirectX::XMFLOAT2 texSize_ = sprite_->GetTextureSize();

        // --- テクスチャ座標を算出（01に正規化） ---
        float u0 = catPos_.x / texSize_.x;             // 左
        float v0 = catPos_.y / texSize_.y;             // 上
        float u1 = (catPos_.x + catSize_.x) / texSize_.x; // 右
        float v1 = (catPos_.y + catSize_.y) / texSize_.y; // 下

        // DirectXは通常テクスチャの原点が左上なので、
        // 上下の反転が必要な場合はv0,v1を入れ替える（使ってるローダーに応じて調整）

        // 頂点に設定するUV座標
        DirectX::XMFLOAT2 texcoords_[4] =
        {
            { u0, v1 }, // 左下
            { u0, v0 }, // 左上
            { u1, v0 }, // 右上
            { u1, v1 }, // 右下
        };

        // 頂点バッファ更新
        D3D11_MAPPED_SUBRESOURCE mapped{};
        HRESULT hr = context_->Map(vertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        if (SUCCEEDED(hr))
        {
            SpriteVertex* v = static_cast<SpriteVertex*>(mapped.pData);
            for (int i = 0; i < 4; ++i)
            {
                v[i].pos = { positions_[i].x, positions_[i].y, tPos_.z };
                v[i].color = sprite_->GetColor();
                // --- 切り抜きUV適用 ---
                v[i].texcoord = texcoords_[i];
            }
            context_->Unmap(vertexBuffer.Get(), 0);
        }

        // テクスチャとサンプラ設定
        context_->PSSetShaderResources(0, 1, sprite_->GetTexture().GetAddressOf());
        context_->PSSetSamplers(0, 1, samplerState.GetAddressOf());


        // シェーダ設定
        auto& shader_ = shaders[info_.shaderID];
        context_->IASetInputLayout(shader_.inputLayout.Get());
        context_->VSSetShader(shader_.vertexShader.Get(), nullptr, 0);
        context_->PSSetShader(shader_.pixelShader.Get(), nullptr, 0);

        // --- ★2枚の三角形（6インデックス）描画 ---
        context_->DrawIndexed(6, 0, 0);
    }

    drawList.clear();

}


void SpriteRenderSystem::CreateShader(ID3D11Device* device_,
	SpriteShaderID id_, 
	const char* vsFile_, const char* psFile_, 
	D3D11_INPUT_ELEMENT_DESC* inputElementDesc_,
	UINT numElements_)
{
    SpriteShader shader_;

    // CPUResourceLoader::createVSFromCSO のシグネチャに合わせる
    HRESULT hr = CPUResourceLoader::CreateVSFromCSO(
        device_,
        vsFile_,
        shader_.vertexShader.GetAddressOf(),
        shader_.inputLayout.GetAddressOf(),
        inputElementDesc_,
        numElements_);
    if (FAILED(hr))
    {
        // エラーハンドリング（ログ等）
        return;
    }

    hr = CPUResourceLoader::CreatePSFromCSO(
        device_,
        psFile_,
        shader_.pixelShader.GetAddressOf());
    if (FAILED(hr))
    {
        // エラーハンドリング（ログ等）
        return;
    }

    shaders[id_] = shader_;
}
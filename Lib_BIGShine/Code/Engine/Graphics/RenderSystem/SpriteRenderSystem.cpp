#include "PrecompiledHeader.h"

#include "SpriteRenderSystem.h"

#include "Engine/Debug/TracyProfiler.h"
#include "Engine/Graphics/Core/CPUResourceLoader.h"
#include "Engine/UI/UIVertexModifierComponent.h"
#include "Engine/UI/UIWidgetComponent.h"
#include "Engine/UI/WidgetBoardComponent.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <cstring>
#include "Engine/UI/UIMotionComponent.h"
#include "Engine/UI/UIVisualEffectComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"

namespace
{
    DirectX::XMFLOAT2 ToNdc(const DirectX::XMFLOAT2& position_, const DirectX::XMFLOAT2& screenSize_)
    {
        // UI側のピクセル座標を、DirectXのクリップ空間(-1〜1)へ変換する。
        // Y方向は画面上が0なので、NDCの上方向に合わせて反転する。
        return
        {
            2.0f * position_.x / (std::max)(1.0f, screenSize_.x) - 1.0f,
            1.0f - 2.0f * position_.y / (std::max)(1.0f, screenSize_.y)
        };
    }

    DirectX::XMFLOAT4 MultiplyColor(
        const DirectX::XMFLOAT4& left_,
        const DirectX::XMFLOAT4& right_)
    {
        // スプライト色と頂点ごとの補正色を掛け合わせ、フェードや角ごとの色変化を反映する。
        return
        {
            left_.x * right_.x,
            left_.y * right_.y,
            left_.z * right_.z,
            left_.w * right_.w
        };
    }

    void RotateAroundPivot(
        DirectX::XMFLOAT2& point_,
        const DirectX::XMFLOAT2& pivot_,
        float rotationDegree_)
    {
        // WidgetのRectを、ピボット位置を中心に2D回転させる。
        const float theta_ = DirectX::XMConvertToRadians(rotationDegree_);
        const float cos_ = std::cos(theta_);
        const float sin_ = std::sin(theta_);
        const float localX_ = point_.x - pivot_.x;
        const float localY_ = point_.y - pivot_.y;

        point_.x = pivot_.x + localX_ * cos_ - localY_ * sin_;
        point_.y = pivot_.y + localX_ * sin_ + localY_ * cos_;
    }
}

void SpriteRenderSystem::Finalize()
{
    BS_PROFILE_SCOPE("SpriteRenderSystem::Finalize");

    // デバイス再作成や終了時に、描画キューとD3Dリソースをまとめて解放する。
    drawList.clear();
    nextSubmitOrder = 0;
    vertexBuffer.Reset();
    indexBuffer.Reset();
    constantBuffer.Reset();
    uiEffectShader.Reset();
    uiEffectBuffer.Reset();
    samplerState.Reset();
    linearSamplerState.Reset();
    for (auto& shader_ : shaders)
    {
        shader_.second.vertexShader.Reset();
        shader_.second.pixelShader.Reset();
        shader_.second.inputLayout.Reset();
    }
    shaders.clear();
}

void SpriteRenderSystem::Initialize(ID3D11Device* device_, ID3D11DeviceContext* context_)
{
    BS_PROFILE_SCOPE("SpriteRenderSystem::Initialize");
    (void)context_;

    // 再初期化でも古いリソースを残さないよう、先にFinalizeで状態を空にする。
    Finalize();

    D3D11_INPUT_ELEMENT_DESC inputElementDesc[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    const UINT numElements_ = _countof(inputElementDesc);

    // スプライト描画は基本シェーダを1つ登録し、必要に応じてshaderIDで差し替えられる形にしている。
    CreateShader(device_, SpriteShaderID::Default, "Data/Shader/sprite_VS.cso", "Data/Shader/sprite_PS.cso", inputElementDesc, numElements_);

    D3D11_BUFFER_DESC cbd_{};
    cbd_.Usage = D3D11_USAGE_DEFAULT;
    cbd_.ByteWidth = sizeof(ConstantBuffer);
    cbd_.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    device_->CreateBuffer(&cbd_, nullptr, constantBuffer.GetAddressOf());

    // 新規CSOが未生成でも、従来のSprite描画へ安全に戻す。
    std::ifstream effectFile_("Data/Shader/UIVisualEffectPS.cso",std::ios::binary);
    const std::vector<char> effectBytes_{std::istreambuf_iterator<char>(effectFile_),std::istreambuf_iterator<char>()};
    if (!effectBytes_.empty())
    {
        device_->CreatePixelShader(effectBytes_.data(),effectBytes_.size(),nullptr,uiEffectShader.GetAddressOf());
    }
    static_assert(sizeof(UIEffectConstants) % 16 == 0);
    cbd_.ByteWidth = sizeof(UIEffectConstants);
    device_->CreateBuffer(&cbd_,nullptr,uiEffectBuffer.GetAddressOf());
    D3D11_SAMPLER_DESC samplerDesc_{};
    samplerDesc_.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    samplerDesc_.AddressU = samplerDesc_.AddressV = samplerDesc_.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    device_->CreateSamplerState(&samplerDesc_, samplerState.GetAddressOf());
    // 線形補間を指定したUIだけ別Samplerを使い、既存SpriteのPoint描画を維持する。
    samplerDesc_.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    device_->CreateSamplerState(&samplerDesc_, linearSamplerState.GetAddressOf());

    // 実際の座標やUVは描画ごとにMapで書き換えるため、初期値は単位四角形として用意する。
    SpriteVertex vertices_[4] =
    {
        { {-0.5f, -0.5f, 0.0f}, {1, 1, 1, 1}, {0, 1} },
        { {-0.5f,  0.5f, 0.0f}, {1, 1, 1, 1}, {0, 0} },
        { { 0.5f,  0.5f, 0.0f}, {1, 1, 1, 1}, {1, 0} },
        { { 0.5f, -0.5f, 0.0f}, {1, 1, 1, 1}, {1, 1} }
    };

    D3D11_BUFFER_DESC bufferDesc_{};
    bufferDesc_.Usage = D3D11_USAGE_DYNAMIC;
    bufferDesc_.ByteWidth = sizeof(vertices_);
    bufferDesc_.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bufferDesc_.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    bufferDesc_.StructureByteStride = sizeof(SpriteVertex);

    D3D11_SUBRESOURCE_DATA initData_{};
    initData_.pSysMem = vertices_;
    device_->CreateBuffer(&bufferDesc_, &initData_, vertexBuffer.GetAddressOf());

    unsigned short indices_[6] = { 0, 1, 2, 0, 2, 3 };
    bufferDesc_.ByteWidth = sizeof(indices_);
    bufferDesc_.BindFlags = D3D11_BIND_INDEX_BUFFER;
    bufferDesc_.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    bufferDesc_.StructureByteStride = 0;
    initData_.pSysMem = indices_;
    device_->CreateBuffer(&bufferDesc_, &initData_, indexBuffer.GetAddressOf());
}

void SpriteRenderSystem::Submit(const SpriteRenderCommand& command_)
{
    // 同じ描画優先度のスプライトは、Submitされた順番を最後の並び順として保持する。
    SpriteRenderCommand copiedCommand_ = command_;
    copiedCommand_.submitOrder = nextSubmitOrder++;
    drawList.push_back(copiedCommand_);
}

void SpriteRenderSystem::RenderAll(ID3D11DeviceContext* context_)
{
    BS_PROFILE_SCOPE("SpriteRenderSystem::RenderAll");
    if (!context_ || drawList.empty())
    {
        // UI描画が行えないフレームでも、Submit順と描画リストは次フレームへ持ち越さない。
        drawList.clear();
        nextSubmitOrder = 0;
        return;
    }

    // 現在のViewportサイズを基準に、各スプライトをNDCへ変換して描画する。
    D3D11_VIEWPORT viewport_{};
    UINT numViewports_{ 1 };
    context_->RSGetViewports(&numViewports_, &viewport_);
    const DirectX::XMFLOAT2 screenSize_ = { viewport_.Width, viewport_.Height };

    UINT stride_ = sizeof(SpriteVertex);
    UINT offset_ = 0;
    context_->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride_, &offset_);
    context_->IASetIndexBuffer(indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    SortDrawList();

    // 1枚ずつ頂点バッファを更新しながら描画する。UIは枚数より柔軟な変形を優先している。
    for (SpriteRenderCommand& renderCommand_ : drawList)
    {
        RenderSprite(renderCommand_, context_, screenSize_);
    }

    // 後続の描画へSRV/Samplerの束縛が漏れないよう、Sprite描画で使ったスロットを外す。
    ID3D11ShaderResourceView* nullShaderResourceViews_[2] = {};
    ID3D11SamplerState* nullSamplerStates_[3] = {};
    context_->PSSetShaderResources(0, _countof(nullShaderResourceViews_), nullShaderResourceViews_);
    context_->PSSetSamplers(0, _countof(nullSamplerStates_), nullSamplerStates_);

    ID3D11Buffer* emptyBuffer_ = nullptr;
    context_->PSSetConstantBuffers(1,1,&emptyBuffer_);
    drawList.clear();
    nextSubmitOrder = 0;
}

void SpriteRenderSystem::SortDrawList()
{
    BS_PROFILE_SCOPE("SpriteRenderSystem::SortDrawList");

    // Board、Widget、階層、Submit順の順で安定ソートし、UIの前後関係を再現する。
    std::stable_sort(
        drawList.begin(),
        drawList.end(),
        [](const SpriteRenderCommand& left_, const SpriteRenderCommand& right_)
        {
            if (left_.boardLayer != right_.boardLayer)
        {
            return left_.boardLayer < right_.boardLayer;
        }
        if (left_.boardZOrder != right_.boardZOrder)
        {
            return left_.boardZOrder < right_.boardZOrder;
        }
        if (left_.widgetZOrder != right_.widgetZOrder)
        {
            return left_.widgetZOrder < right_.widgetZOrder;
        }
        if (left_.hierarchyOrder != right_.hierarchyOrder)
        {
            return left_.hierarchyOrder < right_.hierarchyOrder;
        }
        return left_.submitOrder < right_.submitOrder;
        });
}

void SpriteRenderSystem::BuildSpriteVertices(
    SpriteRenderCommand& renderCommand_,
    const DirectX::XMFLOAT2& screenSize_,
    std::array<SpriteVertex, 4>& vertices_)
{
    BS_PROFILE_SCOPE("SpriteRenderSystem::BuildSpriteVertices");

    // まず従来SpriteのTransformから四隅を作り、Widget指定があればRectLayoutで上書きする。
    BuildLegacySpriteVertices(renderCommand_, screenSize_, vertices_);

    if (renderCommand_.widget)
    {
        ApplyWidgetLayout(renderCommand_, screenSize_, vertices_);
    }

    ApplyVertexModification(renderCommand_, screenSize_, vertices_);
}

void SpriteRenderSystem::BuildLegacySpriteVertices(
    const SpriteRenderCommand& renderCommand_,
    const DirectX::XMFLOAT2& screenSize_,
    std::array<SpriteVertex, 4>& vertices_)
{
    BS_PROFILE_SCOPE("SpriteRenderSystem::BuildLegacySpriteVertices");

    auto sprite_ = renderCommand_.sprite;
    if (!sprite_)
    {
        return;
    }

    auto transform_ = sprite_->GetTransform();
    if (!transform_)
    {
        return;
    }

    const DirectX::XMFLOAT3 transformPosition_ = transform_->GetWorldPosition();
    const DirectX::XMFLOAT2 drawSize_ = sprite_->GetSize();

    // 旧SpriteはPivotの種類ごとに、Transform位置から見た左上/中央/下中央の基準を切り替える。
    DirectX::XMFLOAT2 pivotOffset_ = { 0.0f, 0.0f };
    switch (sprite_->GetPivot())
    {
    case PosOfsset::TOP_L:
        pivotOffset_ = { 0.0f, 0.0f };
        break;
    case PosOfsset::MID:
        pivotOffset_ = { -drawSize_.x * 0.5f, -drawSize_.y * 0.5f };
        break;
    case PosOfsset::UNDER_M:
        pivotOffset_ = { -drawSize_.x * 0.5f, -drawSize_.y };
        break;
    }

    std::array<DirectX::XMFLOAT2, 4> positions_ =
    {
        DirectX::XMFLOAT2{ pivotOffset_.x,               pivotOffset_.y + drawSize_.y },
        DirectX::XMFLOAT2{ pivotOffset_.x,               pivotOffset_.y },
        DirectX::XMFLOAT2{ pivotOffset_.x + drawSize_.x, pivotOffset_.y },
        DirectX::XMFLOAT2{ pivotOffset_.x + drawSize_.x, pivotOffset_.y + drawSize_.y }
    };

    const float theta_ = DirectX::XMConvertToRadians(transform_->GetWorldRotation().z);
    const DirectX::XMMATRIX rotationMatrix_ = DirectX::XMMatrixRotationZ(theta_);

    // ローカル矩形をZ回転してからWorld座標へ移し、最終的にNDCへ変換する。
    for (DirectX::XMFLOAT2& position_ : positions_)
    {
        DirectX::XMVECTOR positionVector_ = DirectX::XMVectorSet(position_.x, position_.y, 0.0f, 1.0f);
        positionVector_ = DirectX::XMVector3Transform(positionVector_, rotationMatrix_);

        DirectX::XMFLOAT3 result_;
        DirectX::XMStoreFloat3(&result_, positionVector_);
        position_.x = result_.x + transformPosition_.x;
        position_.y = result_.y + transformPosition_.y;
    }

    const DirectX::XMFLOAT2 catPos_ = sprite_->GetCatPos();
    const DirectX::XMFLOAT2 catSize_ = sprite_->GetCatSize();
    DirectX::XMFLOAT2 textureSize_ = sprite_->GetTextureSize();
    textureSize_.x = (std::max)(1.0f, textureSize_.x);
    textureSize_.y = (std::max)(1.0f, textureSize_.y);

    // catPos/catSizeで指定された切り出し範囲を、テクスチャ全体に対するUVへ変換する。
    const float u0_ = catPos_.x / textureSize_.x;
    const float v0_ = catPos_.y / textureSize_.y;
    const float u1_ = (catPos_.x + catSize_.x) / textureSize_.x;
    const float v1_ = (catPos_.y + catSize_.y) / textureSize_.y;

    const std::array<DirectX::XMFLOAT2, 4> texcoords_ =
    {
        DirectX::XMFLOAT2{ u0_, v1_ },
        DirectX::XMFLOAT2{ u0_, v0_ },
        DirectX::XMFLOAT2{ u1_, v0_ },
        DirectX::XMFLOAT2{ u1_, v1_ }
    };

    const DirectX::XMFLOAT4 color_ = sprite_->GetColor();
    for (size_t index_ = 0; index_ < vertices_.size(); ++index_)
    {
        const DirectX::XMFLOAT2 ndc_ = ToNdc(positions_[index_], screenSize_);
        vertices_[index_].pos = { ndc_.x, ndc_.y, transformPosition_.z };
        vertices_[index_].color = color_;
        vertices_[index_].texcoord = texcoords_[index_];
    }
}

void SpriteRenderSystem::ApplyWidgetLayout(
    SpriteRenderCommand& renderCommand_,
    const DirectX::XMFLOAT2& screenSize_,
    std::array<SpriteVertex, 4>& vertices_)
{
    BS_PROFILE_SCOPE("SpriteRenderSystem::ApplyWidgetLayout");

    if (!renderCommand_.widget || !renderCommand_.sprite)
    {
        return;
    }

    // 親WidgetのLayoutが必要なため、対象Widgetまでの親階層を先に再計算する。
    CalculateWidgetLayoutRecursive(renderCommand_.widget, renderCommand_.widgetBoard, screenSize_, 0);

    const UIScreenRect& rect_ = renderCommand_.widget->GetScreenRect();
    const UIRectTransform& rectTransform_ = renderCommand_.widget->GetRectTransform();
    const DirectX::XMFLOAT2 rectSize_ = rect_.GetSize();
    const DirectX::XMFLOAT2 pivotPosition_ =
    {
        rect_.min.x + rectSize_.x * rectTransform_.pivot.x,
        rect_.min.y + rectSize_.y * rectTransform_.pivot.y
    };

    std::array<DirectX::XMFLOAT2, 4> positions_ =
    {
        DirectX::XMFLOAT2{ rect_.min.x, rect_.max.y },
        DirectX::XMFLOAT2{ rect_.min.x, rect_.min.y },
        DirectX::XMFLOAT2{ rect_.max.x, rect_.min.y },
        DirectX::XMFLOAT2{ rect_.max.x, rect_.max.y }
    };

    // WidgetのRectTransform回転を、ピボット位置を中心に四隅へ反映する。
    for (DirectX::XMFLOAT2& position_ : positions_)
    {
        RotateAroundPivot(position_, pivotPosition_, renderCommand_.widget->GetVisualRotation());
    }

    auto transform_ = renderCommand_.sprite->GetTransform();
    const float z_ = transform_ ? transform_->GetWorldPosition().z : 0.0f;

    for (size_t index_ = 0; index_ < vertices_.size(); ++index_)
    {
        const DirectX::XMFLOAT2 ndc_ = ToNdc(positions_[index_], screenSize_);
        vertices_[index_].pos.x = ndc_.x;
        vertices_[index_].pos.y = ndc_.y;
        vertices_[index_].pos.z = z_;
    }
}

void SpriteRenderSystem::CalculateWidgetLayoutRecursive(
    UIWidgetComponent* widget_,
    WidgetBoardComponent* board_,
    const DirectX::XMFLOAT2& screenSize_,
    int depth_)
{
    BS_PROFILE_SCOPE("SpriteRenderSystem::CalculateWidgetLayoutRecursive");

    if (!widget_ || depth_ > 32)
    {
        return;
    }

    // 保存された親IDだけがある場合は、描画直前に実体への参照を解決する。
    if (!widget_->GetParentWidgetId().empty() && !widget_->GetParentWidget())
    {
        widget_->ResolveParentWidget();
    }

    UIWidgetComponent* parentWidget_ = widget_->GetParentWidget();
    if (parentWidget_)
    {
        // 子のLayoutは親のRectに依存するため、親から順に計算する。
        CalculateWidgetLayoutRecursive(parentWidget_, board_, screenSize_, depth_ + 1);
    }

    widget_->CalculateLayout(board_, parentWidget_, screenSize_);
}

void SpriteRenderSystem::ApplyVertexModification(
    const SpriteRenderCommand& renderCommand_,
    const DirectX::XMFLOAT2& screenSize_,
    std::array<SpriteVertex, 4>& vertices_)
{
    BS_PROFILE_SCOPE("SpriteRenderSystem::ApplyVertexModification");

    if (!renderCommand_.vertexModifier)
    {
        return;
    }

    // 左下、左上、右上、右下の順をUIVertexModifier側の頂点指定へ対応させる。
    const std::array<UI_VERTEX_INDEX, 4> vertexIndexMap_ =
    {
        UI_VERTEX_INDEX::LEFT_BOTTOM,
        UI_VERTEX_INDEX::LEFT_TOP,
        UI_VERTEX_INDEX::RIGHT_TOP,
        UI_VERTEX_INDEX::RIGHT_BOTTOM
    };

    for (size_t index_ = 0; index_ < vertices_.size(); ++index_)
    {
        // 位置補正はピクセル単位で指定されるため、NDC上の移動量へ変換して加算する。
        const UIVertexModification& modification_ =
            renderCommand_.vertexModifier->GetVertexModification(vertexIndexMap_[index_]);
        vertices_[index_].pos.x += 2.0f * modification_.positionOffset.x / (std::max)(1.0f, screenSize_.x);
        vertices_[index_].pos.y -= 2.0f * modification_.positionOffset.y / (std::max)(1.0f, screenSize_.y);
        vertices_[index_].texcoord.x += modification_.uvOffset.x;
        vertices_[index_].texcoord.y += modification_.uvOffset.y;
        vertices_[index_].color = MultiplyColor(vertices_[index_].color, modification_.colorMultiplier);
    }
}

void SpriteRenderSystem::RenderSprite(
    SpriteRenderCommand& renderCommand_,
    ID3D11DeviceContext* context_,
    const DirectX::XMFLOAT2& screenSize_)
{
    BS_PROFILE_SCOPE("SpriteRenderSystem::RenderSprite");

    auto sprite_ = renderCommand_.sprite;
    if (!sprite_ || !sprite_->IsVisible())
    {
        return;
    }
    // BoardやWidgetが非表示/無効の場合は、その配下のSpriteだけ描画を止める。
    if (renderCommand_.widgetBoard &&
        (!renderCommand_.widgetBoard->IsVisible() || !renderCommand_.widgetBoard->IsEnabled()))
    {
        return;
    }
    if (renderCommand_.widget &&
        (!renderCommand_.widget->IsVisible() || !renderCommand_.widget->IsEnabled()))
    {
        return;
    }

    std::array<SpriteVertex, 4> vertices_{};
    BuildSpriteVertices(renderCommand_, screenSize_, vertices_);

    const auto owner_ = sprite_->GetGameObject();
    const auto motion_ = owner_ ? owner_->GetComponent<UIMotionComponent>() : nullptr;
    if (motion_ && motion_->IsEnabled() && motion_->IsStarted())
    {
        const auto& sample_ = motion_->GetSample();
        DirectX::XMFLOAT2 center_{0,0};
        for (const auto& vertex_ : vertices_)
        {
            center_.x += vertex_.pos.x * 0.25f;
            center_.y += vertex_.pos.y * 0.25f;
        }
        for (auto& vertex_ : vertices_)
        {
            // Widgetの移動・拡縮はLayout側で適用済み。旧式Spriteだけ頂点を補正する。
            if (!renderCommand_.widget)
            {
                DirectX::XMFLOAT2 point_{(vertex_.pos.x-center_.x)*screenSize_.x*0.5f*sample_.scale.x,
                    -(vertex_.pos.y-center_.y)*screenSize_.y*0.5f*sample_.scale.y};
                RotateAroundPivot(point_,{0,0},sample_.rotation);
                vertex_.pos.x = center_.x + 2*(point_.x+sample_.offset.x)/(std::max)(1.0f,screenSize_.x);
                vertex_.pos.y = center_.y - 2*(point_.y+sample_.offset.y)/(std::max)(1.0f,screenSize_.y);
            }
            vertex_.color.w *= sample_.opacity;
        }
    }
    auto shaderIterator_ = shaders.find(renderCommand_.shaderID);
    if (shaderIterator_ == shaders.end())
    {
        shaderIterator_ = shaders.find(SpriteShaderID::Default);
    }
    if (shaderIterator_ == shaders.end())
    {
        return;
    }
    auto& shader_ = shaderIterator_->second;
    context_->IASetInputLayout(shader_.inputLayout.Get());
    context_->VSSetShader(shader_.vertexShader.Get(),nullptr,0);
    auto* sampler_ = sprite_->UsesLinearSampling() && linearSamplerState ? linearSamplerState.Get() : samplerState.Get();
    ID3D11SamplerState* samplers_[] = {sampler_,sampler_,sampler_};
    context_->PSSetSamplers(0,3,samplers_);
    const auto draw_ = [&](const std::array<SpriteVertex,4>& quad_,ID3D11ShaderResourceView* texture_,ID3D11PixelShader* pixelShader_)
    {
        D3D11_MAPPED_SUBRESOURCE mapped_{};
        if (FAILED(context_->Map(vertexBuffer.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped_)))
        {
            return;
        }
        std::memcpy(mapped_.pData,quad_.data(),sizeof(SpriteVertex)*quad_.size());
        context_->Unmap(vertexBuffer.Get(),0);
        context_->PSSetShaderResources(0,1,&texture_);
        context_->PSSetShader(pixelShader_,nullptr,0);
        context_->DrawIndexed(6,0,0);
    };
    const auto effect_ = owner_ ? owner_->GetComponent<UIVisualEffectComponent>() : nullptr;
    const bool useEffect_ = effect_ && effect_->IsEnabled() && effect_->IsStarted() && renderCommand_.shaderID == SpriteShaderID::Default;
    const auto baseVertices_ = vertices_;
    if (useEffect_)
    {
        effect_->PrepareTrailView(screenSize_);
        const auto& settings_ = effect_->GetSettings();
        // 履歴は最大6枚。古いFontテクスチャもSRVを保持して寿命を保証する。
        for (const auto& trail_ : effect_->GetTrails())
        {
            std::array<SpriteVertex,4> ghost_{};
            float centerX_ = 0, centerY_ = 0;
            for (const auto& point_ : trail_.positions)
            {
                centerX_ += point_.x*0.25f;
                centerY_ += point_.y*0.25f;
            }
            const float age_ = std::clamp((effect_->GetTrailTime()-trail_.time)/settings_.trailLifetime,0.0f,1.0f);
            for (std::size_t index_ = 0; index_ < ghost_.size(); ++index_)
            {
                ghost_[index_] = {trail_.positions[index_],MultiplyColor(trail_.colors[index_],settings_.trailColor),trail_.texcoords[index_]};
                ghost_[index_].pos.x = centerX_+(ghost_[index_].pos.x-centerX_)*std::lerp(1.0f,settings_.trailScale,age_);
                ghost_[index_].pos.y = centerY_+(ghost_[index_].pos.y-centerY_)*std::lerp(1.0f,settings_.trailScale,age_);
                ghost_[index_].color.w *= (1-age_)*(1-age_);
            }
            draw_(ghost_,trail_.texture.Get(),shader_.pixelShader.Get());
        }
        if (uiEffectShader && uiEffectBuffer)
        {
            const float strength_ = effect_->GetStrength();
            UIEffectConstants cb_{};
            cb_.glowColor = settings_.glowColor;
            cb_.glowColor.w *= settings_.glowIntensity*strength_;
            cb_.outlineColor = settings_.outlineColor;
            cb_.outlineColor.w *= strength_;
            cb_.shadowColor = settings_.shadowColor;
            cb_.shadowColor.w *= settings_.shadowOpacity*strength_;
            cb_.sweepColor = settings_.sweepColor;
            cb_.sweepColor.w *= settings_.sweepIntensity*strength_;
            cb_.gradientTop = settings_.gradientTop;
            cb_.gradientBottom = settings_.gradientBottom;
            cb_.tone = {settings_.grayscale*strength_,settings_.sepia*strength_,settings_.invert*strength_,std::lerp(1.0f,settings_.pixelSize,strength_)};
            const float dissolve_ = (std::max)(settings_.dissolve * strength_, effect_->GetRuntimeDissolve());
            cb_.style = {settings_.glowRadius,settings_.outlineWidth,settings_.blurRadius*strength_,dissolve_};
            cb_.animation = {effect_->GetTime(),settings_.pulseAmount*strength_,settings_.pulseSpeed,settings_.sweepSpeed};
            cb_.sweep = {DirectX::XMConvertToRadians(settings_.sweepAngle),settings_.sweepWidth,settings_.edgeWidth,settings_.noiseScale};
            const auto length_ = [&](std::size_t first_,std::size_t second_)
            {
                return (std::max)(1.0f,std::hypot((vertices_[first_].pos.x-vertices_[second_].pos.x)*screenSize_.x*0.5f,(vertices_[first_].pos.y-vertices_[second_].pos.y)*screenSize_.y*0.5f));
            };
            const float width_ = length_(1,2), height_ = length_(1,0);
            const float u0_ = vertices_[1].texcoord.x, v0_ = vertices_[1].texcoord.y;
            const float u1_ = vertices_[3].texcoord.x, v1_ = vertices_[3].texcoord.y;
            cb_.sampling = {(u1_-u0_)/width_,(v1_-v0_)/height_,settings_.shadowSoftness,settings_.gradientStrength*strength_};
            cb_.shadow = {settings_.shadowOffset.x,settings_.shadowOffset.y,0,0};
            cb_.maskUV = {settings_.maskScale.x,settings_.maskScale.y,settings_.maskOffset.x,settings_.maskOffset.y};
            cb_.maskOptions = {effect_->GetMaskTexture() ? 1.0f : 0.0f,settings_.maskInvert ? 1.0f : 0.0f,0,0};
            auto* mask_ = effect_->GetMaskTexture();
            context_->PSSetShaderResources(1,1,&mask_);
            cb_.uvRect = {u0_,v0_,u1_,v1_};
            cb_.detail = {settings_.uvScroll.x,settings_.uvScroll.y,0,0};
            cb_.burnColor = settings_.burnColor;
            // 光と影の余白を確保し、字形や画像の矩形境界で切れないようにする。
            const float padding_ = (std::max)({settings_.glowIntensity > 0 ? settings_.glowRadius : 0,settings_.outlineWidth,
                settings_.shadowOpacity > 0 ? (std::max)(std::abs(settings_.shadowOffset.x),std::abs(settings_.shadowOffset.y))+settings_.shadowSoftness : 0,settings_.blurRadius})+1;
            const float px_ = padding_/width_, py_ = padding_/height_;
            const DirectX::XMFLOAT2 local_[] = {{-px_,1+py_},{-px_,-py_},{1+px_,-py_},{1+px_,1+py_}};
            for (std::size_t index_ = 0; index_ < vertices_.size(); ++index_)
            {
                const float x_ = local_[index_].x, y_ = local_[index_].y;
                vertices_[index_].pos.x = baseVertices_[1].pos.x+(baseVertices_[2].pos.x-baseVertices_[1].pos.x)*x_+(baseVertices_[0].pos.x-baseVertices_[1].pos.x)*y_+(baseVertices_[3].pos.x-baseVertices_[2].pos.x-baseVertices_[0].pos.x+baseVertices_[1].pos.x)*x_*y_;
                vertices_[index_].pos.y = baseVertices_[1].pos.y+(baseVertices_[2].pos.y-baseVertices_[1].pos.y)*x_+(baseVertices_[0].pos.y-baseVertices_[1].pos.y)*y_+(baseVertices_[3].pos.y-baseVertices_[2].pos.y-baseVertices_[0].pos.y+baseVertices_[1].pos.y)*x_*y_;
                vertices_[index_].texcoord = {std::lerp(u0_,u1_,x_),std::lerp(v0_,v1_,y_)};
            }
            context_->UpdateSubresource(uiEffectBuffer.Get(),0,nullptr,&cb_,0,0);
            context_->PSSetConstantBuffers(1,1,uiEffectBuffer.GetAddressOf());
            draw_(vertices_,sprite_->GetTexture().Get(),uiEffectShader.Get());
        }
        else
        {
            draw_(vertices_,sprite_->GetTexture().Get(),shader_.pixelShader.Get());
        }
        UIEffectTrailSample sample_;
        for (std::size_t index_ = 0; index_ < vertices_.size(); ++index_)
        {
            sample_.positions[index_] = baseVertices_[index_].pos;
            sample_.texcoords[index_] = baseVertices_[index_].texcoord;
            sample_.colors[index_] = baseVertices_[index_].color;
        }
        sample_.texture = sprite_->GetTexture();
        effect_->CaptureTrail(std::move(sample_),screenSize_);
    }
    else
    {
        draw_(vertices_,sprite_->GetTexture().Get(),shader_.pixelShader.Get());
    }
}

void SpriteRenderSystem::CreateShader(
    ID3D11Device* device_,
    SpriteShaderID id_,
    const char* vsFile_,
    const char* psFile_,
    D3D11_INPUT_ELEMENT_DESC* inputElementDesc_,
    UINT numElements_)
{
    BS_PROFILE_SCOPE("SpriteRenderSystem::CreateShader");

    // CSOからVS/PSとInputLayoutを作り、SpriteShaderIDで後から選べるように保持する。
    SpriteShader shader_;
    HRESULT hr_ = CPUResourceLoader::CreateVSFromCSO(
        device_,
        vsFile_,
        shader_.vertexShader.GetAddressOf(),
        shader_.inputLayout.GetAddressOf(),
        inputElementDesc_,
        numElements_);
    if (FAILED(hr_))
    {
        return;
    }

    hr_ = CPUResourceLoader::CreatePSFromCSO(
        device_,
        psFile_,
        shader_.pixelShader.GetAddressOf());
    if (FAILED(hr_))
    {
        return;
    }

    shaders[id_] = shader_;
}

#pragma once

#include <vector>
#include <memory>
#include <algorithm>
#include <d3d11.h>
#include <DirectXMath.h>
#include "Engine/Graphics/Core/LightManager.h"
#include "Engine/GamePlay/Camera/CameraComponent.h"
#include "RenderState.h"
#include "Engine/Graphics/Decal/DecalManager.h"
#include "Engine/Graphics/RenderSystem/CascadeShadowMap.h"

class ShapeRenderSystem;

// ===========================
// RenderContext
// ===========================
struct RenderContext
{
    ID3D11Device* device = nullptr; // 【追加】リソース生成の魔力
    ID3D11DeviceContext* deviceContext;

    const RenderState* renderState;
    const CameraComponent* camera = nullptr;       // シーンカメラ
    const LightManager* lights = nullptr; // シーンライト

    const DecalManager* decal = nullptr; //シーンデカール

    ID3D11RenderTargetView* backBufferRTV = nullptr;
    ID3D11ShaderResourceView* depthSRV = nullptr;    // 現在の深度バッファのSRV
    ID3D11ShaderResourceView* baseColorSRV = nullptr;
    ShapeRenderSystem* shapeSystem = nullptr;        // Boxメッシュの供給源

    CascadeShadowMap* cascadeShadowMap = nullptr;

    // 必要であれば、ポストプロセス等で重宝する画面サイズ
    float screenWidth = 0.0f;
    float screenHeight = 0.0f;
};

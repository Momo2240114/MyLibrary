#pragma once

#include <Vector>
#include <Memory>
#include "RenderContext.h"
#include "IRenderPass.h"

//// RenderPipeline.h
//class RenderPipeline {
//private:
//    std::vector<std::unique_ptr<IRenderPass>> m_passes;
//    // 中間テクスチャ（パス間で受け渡すためのバッファ）
//    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_intermediateRTV;
//    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_intermediateSRV;
//
//public:
//    void Initialize(ID3D11Device* device, int width, int height);
//
//    void AddPass(std::unique_ptr<IRenderPass> pass) { m_passes.push_back(std::move(pass)); }
//
//    void Execute(const RenderContext& rc);
//};
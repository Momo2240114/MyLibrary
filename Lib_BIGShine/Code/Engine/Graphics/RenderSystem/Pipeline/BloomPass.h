//// BloomPass.h
//#pragma once
//
//#include "IRenderPass.h"
//#include "Engine/Graphics/RenderSystem/BloomSystem.h"
//
//class BloomPass : public IRenderPass
//{
//private:
//    BloomSystem m_bloomSystem;
//    BloomSystem::Parameters m_params;
//    BloomSystem::CbColorFilter m_filterParams;
//
//public:
//    void Initialize(ID3D11Device* device, float width, float height) override {
//        m_bloomSystem.Initialize(device, width, height);
//    }
//
//    void Execute(const RenderContext& rc, ID3D11ShaderResourceView* inputSRV, ID3D11RenderTargetView* outputRTV) override {
//        // ここでBloomSystemの機能を呼び出す
//        m_bloomSystem.Execute(rc, inputSRV, outputRTV, m_params, m_filterParams);
//    }
//
//    // パラメータを外部からいじるための口
//    BloomSystem::Parameters& GetParams() { return m_params; }
//};
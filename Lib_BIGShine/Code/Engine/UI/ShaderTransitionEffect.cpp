#include "PrecompiledHeader.h"
#include "ShaderTransitionEffect.h"
#include "UIPresentationPresets.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Graphics/Core/CPUResourceLoader.h"
#include "Engine/Graphics/Core/RenderState.h"
#include "Engine/Debug/TracyProfiler.h"
#include "Engine/Common/DebugLog.h"
#include <imgui.h>
#include <array>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <cstring>
namespace
{
    // 16バイト境界を保ち、TransitionCommon.hlsliと同じ順序で共有する。
    struct TransitionConstants
    {
        DirectX::XMFLOAT4 color, edgeColor, timing, geometry, noise, style, maskUV, maskParameters, panels, maskAnimation, maskFrames;
    };
    Microsoft::WRL::ComPtr<ID3D11VertexShader> sharedVertexShader;
    Microsoft::WRL::ComPtr<ID3D11Buffer> sharedConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> sharedNoise;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> sharedWhite;

    // 欠損・破損したCSOはassertへ進まず、FADEの代替描画へ戻す。
    std::vector<char> ReadShader(const std::string& path_)
    {
        std::ifstream file_(path_,std::ios::binary | std::ios::ate);
        if (!file_ || file_.tellg() <= 0) return {};
        std::vector<char> bytes_(static_cast<size_t>(file_.tellg()));
        file_.seekg(0);
        if (!file_.read(bytes_.data(),bytes_.size())) return {};
        return bytes_;
    }
    // 既存Loaderのキャッシュを利用し、共有Textureの所有権はSRV参照で維持する。
    bool LoadTexture(const nlohmann::json& settings_,const char* key_,Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& output_)
    {
        return UIPresentationPresets::LoadTexture(UIPresentationPresets::String(settings_,key_,""),output_);
    }
    float Number(const nlohmann::json& settings_,const char* key_,float fallback_,float minimum_,float maximum_)
    {
        if (!settings_.contains(key_) || !settings_[key_].is_number()) return fallback_;
        const float value_ = settings_[key_].get<float>();
        return std::isfinite(value_) ? std::clamp(value_,minimum_,maximum_) : fallback_;
    }
}
ShaderTransitionEffect::ShaderTransitionEffect(const char* shaderPath_) : shaderPath(shaderPath_) {}
void ShaderTransitionEffect::SetParameters(const TransitionParameters& parameters_) { parameters = parameters_; }
bool ShaderTransitionEffect::Initialize()
{
    BS_PROFILE_SCOPE("ShaderTransitionEffect::Initialize");
    auto device_ = GraphicsDeviceManager::Instance().GetDevice();
    if (!device_) return false;
    if (!sharedVertexShader)
    {
        const auto bytes_ = ReadShader("Data/Shader/FullScreenVS.cso");
        if (bytes_.empty() || FAILED(device_->CreateVertexShader(bytes_.data(),bytes_.size(),nullptr,sharedVertexShader.GetAddressOf()))) return false;
    }
    if (!sharedConstantBuffer)
    {
        D3D11_BUFFER_DESC desc_{};
        desc_.ByteWidth = sizeof(TransitionConstants); desc_.Usage = D3D11_USAGE_DEFAULT; desc_.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        if (FAILED(device_->CreateBuffer(&desc_,nullptr,sharedConstantBuffer.GetAddressOf()))) return false;
    }
    if (!sharedNoise)
    {
        std::array<std::uint8_t,128 * 128> pixels_{};
        std::uint32_t seed_ = 0x42534849;
        for (auto& pixel_ : pixels_) { seed_ = seed_ * 1664525u + 1013904223u; pixel_ = static_cast<std::uint8_t>(seed_ >> 24); }
        D3D11_TEXTURE2D_DESC desc_{};
        desc_.Width = desc_.Height = 128; desc_.MipLevels = desc_.ArraySize = 1;
        desc_.Format = DXGI_FORMAT_R8_UNORM; desc_.SampleDesc.Count = 1;
        desc_.Usage = D3D11_USAGE_IMMUTABLE; desc_.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA initial_{}; initial_.pSysMem = pixels_.data(); initial_.SysMemPitch = 128;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture_;
        if (FAILED(device_->CreateTexture2D(&desc_,&initial_,texture_.GetAddressOf())) ||
            FAILED(device_->CreateShaderResourceView(texture_.Get(),nullptr,sharedNoise.GetAddressOf()))) return false;
    }
    if (!sharedWhite && FAILED(CPUResourceLoader::CreateDummyTexture(device_,0xffffffff,sharedWhite.GetAddressOf()))) return false;
    const auto bytes_ = ReadShader(shaderPath);
    if (bytes_.empty() || FAILED(device_->CreatePixelShader(bytes_.data(),bytes_.size(),nullptr,pixelShader.ReleaseAndGetAddressOf()))) return false;
    if (parameters.type == TransitionType::GLASS)
    {
        const auto vertexBytes_ = ReadShader("Data/Shader/GlassTransitionVS.cso");
        if (vertexBytes_.empty() || FAILED(device_->CreateVertexShader(vertexBytes_.data(),vertexBytes_.size(),nullptr,glassVertexShader.GetAddressOf()))) return false;
    }
    LoadResources(); ready = true; return true;
}
void ShaderTransitionEffect::LoadResources()
{
    BS_PROFILE_SCOPE("ShaderTransitionEffect::LoadResources");
    if (!LoadTexture(parameters.effectSettings,"NoiseTexture",noiseTexture)) noiseTexture = sharedNoise;
    hasTexture = LoadTexture(parameters.effectSettings,"Texture",effectTexture);
    if (!hasTexture) effectTexture = sharedWhite;
    // UVを画面全体へ引き伸ばさず、読み込んだ画像の縦横比を形状用PSへ渡す。
    shapeTextureAspect = 1.0f;
    const bool shapeMask_ = parameters.type == TransitionType::DIAMOND || parameters.type == TransitionType::CHECKER;
    if (shapeMask_ && hasTexture)
    {
        Microsoft::WRL::ComPtr<ID3D11Resource> resource_;
        effectTexture->GetResource(resource_.GetAddressOf());
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture_;
        if (resource_ && SUCCEEDED(resource_.As(&texture_)))
        {
            D3D11_TEXTURE2D_DESC description_{};
            texture_->GetDesc(&description_);
            shapeTextureAspect = static_cast<float>(description_.Width) / (std::max)(1u,description_.Height);
        }
    }
    else if (shapeMask_ && !UIPresentationPresets::String(parameters.effectSettings,"Texture","").empty())
    {
        BS_LOG(LOG_CATEGORY::RENDER,LOG_LEVEL::WARNING,
            "Shape transition mask unavailable; using procedural shape: " +
            UIPresentationPresets::String(parameters.effectSettings,"Texture",""));
    }
    hasGlow = LoadTexture(parameters.effectSettings,"GlowTexture",glowTexture);
    if (!hasGlow) glowTexture = sharedWhite;
}
void ShaderTransitionEffect::ShutdownSharedResources()
{
    UIPresentationPresets::ClearTexturePreviews();
    sharedWhite.Reset(); sharedNoise.Reset(); sharedConstantBuffer.Reset(); sharedVertexShader.Reset();
}
bool ShaderTransitionEffect::Render(ID3D11RenderTargetView* renderTarget_)
{
    BS_PROFILE_SCOPE("ShaderTransitionEffect::Render");
    if (!ready) return false;
    auto& graphics_ = GraphicsDeviceManager::Instance();
    auto context_ = graphics_.GetContext();
    auto states_ = graphics_.GetRenderState();
    if (!context_ || !states_ || !graphics_.GetRenderTargetView()) return false;
    BS_PROFILE_GPU_SCOPE(graphics_.GetTracyGpuContext(),"GPU::ScreenTransition");
    graphics_.SetBackBufferRenderTarget();
    auto* target_ = renderTarget_ ? renderTarget_ : graphics_.GetRenderTargetView();
    if (renderTarget_) context_->OMSetRenderTargets(1, &target_, nullptr);
    // ガラス演出だけ現在の画面を保存する。開く段階では新Sceneを取り込み直す。
    if (parameters.type == TransitionType::GLASS)
    {
        Microsoft::WRL::ComPtr<ID3D11Resource> source_;
        target_->GetResource(source_.GetAddressOf());
        Microsoft::WRL::ComPtr<ID3D11Texture2D> sourceTexture_;
        source_.As(&sourceTexture_);
        if (sourceTexture_)
        {
            D3D11_TEXTURE2D_DESC description_{}; sourceTexture_->GetDesc(&description_);
            D3D11_TEXTURE2D_DESC old_{}; if (snapshotTexture) snapshotTexture->GetDesc(&old_);
            const bool resized_ = old_.Width != description_.Width || old_.Height != description_.Height || old_.Format != description_.Format;
            if (resized_) { snapshotView.Reset(); snapshotTexture.Reset(); }
            if (!snapshotView || resized_ || snapshotRevealing != revealing)
            {
                if (!snapshotTexture && description_.SampleDesc.Count == 1)
                {
                    description_.BindFlags = D3D11_BIND_SHADER_RESOURCE; description_.Usage = D3D11_USAGE_DEFAULT;
                    description_.CPUAccessFlags = 0; description_.MiscFlags = 0;
                    if (SUCCEEDED(graphics_.GetDevice()->CreateTexture2D(&description_,nullptr,snapshotTexture.GetAddressOf())))
                        graphics_.GetDevice()->CreateShaderResourceView(snapshotTexture.Get(),nullptr,snapshotView.GetAddressOf());
                }
                if (snapshotView) context_->CopyResource(snapshotTexture.Get(),sourceTexture_.Get());
                snapshotRevealing = revealing;
            }
        }
    }
    TransitionConstants cb_{};
    cb_.color = parameters.color; cb_.edgeColor = parameters.edgeColor;
    cb_.timing = {progress,elapsedTime,0,static_cast<float>(parameters.easing)};
    cb_.geometry = {parameters.centerPosition.x,parameters.centerPosition.y,parameters.direction.x,parameters.direction.y};
    cb_.noise = {parameters.noiseScale,parameters.noiseSpeed,parameters.glowIntensity,parameters.edgeWidth};
    cb_.style = {static_cast<float>((std::max)(1,graphics_.GetRenderWidth())) / (std::max)(1,graphics_.GetRenderHeight()),parameters.softness,parameters.boundaryCurve,hasGlow ? 1.0f : 0.0f};
    if (parameters.type == TransitionType::GLASS) cb_.style.z = Number(parameters.effectSettings,"ShardRotation",.8f,0,2);
    cb_.panels = {static_cast<float>(parameters.panelCount),parameters.panelDelay,revealing ? parameters.durationIn : parameters.durationOut,hasTexture ? 1.0f : 0.0f};
    const auto& settings_ = parameters.effectSettings;
    cb_.maskUV = {Number(settings_,"ScaleX",1,0.01f,100),Number(settings_,"ScaleY",1,0.01f,100),Number(settings_,"OffsetX",0,-100,100),Number(settings_,"OffsetY",0,-100,100)};
    cb_.maskParameters = {UIPresentationPresets::Boolean(settings_,"Invert",false) ? 1.0f : 0.0f,DirectX::XMConvertToRadians(Number(settings_,"Rotation",0,-360,360)),Number(settings_,"SurfaceStrength",0.15f,0,1),Number(settings_,"Channel",0,0,4)};
    cb_.maskAnimation = {Number(settings_,"MaskMode",0,0,3),Number(settings_,"ScrollX",0,-5,5),Number(settings_,"ScrollY",0,-5,5),snapshotView ? 1.0f : 0.0f};
    cb_.maskFrames = {Number(settings_,"Columns",1,1,16),Number(settings_,"Rows",1,1,16),Number(settings_,"FrameCount",1,1,256),0};
    if (parameters.type == TransitionType::DIAMOND || parameters.type == TransitionType::CHECKER)
    {
        // 既存の定数バッファ配置を維持し、形状PSだけで使う成分へ専用設定を詰める。
        cb_.maskAnimation.x = Number(settings_,"ShapeThreshold",0.5f,0.01f,0.99f);
        cb_.maskFrames = {Number(settings_,"CompletionFill",0.08f,0.01f,0.25f),0,
            Number(settings_,"ShapeEndScale",parameters.type == TransitionType::CHECKER ? 3.0f : 1.0f,1.0f,8.0f),shapeTextureAspect};
    }
    context_->UpdateSubresource(sharedConstantBuffer.Get(),0,nullptr,&cb_,0,0);
    context_->RSSetState(states_->GetRasterizerState(RasterizerState::SolidCullNone));
    context_->OMSetDepthStencilState(states_->GetDepthStencilState(DepthState::NoTestNoWrite),0);
    context_->OMSetBlendState(states_->GetBlendState(renderTarget_ ? BlendState::TransparencyKeepAlpha : BlendState::Transparency),nullptr,0xffffffff);
    context_->IASetInputLayout(nullptr); context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context_->VSSetShader(glassVertexShader ? glassVertexShader.Get() : sharedVertexShader.Get(),nullptr,0); context_->PSSetShader(pixelShader.Get(),nullptr,0);
    context_->GSSetShader(nullptr,nullptr,0); context_->HSSetShader(nullptr,nullptr,0); context_->DSSetShader(nullptr,nullptr,0);
    context_->PSSetConstantBuffers(0,1,sharedConstantBuffer.GetAddressOf());
    ID3D11ShaderResourceView* resources_[] = {noiseTexture.Get(),effectTexture.Get(),glowTexture.Get(),snapshotView.Get()};
    context_->PSSetShaderResources(0,4,resources_);
    auto sampler_ = states_->GetSamplerState(SamplerState::LinearWrap);
    context_->PSSetSamplers(0,1,&sampler_);
    // 形状マスクは画像外を繰り返さない。Noise等が使う既存Wrap samplerとは分ける。
    auto shapeSampler_ = states_->GetSamplerState(SamplerState::LinearClamp);
    context_->PSSetSamplers(1,1,&shapeSampler_);
    if (glassVertexShader)
    {
        // 背景の三角形と破片を同じDrawへまとめ、PSで全破片を探索する処理を避ける。
        context_->VSSetConstantBuffers(0,1,sharedConstantBuffer.GetAddressOf());
        const int columns_ = std::clamp(parameters.panelCount,2,6);
        const int rows_ = std::clamp(static_cast<int>(std::round(columns_ / (std::max)(cb_.style.x,.5f))),2,4);
        context_->Draw(3+columns_*rows_*6,0);
        ID3D11Buffer* emptyBuffer_ = nullptr; context_->VSSetConstantBuffers(0,1,&emptyBuffer_);
    }
    else context_->Draw(3,0);
    ID3D11ShaderResourceView* empty_[4]{}; context_->PSSetShaderResources(0,4,empty_);
    return true;
}
void ShaderTransitionEffect::DrawEdges(TransitionParameters& parameters_)
{
    ImGui::ColorEdit4("Edge Color",&parameters_.edgeColor.x);
    ImGui::DragFloat("Edge Width",&parameters_.edgeWidth,0.001f,0.0001f,0.2f);
    ImGui::DragFloat("Softness",&parameters_.softness,0.001f,0.0001f,0.1f);
    ImGui::DragFloat("Glow Intensity",&parameters_.glowIntensity,0.05f,0,8);
}
void ShaderTransitionEffect::DrawDirection(TransitionParameters& parameters_)
{
    ImGui::SliderFloat2("Direction",&parameters_.direction.x,-1,1);
    if (ImGui::Button("Left to Right")) parameters_.direction = {1,0};
    ImGui::SameLine(); if (ImGui::Button("Right to Left")) parameters_.direction = {-1,0};
    if (ImGui::Button("Top to Bottom")) parameters_.direction = {0,1};
    ImGui::SameLine(); if (ImGui::Button("Bottom to Top")) parameters_.direction = {0,-1};
    ImGui::SameLine(); if (ImGui::Button("Diagonal")) parameters_.direction = {0.8f,0.6f};
}
void ShaderTransitionEffect::DrawTexture(TransitionParameters& parameters_,const char* key_,const char* label_)
{
    UIPresentationPresets::DrawTexture(parameters_.effectSettings,key_,label_);
}
void ShaderTransitionEffect::DrawMaskUV(TransitionParameters& parameters_)
{
    for (const auto key_ : {"ScaleX","ScaleY","OffsetX","OffsetY","Rotation","SurfaceStrength"})
    {
        const bool scale_ = key_[0] == 'S' && key_[1] == 'c';
        const bool strength_ = std::strcmp(key_,"SurfaceStrength") == 0;
        const bool rotation_ = std::strcmp(key_,"Rotation") == 0;
        const float minimum_ = scale_ ? .01f : strength_ ? 0 : rotation_ ? -360.0f : -100.0f;
        const float maximum_ = strength_ ? 1.0f : rotation_ ? 360.0f : 100.0f;
        const float fallback_ = scale_ ? 1.0f : strength_ ? .15f : 0;
        float value_ = Number(parameters_.effectSettings,key_,fallback_,minimum_,maximum_);
        if (ImGui::DragFloat(key_,&value_,.01f,minimum_,maximum_)) parameters_.effectSettings[key_] = value_;
    }
}

// 画像の白い領域を輪郭として使い、濃度の時間変化で形が崩れることを避ける。
void ShaderTransitionEffect::DrawShapeMask(TransitionParameters& parameters_)
{
    BS_PROFILE_SCOPE("ShaderTransitionEffect::DrawShapeMask");
    ImGui::SeparatorText("Shape Mask (optional)");
    DrawTexture(parameters_,"Texture","Shape Mask Texture");
    ImGui::TextWrapped("White = shape. Image aspect is preserved. Clear the texture to use the original shape.");
    constexpr const char* CHANNELS[] = {"Red","Green","Blue","Alpha","Luminance"};
    int channel_ = static_cast<int>(Number(parameters_.effectSettings,"Channel",0,0,4));
    if (ImGui::Combo("Shape Channel",&channel_,CHANNELS,5)) parameters_.effectSettings["Channel"] = channel_;
    bool invert_ = UIPresentationPresets::Boolean(parameters_.effectSettings,"Invert",false);
    if (ImGui::Checkbox("Invert Shape",&invert_)) parameters_.effectSettings["Invert"] = invert_;
    for (const auto key_ : {"ShapeThreshold","ShapeEndScale","Rotation","CompletionFill"})
    {
        const bool threshold_ = std::strcmp(key_,"ShapeThreshold") == 0;
        const bool scale_ = std::strcmp(key_,"ShapeEndScale") == 0;
        const bool rotation_ = std::strcmp(key_,"Rotation") == 0;
        const float fallback_ = threshold_ ? 0.5f : scale_ ? (parameters_.type == TransitionType::CHECKER ? 3.0f : 1.0f) : rotation_ ? 0.0f : 0.08f;
        const float minimum_ = rotation_ ? -360.0f : scale_ ? 1.0f : 0.01f;
        const float maximum_ = rotation_ ? 360.0f : scale_ ? 8.0f : threshold_ ? 0.99f : 0.25f;
        float value_ = Number(parameters_.effectSettings,key_,fallback_,minimum_,maximum_);
        if (ImGui::DragFloat(key_,&value_,rotation_ ? 1.0f : 0.01f,minimum_,maximum_)) parameters_.effectSettings[key_] = value_;
    }
    ImGui::TextWrapped("ShapeEndScale controls expansion. CompletionFill seals holes at the end so scene changes remain hidden.");
}

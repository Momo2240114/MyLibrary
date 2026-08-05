#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>

#include "Engine/Graphics/Core/RenderContext.h"

class MotionBlurSystem
{
public:
    class ObjectVelocityRenderer
    {
    public:
        virtual ~ObjectVelocityRenderer() = default;
        // Object固有の現在/前Frame行列からVelocity Bufferへ速度を書き込む。
        virtual void DrawObjectVelocity(
            const RenderContext& rc_,
            ID3D11RenderTargetView* velocityRTV_,
            int width_,
            int height_,
            const DirectX::XMFLOAT4X4& currentViewProjection_,
            const DirectX::XMFLOAT4X4& previousViewProjection_) = 0;
        // Velocity描画後に、各Objectの現在Transformを次Frame履歴へ確定する。
        virtual void CommitHistory() {}
    };

    struct Parameters
    {
        // falseならMotionBlur合成を無効化する。
        bool enabled = false;
        // Camera移動由来のVelocityをDepthから生成するかどうか。
        bool useCameraVelocity = false;
        // Blur Sample数。多いほど滑らかだが重くなる。
        int sampleCount = 6;
        // Velocityに掛けるBlur強度。
        float strength = 0.6f;
        // Blurの最大長を制限する値。
        float maxVelocity = 0.035f;
    };

    // ShaderとVelocity/Tile Bufferを初期化する。
    bool Initialize(ID3D11Device* device_, int width_, int height_);
    void Shutdown();
    // 現在Camera行列をpreviousとして同期する。
    void SynchronizeCameraHistory(const RenderContext& rc_);
    // Camera履歴を破棄し、次FrameのCamera Velocity生成を止める。
    void ResetCameraHistory();
    // Object Velocityを書き込む外部Rendererを接続する。
    void SetObjectVelocityRenderer(ObjectVelocityRenderer* objectVelocityRenderer_);
    // Velocity Bufferを0でClearする。
    void ClearVelocityBuffer(ID3D11DeviceContext* dc_);
    // 既に作られたVelocity BufferからTileMax/NeighborMax/Blur合成を実行する。
    void ExecuteBlurFromCurrentVelocity(
        const RenderContext& rc_,
        ID3D11ShaderResourceView* sceneSRV_,
        ID3D11ShaderResourceView* depthSRV_,
        ID3D11RenderTargetView* outputRTV_,
        const Parameters& parameters_);

    ID3D11RenderTargetView* GetVelocityRTV() const { return velocityBuffer.rtv.Get(); }
    ID3D11ShaderResourceView* GetVelocitySRV() const { return velocityBuffer.srv.Get(); }
    ID3D11ShaderResourceView* GetTileMaxSRV() const { return tileMaxBuffer.srv.Get(); }
    ID3D11ShaderResourceView* GetNeighborMaxSRV() const { return neighborMaxBuffer.srv.Get(); }
    int GetVelocityWidth() const { return width; }
    int GetVelocityHeight() const { return height; }

    // Velocity生成からBlur合成、履歴更新までをまとめて実行する。
    void Execute(
        const RenderContext& rc_,
        ID3D11ShaderResourceView* sceneSRV_,
        ID3D11ShaderResourceView* depthSRV_,
        ID3D11RenderTargetView* outputRTV_,
        const Parameters& parameters_);

private:
    // TileMaxでまとめるPixel Blockサイズ。
    static constexpr int TILE_SIZE = 20;

    struct CbMotionBlur
    {
        // MotionBlur各Passで共有する画面/Camera/Blur設定。
        DirectX::XMINT2 screenSize;
        DirectX::XMFLOAT2 screenPixelSize;
        DirectX::XMFLOAT4X4 invViewProjection;
        DirectX::XMFLOAT4X4 previousViewProjection;
        int enabled;
        int sampleCount;
        float strength;
        float maxVelocity;
    };

    struct RenderTarget
    {
        // PostEffect内部でRTV/SRV両方として使うTexture。
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    };

    // RTV/SRV付きの内部RenderTargetを作成する。
    bool CreateRenderTarget(
        ID3D11Device* device_,
        int width_,
        int height_,
        DXGI_FORMAT format_,
        RenderTarget& target_);
    // MotionBlur用ConstantBufferを更新する。
    void UpdateConstantBuffer(const RenderContext& rc_, const Parameters& parameters_);
    // 現在CameraのViewProjectionをpreviousとして保存する。
    void UpdatePreviousViewProjection(const RenderContext& rc_);
    // 現在CameraのViewProjectionを作成する。
    DirectX::XMFLOAT4X4 BuildViewProjection(const RenderContext& rc_) const;
    // 任意Pixel Shaderで全画面Passを1回実行する共通処理。
    void DrawPass(
        ID3D11DeviceContext* dc_,
        ID3D11RenderTargetView* outputRTV_,
        ID3D11PixelShader* pixelShader_,
        ID3D11ShaderResourceView* const* shaderResources_,
        UINT shaderResourceCount_,
        int width_,
        int height_);
    // VertexBufferなしの全画面Triangleを描画する。
    void DrawFullScreenQuad(ID3D11DeviceContext* dc_);

    // Scene解像度とTileMax用解像度。
    int width = 0;
    int height = 0;
    int tileWidth = 0;
    int tileHeight = 0;
    bool hasPreviousViewProjection = false;
    // Camera Velocity計算に使う前FrameのViewProjection。
    DirectX::XMFLOAT4X4 previousViewProjection{};
    // Object単位のVelocityを書き込むための外部Renderer。
    ObjectVelocityRenderer* objectVelocityRenderer = nullptr;

    // MotionBlur内部Pass用RenderTarget。
    RenderTarget velocityBuffer;
    RenderTarget tileMaxBuffer;
    RenderTarget neighborMaxBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> velocityPixelShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> tileMaxPixelShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> neighborMaxPixelShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> fullScreenVs;
};

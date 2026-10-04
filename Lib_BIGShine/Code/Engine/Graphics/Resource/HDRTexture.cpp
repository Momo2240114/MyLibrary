#include "PrecompiledHeader.h"

#include "HDRTexture.h"
#include "Engine/Debug/TracyProfiler.h"

namespace
{
    constexpr int HDR_CUBE_SIZE = 512;
    constexpr float PI = 3.14159265358979323846f;

    struct Float3
    {
        float x;
        float y;
        float z;
    };

    Float3 Normalize(const Float3& value_)
    {
        const float length_ = std::sqrt(
            value_.x * value_.x + value_.y * value_.y + value_.z * value_.z);
        if (length_ <= 0.000001f)
        {
            return { 0.0f, 1.0f, 0.0f };
        }

        const float invLength_ = 1.0f / length_;
        return { value_.x * invLength_, value_.y * invLength_, value_.z * invLength_ };
    }

    Float3 GetCubeDirection(int faceIndex_, int x_, int y_, int size_)
    {
        const float u_ = (static_cast<float>(x_) + 0.5f) / static_cast<float>(size_) * 2.0f - 1.0f;
        const float v_ = (static_cast<float>(y_) + 0.5f) / static_cast<float>(size_) * 2.0f - 1.0f;

        switch (faceIndex_)
        {
        case 0: return Normalize({ 1.0f, -v_, -u_ });
        case 1: return Normalize({ -1.0f, -v_, u_ });
        case 2: return Normalize({ u_, 1.0f, v_ });
        case 3: return Normalize({ u_, -1.0f, -v_ });
        case 4: return Normalize({ u_, -v_, 1.0f });
        default: return Normalize({ -u_, -v_, -1.0f });
        }
    }

    int CalculateMipCount(int size_)
    {
        int mipCount_ = 1;
        while (size_ > 1)
        {
            size_ >>= 1;
            ++mipCount_;
        }
        return mipCount_;
    }
}

bool HDRTexture::Load(
    ID3D11Device* device_,
    ID3D11DeviceContext* deviceContext_,
    const wchar_t* filename_,
    float intensity_)
{
    BS_PROFILE_SCOPE("HDRTexture::Load");

    if (!device_ || !deviceContext_ || !filename_)
    {
        return false;
    }

    FILE* file_ = nullptr;
    if (_wfopen_s(&file_, filename_, L"rb") != 0 || !file_)
    {
        return false;
    }

    if (!CheckHeader(file_))
    {
        fclose(file_);
        return false;
    }

    std::vector<float> pixels_;
    if (!ReadPixels(file_, pixels_))
    {
        fclose(file_);
        return false;
    }
    fclose(file_);

    cubeSize = HDR_CUBE_SIZE;
    std::vector<float> cubePixels_;
    const float hdrIntensity_ = std::max(0.0f, intensity_);
    BuildCubePixels(pixels_, cubePixels_, hdrIntensity_);

    const int mipCount_ = CalculateMipCount(cubeSize);
    D3D11_TEXTURE2D_DESC textureDescription_{};
    textureDescription_.Width = static_cast<UINT>(cubeSize);
    textureDescription_.Height = static_cast<UINT>(cubeSize);
    textureDescription_.MipLevels = static_cast<UINT>(mipCount_);
    textureDescription_.ArraySize = 6;
    textureDescription_.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    textureDescription_.SampleDesc.Count = 1;
    textureDescription_.Usage = D3D11_USAGE_DEFAULT;
    textureDescription_.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    textureDescription_.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS | D3D11_RESOURCE_MISC_TEXTURECUBE;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture_;
    HRESULT result_ = device_->CreateTexture2D(
        &textureDescription_, nullptr, texture_.GetAddressOf());
    if (FAILED(result_))
    {
        return false;
    }

    // 既存シェーダーがTextureCubeを参照するため、HDRを6面のキューブマップへ展開して登録する。
    for (int faceIndex_ = 0; faceIndex_ < 6; ++faceIndex_)
    {
        const UINT subresourceIndex_ = D3D11CalcSubresource(0, static_cast<UINT>(faceIndex_), mipCount_);
        const float* facePixels_ =
            cubePixels_.data() + static_cast<std::size_t>(faceIndex_) * cubeSize * cubeSize * 4;

        deviceContext_->UpdateSubresource(
            texture_.Get(),
            subresourceIndex_,
            nullptr,
            facePixels_,
            static_cast<UINT>(sizeof(float) * 4 * cubeSize),
            0);
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDescription_{};
    shaderResourceViewDescription_.Format = textureDescription_.Format;
    shaderResourceViewDescription_.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
    shaderResourceViewDescription_.TextureCube.MostDetailedMip = 0;
    shaderResourceViewDescription_.TextureCube.MipLevels = static_cast<UINT>(mipCount_);

    result_ = device_->CreateShaderResourceView(
        texture_.Get(),
        &shaderResourceViewDescription_,
        shaderResourceView.ReleaseAndGetAddressOf());
    if (FAILED(result_))
    {
        return false;
    }

    deviceContext_->GenerateMips(shaderResourceView.Get());
    return true;
}

void HDRTexture::Set(ID3D11DeviceContext* deviceContext_, int slot_) const
{
    if (!deviceContext_)
    {
        return;
    }

    ID3D11ShaderResourceView* shaderResources_[1] = { shaderResourceView.Get() };
    deviceContext_->PSSetShaderResources(slot_, 1, shaderResources_);
}

void HDRTexture::BuildCubePixels(
    const std::vector<float>& sourcePixels_,
    std::vector<float>& cubePixels_,
    float intensity_) const
{
    cubePixels_.resize(static_cast<std::size_t>(cubeSize) * cubeSize * 6 * 4);

    for (int faceIndex_ = 0; faceIndex_ < 6; ++faceIndex_)
    {
        for (int y_ = 0; y_ < cubeSize; ++y_)
        {
            for (int x_ = 0; x_ < cubeSize; ++x_)
            {
                const Float3 direction_ = GetCubeDirection(faceIndex_, x_, y_, cubeSize);
                float u_ = std::atan2(direction_.z, direction_.x) / (2.0f * PI) + 0.5f;
                const float v_ = std::acos(std::clamp(direction_.y, -1.0f, 1.0f)) / PI;
                u_ -= std::floor(u_);

                const float sourceX_ = u_ * static_cast<float>(width - 1);
                const float sourceY_ = v_ * static_cast<float>(height - 1);
                const int x0_ = static_cast<int>(std::floor(sourceX_));
                const int y0_ = static_cast<int>(std::floor(sourceY_));
                const int x1_ = (x0_ + 1) % width;
                const int y1_ = std::min(y0_ + 1, height - 1);
                const float rateX_ = sourceX_ - static_cast<float>(x0_);
                const float rateY_ = sourceY_ - static_cast<float>(y0_);

                const std::size_t destinationIndex_ =
                    (static_cast<std::size_t>(faceIndex_) * cubeSize * cubeSize +
                        static_cast<std::size_t>(y_) * cubeSize + x_) * 4;

                // 面の境界が目立ちにくいよう、全天球画像をバイリニアでサンプリングする。
                for (int channelIndex_ = 0; channelIndex_ < 4; ++channelIndex_)
                {
                    const float c00_ = sourcePixels_[(static_cast<std::size_t>(y0_) * width + x0_) * 4 + channelIndex_];
                    const float c10_ = sourcePixels_[(static_cast<std::size_t>(y0_) * width + x1_) * 4 + channelIndex_];
                    const float c01_ = sourcePixels_[(static_cast<std::size_t>(y1_) * width + x0_) * 4 + channelIndex_];
                    const float c11_ = sourcePixels_[(static_cast<std::size_t>(y1_) * width + x1_) * 4 + channelIndex_];
                    const float top_ = c00_ + (c10_ - c00_) * rateX_;
                    const float bottom_ = c01_ + (c11_ - c01_) * rateX_;
                    cubePixels_[destinationIndex_ + channelIndex_] =
                        (top_ + (bottom_ - top_) * rateY_) * intensity_;
                }

                cubePixels_[destinationIndex_ + 3] = 1.0f;
            }
        }
    }
}

bool HDRTexture::CheckHeader(FILE* file_)
{
    if (!file_)
    {
        return false;
    }

    char line_[256]{};

    // 空行までをHDRヘッダーとして読み飛ばし、画像サイズ行へ進める。
    for (;;)
    {
        if (!fgets(line_, static_cast<int>(sizeof(line_)), file_))
        {
            return false;
        }

        if (line_[0] == '\n' || line_[0] == '\r')
        {
            break;
        }
    }

    if (!fgets(line_, static_cast<int>(sizeof(line_)), file_))
    {
        return false;
    }

    char ySign_[8]{};
    char xSign_[8]{};
    const int readCount_ = sscanf_s(
        line_,
        "%7s %d %7s %d",
        ySign_, static_cast<unsigned int>(sizeof(ySign_)),
        &height,
        xSign_, static_cast<unsigned int>(sizeof(xSign_)),
        &width);

    return readCount_ == 4 && width > 0 && height > 0;
}

bool HDRTexture::ReadLine(std::vector<std::uint8_t>& scanline_, FILE* file_) const
{
    if (!file_ || width <= 0)
    {
        return false;
    }

    const int first_ = getc(file_);
    const int second_ = getc(file_);
    const int widthHigh_ = getc(file_);
    const int widthLow_ = getc(file_);
    if (first_ == EOF || second_ == EOF || widthHigh_ == EOF || widthLow_ == EOF)
    {
        return false;
    }

    // Radiance HDRの新RLE形式のみを対象にし、幅が合わないデータは失敗扱いにする。
    if (first_ != 2 || second_ != 2 || ((widthHigh_ << 8) | widthLow_) != width)
    {
        return false;
    }

    for (int channelIndex_ = 0; channelIndex_ < 4; ++channelIndex_)
    {
        for (int x_ = 0; x_ < width;)
        {
            int length_ = getc(file_);
            if (length_ == EOF)
            {
                return false;
            }

            if (length_ > 128)
            {
                length_ -= 128;
                const int value_ = getc(file_);
                if (value_ == EOF || length_ <= 0 || x_ + length_ > width)
                {
                    return false;
                }

                while (length_ > 0)
                {
                    scanline_[static_cast<std::size_t>(x_) * 4 + channelIndex_] =
                        static_cast<std::uint8_t>(value_);
                    ++x_;
                    --length_;
                }
            }
            else
            {
                if (length_ <= 0 || x_ + length_ > width)
                {
                    return false;
                }

                while (length_ > 0)
                {
                    const int value_ = getc(file_);
                    if (value_ == EOF)
                    {
                        return false;
                    }

                    scanline_[static_cast<std::size_t>(x_) * 4 + channelIndex_] =
                        static_cast<std::uint8_t>(value_);
                    ++x_;
                    --length_;
                }
            }
        }
    }

    return true;
}

bool HDRTexture::ReadPixels(FILE* file_, std::vector<float>& pixels_) const
{
    if (!file_ || width <= 0 || height <= 0)
    {
        return false;
    }

    pixels_.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4);
    std::vector<std::uint8_t> scanline_(static_cast<std::size_t>(width) * 4);

    for (int y_ = height - 1; y_ >= 0; --y_)
    {
        if (!ReadLine(scanline_, file_))
        {
            return false;
        }

        for (int x_ = 0; x_ < width; ++x_)
        {
            const std::size_t sourceIndex_ = static_cast<std::size_t>(x_) * 4;
            const std::size_t destinationIndex_ =
                (static_cast<std::size_t>(height - 1 - y_) * width + x_) * 4;

            const float exponent_ = std::ldexp(1.0f, static_cast<int>(scanline_[sourceIndex_ + 3]) - 128);
            pixels_[destinationIndex_ + 0] = static_cast<float>(scanline_[sourceIndex_ + 0]) * exponent_;
            pixels_[destinationIndex_ + 1] = static_cast<float>(scanline_[sourceIndex_ + 1]) * exponent_;
            pixels_[destinationIndex_ + 2] = static_cast<float>(scanline_[sourceIndex_ + 2]) * exponent_;
            pixels_[destinationIndex_ + 3] = 1.0f;
        }
    }

    return true;
}

// 形状用Textureは繰り返さず、タイル外へ別の輪郭が現れることを防ぐ。
SamplerState shapeSampler : register(s1);

// 点は画面高または正方形セルを基準とする等方座標。逆回転した点へ画像比率を適用し、
// 縦長・横長画像と回転した形状の両方を、画面サイズによらず相似のまま拡縮する。
float SampleShapeDistance(float2 point_, float radius_)
{
    float sine_, cosine_;
    sincos(maskParameters.y,sine_,cosine_);
    float2 rotated_ = float2(point_.x*cosine_ + point_.y*sine_,
        -point_.x*sine_ + point_.y*cosine_);
    float2 sampleUV_ = rotated_ / (max(radius_,0.00001)*float2(max(maskFrames.w,0.0001),1)) * 0.5 + 0.5;
    float4 sample_ = effectTexture.SampleLevel(shapeSampler,sampleUV_,0);
    int channel_ = (int)maskParameters.w;
    float value_ = channel_ < 4 ? sample_[channel_] : dot(sample_.rgb,float3(0.2126,0.7152,0.0722));
    if (maskParameters.x > 0.5)
    {
        value_ = 1 - value_;
    }
    // 反転指定でも画像の外側は常に形状外として扱う。
    if (any(sampleUV_ < 0) || any(sampleUV_ > 1))
    {
        value_ = 0;
    }
    return value_ - maskAnimation.x;
}

// 穴のある画像でも完全被覆してからSceneを切り替える。通常の拡縮区間は輪郭を固定する。
float4 CompleteShapeCover(float4 result_, float progress_)
{
    float completion_ = smoothstep(1-maskFrames.x,1,progress_);
    result_.rgb = lerp(result_.rgb,color.rgb,completion_);
    result_.a = lerp(result_.a,1,completion_);
    return result_;
}

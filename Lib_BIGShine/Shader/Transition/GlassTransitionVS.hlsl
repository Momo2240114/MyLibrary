#include "TransitionCommon.hlsli"
struct GlassOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float3 barycentric : TEXCOORD1;
    float background : TEXCOORD2;
};
float HashGlass(float2 value_) { return frac(sin(dot(value_,float2(127.1,311.7)))*43758.5453); }
// 隣接する破片は同じ境界頂点を共有し、初期状態で元の画面を正確に覆う。
float2 GlassVertex(float2 cell_,float2 grid_)
{
    float2 jitter_ = float2(HashGlass(cell_),HashGlass(cell_+37))-.5;
    if (cell_.x <= 0 || cell_.x >= grid_.x) jitter_.x = 0;
    if (cell_.y <= 0 || cell_.y >= grid_.y) jitter_.y = 0;
    return (cell_+jitter_*.55)/grid_;
}
GlassOutput main(uint vertexId_ : SV_VertexID)
{
    GlassOutput output_ = (GlassOutput)0;
    if (vertexId_ < 3)
    {
        // 最初の三角形で破片の隙間を黒くする。残りをその上へ合成する。
        float2 uv_ = float2((vertexId_ << 1) & 2,vertexId_ & 2);
        output_.position = float4(uv_*float2(2,-2)+float2(-1,1),0,1);
        output_.uv = uv_; output_.background = 1;
        return output_;
    }
    uint triangleId_ = (vertexId_-3)/3;
    uint corner_ = (vertexId_-3)%3;
    int columns_ = clamp((int)panels.x,2,6);
    int rows_ = clamp((int)round(columns_/max(style.x,.5)),2,4);
    float2 grid_ = float2(columns_,rows_);
    uint cellId_ = triangleId_/2;
    int half_ = triangleId_%2;
    float2 cell_ = float2(cellId_%columns_,cellId_/columns_);
    float2 a_ = GlassVertex(cell_+(half_ == 0 ? float2(0,0) : float2(1,1)),grid_);
    float2 b_ = GlassVertex(cell_+(half_ == 0 ? float2(1,0) : float2(0,1)),grid_);
    float2 c_ = GlassVertex(cell_+(half_ == 0 ? float2(0,1) : float2(1,0)),grid_);
    float2 center_ = (a_+b_+c_)/3;
    float2 uv_ = corner_ == 0 ? a_ : corner_ == 1 ? b_ : c_;
    float progress_ = saturate(Evaluate(timing.x,(int)timing.w));
    float random_ = HashGlass(cell_+float2(half_*13,0));
    float travel_ = saturate((progress_-.12*random_)/max(.001,1-.12*random_));
    float angle_ = (random_-.5)*travel_*style.z*5;
    float2 point_ = (uv_-center_)*float2(style.x,1)*(1-travel_*.35);
    float sine_,cosine_; sincos(angle_,sine_,cosine_);
    point_ = float2(point_.x*cosine_-point_.y*sine_,point_.x*sine_+point_.y*cosine_)/float2(style.x,1);
    float2 displacement_ = float2((center_.x-.5)*1.5,(.4+random_)*2.5)*travel_*travel_*max(.1,noise.y);
    float2 destination_ = center_+point_+displacement_;
    output_.position = float4(destination_*float2(2,-2)+float2(-1,1),0,1);
    output_.uv = uv_; output_.barycentric = corner_ == 0 ? float3(1,0,0) : corner_ == 1 ? float3(0,1,0) : float3(0,0,1);
    return output_;
}

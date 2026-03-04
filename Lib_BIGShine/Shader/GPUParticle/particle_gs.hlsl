#include "particle.hlsli"
// パーティクル構造体や定数バッファの共通定義を読み込み
// → vertex_shader と同じ定義を共有することで GPU 側と一致させる

//	1粒ごとに：
//	 ├─
//	StructuredBufferからパーティクル情報を取得
//	 │ position, angle, color, size, chip...
//	 ├─
//	生存チェック（ age<= 0 || age >=
//	lifespan） ならスキップ
//	 ├─
//	size補間（ spawn→
//	despawn）
//	 ├─ 4 頂点（
//	左下・ 左上・
//	右下・ 右上）
//	を生成
//	 │    ├─ 回転と拡大
//	 │    ├─
//	UV座標設定（ スプライトシート対応）
//	 │    └─
//	color設定
//	 ├─ output.Append()
//	で描画用ストリームへ送出
//	 └─ RestartStrip() 
//	で区切る（1 枚の四角形完成）



//======================================================================
// GPU上のパーティクル配列（StructuredBuffer）
//======================================================================
// Compute Shader が毎フレーム更新した "particle" 配列をここで読み取る。
// 各インデックス = 1粒。
StructuredBuffer<particle> particle_buffer : register(t9);


//======================================================================
// サンプラーとテクスチャ（今回はパーティクルの色調補正など用）
//======================================================================
#define POINT 0
#define LINEAR 1
#define ANISOTROPIC 2
SamplerState sampler_states[3] : register(s0);
Texture2D color_temper_chart : register(t0);


//======================================================================
// Geometry Shader の宣言
//======================================================================
// 最大4頂点を出力する → 四角形スプライト1枚分
// "point" トポロジー（1粒 = 1点）を入力に取り、
// "TriangleStream" で4頂点のポリゴンを出力する。
[maxvertexcount(4)]
void main(point VS_OUT input[1], inout TriangleStream<GS_OUT> output)
{
    //--------------------------------------------------
    // 四角形スプライトのローカル座標（-1～+1 範囲）
    //--------------------------------------------------
    const float2 corners[] =
    {
        float2(-1.0, -1.0), // 左下
        float2(-1.0, +1.0), // 左上
        float2(+1.0, -1.0), // 右下
        float2(+1.0, +1.0), // 右上
    };

    // 対応するUV座標
    const float2 texcoords[] =
    {
        float2(0.0, 1.0),
        float2(0.0, 0.0),
        float2(1.0, 1.0),
        float2(1.0, 0.0),
    };
    
    //--------------------------------------------------
    // StructuredBuffer から 現在の粒データを取得
    //--------------------------------------------------
    particle p = particle_buffer[input[0].vertex_id];
	
#if 1
	//----------------------------------------------
    // 寿命が0または寿命を超えた粒は描画しない
    //----------------------------------------------
	if (p.age <= 0.0 || p.age >= p.lifespan)
	{
		return;
	}
#endif
	//----------------------------------------------
    // アスペクト比（画面比率補正用）
    //----------------------------------------------
	const float aspect_ratio = 1280.0 / 720.0;

	//----------------------------------------------
    // エイジによるサイズ補間（spawn→despawn）
    //----------------------------------------------
	float age_garadient = saturate(p.age / p.lifespan);
#if 1
	float size = lerp(p.size.x/*spawn*/, p.size.y/*despawn*/, age_garadient);
#else	
	float size = p.max_size;
#endif
	
	// =====================================
    // typeによる分岐
    // 0 = Point, 1 = Quad
    // =====================================
    //if (false)
    if (p.type == 0)
    {
    [unroll]
        for (uint vertex_index = 0; vertex_index < 4; ++vertex_index)
        {
            GS_OUT element;

        // 粒のワールド位置をViewProjectionで変換
            element.position = mul(float4(p.position, 1), view_projection);

        // 角のローカル座標
            float2 corner = corners[vertex_index];

        // 拡大（Point用に極小スケール）
            element.position.xy += corner * float2(p.scale.x, p.scale.y * aspect_ratio);

        // テクスチャは使わず、色のみ
            element.color = p.color;
            element.texcoord = texcoords[vertex_index]; // 必須ではないが形式上入れておく
            element.type = p.type;

        // 出力
            output.Append(element);
        }

    // 1つの四角形が終了したことを通知
        output.RestartStrip();
        return;
    }
    else
    {
	
	//----------------------------------------------
    // 4つの頂点（スプライト）を生成してストリーム出力
    //----------------------------------------------
	[unroll]
        for (uint vertex_index = 0; vertex_index < 4; ++vertex_index)
        {
            GS_OUT element;

        //------------------------------------------
        // 粒のワールド位置をViewProjectionで変換
        //------------------------------------------
            element.position = mul(float4(p.position, 1), view_projection);
		
            float2 corner;
#if 1	
		//--------------------------------------------------
        // パーティクルの回転を考慮（スプライトを回す）
        //--------------------------------------------------
            corner.x = corners[vertex_index].x * cos(p.angle) - corners[vertex_index].y * sin(p.angle);
            corner.y = corners[vertex_index].x * sin(p.angle) + corners[vertex_index].y * cos(p.angle);
#else
		corner = corners[vertex_index];
#endif
		
		//------------------------------------------
        // 四角形を粒の中心を基準に拡大
        //------------------------------------------
            element.position.xy += corner * float2(size, size * aspect_ratio);
		
	    //------------------------------------------
        // 粒の色をセット
        //------------------------------------------
            element.color = p.color;
#if 0
		float fade_in_time = 2.0;
		float fade_out_time = 5.0;
		float alpha = fade_in(fade_in_time, p.age, 1) * fade_out(fade_out_time, p.age, p.lifespan, 1);
		element.color.a = pow(alpha, 1);
#endif
		
		
#if 1
		
		//------------------------------------------
        // スプライトシート対応
        // （複数アニメーションを1枚にまとめたテクスチャ）
        //------------------------------------------
            float2 texcoord = texcoords[vertex_index] / sprite_sheet_grid;
            float2 grid_size = 1.0 / sprite_sheet_grid;
            uint x = p.chip % sprite_sheet_grid.x;
            uint y = p.chip / sprite_sheet_grid.x;
            element.texcoord = texcoord + grid_size * uint2(x, y);
#else
		element.texcoord = texcoords[vertex_index];
#endif
            element.type = p.type;
		//------------------------------------------
        // 出力
        //------------------------------------------
            output.Append(element);
        }
	 //--------------------------------------------------
     // 1つの四角形が終了したことを通知
     //--------------------------------------------------
        output.RestartStrip();
    }
    
}

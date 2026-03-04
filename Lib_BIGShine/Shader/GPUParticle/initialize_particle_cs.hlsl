#include "particle.hlsli" // 共通定義

// GPU上で書き込み可能な粒子バッファ
RWStructuredBuffer<particle> particle_buffer : register(u0);

// ====================================================
// Compute Shader のメイン関数（粒子初期化専用）
// すべての粒子をspawn関数で初期化する
// ====================================================
[numthreads(NUMTHREADS_X, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint id = dtid.x;

	// 現在の粒子データを取得
    particle p = particle_buffer[id];
	
	// --------------------------------------
	// 粒子を初期化
	// --------------------------------------
    spawn(id, p);
	
	// --------------------------------------
    // バーストモードの場合は死亡状態にする
    // （トリガーが来るまで待機）
    // --------------------------------------
    if (emission_mode == 1)
    {
        // バーストモード時は死亡状態で待機
        p.state = STATE_DEAD;
        p.color.a = 0.0f; // 透明化
        p.age = p.lifespan + 1.0f; // 寿命切れ状態にする
    }
    else
    {
        // 継続放出モード時は生存状態
        p.state = STATE_ALIVE;
    }
	
	// 更新後の粒子データを書き戻す
    particle_buffer[id] = p;
}

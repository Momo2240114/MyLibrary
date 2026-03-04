#include "ParticleCommon.hlsli"

// ダブルバッファリング対応
StructuredBuffer<Particle> OldParticles : register(t0);
RWStructuredBuffer<Particle> NewParticles : register(u0);

// 高精度なハッシュ関数
float randam(uint seed)
{
    seed = (seed << 13U) ^ seed;
    seed = seed * (seed * seed * 15731U + 789221U) + 1376312589U;
    return float(seed & 0x7fffffffU) / float(0x7fffffff);
}

// 粒子の発生処理
void Spawn(uint id, inout Particle p)
{
    p.isActive = 1;
    p.age = 0.0f;
    p.position = emissionPosition.xyz;

    // --- 飛び散りロジック (球状/円錐状) ---
    float r1 = randam(id + (uint) (time * 1000.0));
    float r2 = randam(id * 2 + (uint) (time * 1000.0));
    float phi = 2.0 * PI * r1;
    float theta = r2 * emissionConeAngle.x; // 角度

    float3 dir = float3(sin(theta) * cos(phi), cos(theta), sin(theta) * sin(phi));
    
    // 速度と寿命のバラつき
    float speed = lerp(emissionSpeed.x, emissionSpeed.y, randam(id + 7));
    p.velocity = dir * speed;
    p.lifeSpan = lerp(lifespan.x, lifespan.y, randam(id + 8));

    p.startColor = manualColor;
    p.endColor = manualColor;
    p.endColor.a = 0.0f; // 最後は消える
}

// 生きている粒子の更新
void UpdateParticle(inout Particle p)
{
    p.velocity.y += gravity * deltaTime; // 重力適用
    p.position += p.velocity * deltaTime; // 移動
    p.rotation += emissionAngularSpeed.x * deltaTime; // 回転
}

[numthreads(NUMTHREADS_X, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint id = dtid.x;
    if (id >= maxParticleCount) return;

    // 1. 前フレームのデータを取得
    Particle p = OldParticles[id];

    // 2. 発生ロジック
    if (emissionMode == 1)
    { // Burst
        // トリガーON、かつ自分が死んでいるなら発生
        if (burstTrigger > 0.5f && id < burstCount && p.isActive == 0) {
            Spawn(id, p);
        }
    } 
    else { // Continuous
        if (p.isActive == 0) {
            if (randam(id + (uint)(time * 1000.0)) > 0.99f) {
                Spawn(id, p);
            }
        }
    }

    // 3. 更新ロジック（一度 isActive=1 になれば、トリガーが 0 になってもここを通る）
    if (p.isActive == 1) {
        p.age += deltaTime;
        if (p.age >= p.lifeSpan) {
            p.isActive = 0;
        } else {
            UpdateParticle(p); // 物理移動
        }
    }

    // 4. 重要：更新したデータを必ず書き込む
    NewParticles[id] = p;
}
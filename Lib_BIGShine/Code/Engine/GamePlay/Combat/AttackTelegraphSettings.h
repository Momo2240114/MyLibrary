#pragma once

#include <DirectXMath.h>
#include <string>

// 攻撃イベントの時間から導出する予兆と、モデルごとの発生位置を保持する。
struct AttackTelegraphSettings
{
    bool enabled = true;
    // 60FPS基準のアニメーションフレーム数。実行時の描画FPSには依存しない。
    float leadFrames = 12.0f;
    float berserkLeadFrames = 9.0f;
    std::string particleName = "EnemyAttackWarning";
    std::string attachNodeName;
    DirectX::XMFLOAT3 localOffset = { 0.0f, 0.3f, 0.0f };
    // ノードが未指定・取得不能の場合は、所有者の姿勢に対してこの位置を使用する。
    DirectX::XMFLOAT3 fallbackOffset = { 0.0f, 2.0f, 0.0f };
    float particleScale = 1.0f;
    float lifeTime = 0.15f;
};

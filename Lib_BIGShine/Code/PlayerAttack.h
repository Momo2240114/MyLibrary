#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/GamePlay/Input/KeyConfig.h"
#include "Engine/GamePlay/Camera/CameraComponent.h"

#include <DirectXMath.h>

class PlayerAttack : public Component
{
public:
    const std::string& GetName() const override { static std::string name = "PlayerAttack"; return name; }

    float attackDuration = 0.5f;   // 攻撃モーションの長さ
    float timer = 0.0f;           // 現在の経過時間
    bool isAttacking = false;     // 攻撃中か？

    void Update(float deltaTime) override
    {
        if (isAttacking)
        {
            timer -= deltaTime;
            if (timer <= 0.0f)
            {
                isAttacking = false;
            }
        }
    }

    // 攻撃開始
    void DoAttack()
    {
        if (isAttacking) return; // すでに攻撃中なら無視

        isAttacking = true;
        timer = attackDuration;

        // ここで攻撃判定(Collision)の有効化やエフェクト生成の指示を出す
        // printf("Attack Start!\n");
    }

    void Gui() override
    {
        ImGui::Checkbox("Is Attacking", &isAttacking);
        ImGui::SliderFloat("Duration", &attackDuration, 0.1f, 2.0f);
        ImGui::ProgressBar(timer / attackDuration);
    }
};
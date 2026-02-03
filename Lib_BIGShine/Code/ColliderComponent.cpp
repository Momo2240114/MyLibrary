#include "ColliderComponent.h"
#include "Collision.h"

void ColliderComponent::Gui()
{
    ImGui::Checkbox("ShapeRenderFlg", &shapeRenderFlg);
    ImGui::Checkbox("Enabled", &enabled);
    ImGui::Checkbox("Is Static", &isStatic);
    ImGui::Checkbox("Is Trigger", &isTrigger);
    ImGui::Separator();
    ImGui::Checkbox("Use Gravity", &useGravity);

    ImGui::Checkbox("IsHit", &m_IsHitFlag);

    if (useGravity) {
        ImGui::DragFloat("Gravity Scale", &gravityScale, 0.1f, 0.0f, 10.0f);
    }
    ImGui::Separator();
    ImGui::DragFloat3("Offset", &offset.x, 0.01f);
    ImGui::ColorEdit3("PrimitiveColor", &m_DebugPrimitiveColor.x);
}
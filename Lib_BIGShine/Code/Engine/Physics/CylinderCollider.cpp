#include "CylinderCollider.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"

void CylinderCollider::Gui()
{
    ColliderComponent::Gui();

    bool changed_ = false;
    changed_ |= ImGui::DragFloat("Radius", &radius, 0.01f, 0.01f, 1000.0f);
    changed_ |= ImGui::DragFloat("Half Height", &halfHeight, 0.01f, 0.01f, 1000.0f);
    changed_ |= ImGui::SliderInt("Segments", &segments, 6, 64);
    if (changed_) RecreatePhysXActor();
}

void CylinderCollider::Update(float dt_)
{
    (void)dt_;
    if (!shapeRenderEnabled) return;

    auto owner_ = GetGameObject();
    if (!owner_) return;

    auto scale_ = owner_->transform->GetWorldScale();
    auto world_ = owner_->transform->GetWorld();
    world_._41 += offset.x;
    world_._42 += offset.y;
    world_._43 += offset.z;

    auto& renderer_ = GraphicsDeviceManager::Instance().GetShapeRendererSystem();
    renderer_.DrawCylinder(world_, radius * scale_.x, halfHeight * 2.0f * scale_.y, debugPrimitiveColor);
}

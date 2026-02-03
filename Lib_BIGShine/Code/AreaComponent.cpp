#include "AreaComponent.h"

#include"ShapeRenderer.h"
#include"GraphicsDeviceManager.h"

#include"GameObjectManager.h"
#include"SearchAreaManager.h"
void AreaComponent::Start()
{
	GameObjectManager::Instance().GetSearchAreaManager()->SetAreaComponent(this);
}

void AreaComponent::Update(float et)
{
	m_Position = GetGameObject()->transform->GetWorldPosition();

	Vector4 color = {1,1,1,1};
	GraphicsDeviceManager::Instance().GetShapeRendererSystem().DrawSphere(m_Position, m_Radius,color);
}

void AreaComponent::Gui()
{
	ImGui::DragFloat("Radius",&m_Radius);
}


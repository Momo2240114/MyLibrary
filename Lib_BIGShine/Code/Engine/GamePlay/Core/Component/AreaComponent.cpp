#include "AreaComponent.h"

#include"Engine/Graphics/"
#include"GraphicsDeviceManager.h"

#include"GameObjectManager.h"
#include"SearchAreaManager.h"
void AreaComponent::Start()
{
	// SearchAreaManagerへ自身を登録し、検索範囲の中心として使えるようにする。
	GameObjectManager::Instance().GetSearchAreaManager()->SetAreaComponent(this);
}

void AreaComponent::Update(float et)
{
	// 毎フレームTransform位置を反映し、Debug表示用の球を描く。
	m_Position = GetGameObject()->transform->GetWorldPosition();

	Vector4 color = {1,1,1,1};
	GraphicsDeviceManager::Instance().GetShapeRendererSystem().DrawSphere(m_Position, m_Radius,color);
}

void AreaComponent::Gui()
{
	// 検索範囲の半径をEditor上で調整する。
	ImGui::DragFloat("Radius",&m_Radius);
}


//#include "SpinComponent.h"
//#include "GameObject.h"
//#include "MovementComponent.h"
//
//void SpinComponent::Gui()
//{
//	ImGui::DragFloat3("SpinVec", &SpinVec.x);
//}
//
//void SpinComponent::Update(float et)
//{
//	auto obj =  GetGameObject();
//	auto move = obj->GetComponent<MovementComponent>();
//
//	move->AddAngle(SpinVec);
//}
//
//void SpinComponent::Serialize(nlohmann::json& j)const
//{
//	j["SpinVec"] = { SpinVec.x, SpinVec.y, SpinVec.z };
//}
//
//void SpinComponent::Deserialize(const nlohmann::json& j)
//{
//	SpinVec = { j["SpinVec"][0], j["SpinVec"][1], j["SpinVec"][2] };
//}
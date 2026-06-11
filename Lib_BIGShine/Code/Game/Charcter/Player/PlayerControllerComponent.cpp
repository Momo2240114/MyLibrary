#include "PlayerControllerComponent.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Transform/Transform.h"
#include "Engine/Physics/ColliderComponent.h"

#include "PlayerIdleState.h"

#include "Engine/GamePlay/Serialize/ComponentRegistry.h"

void PlayerControllerComponent::Start()
{
    auto owner_ = GetGameObject();
    if (!owner_) return;

    owner_->SetLayer(GameObject::LayerType::Player);

    collider = owner_->GetComponent<ColliderComponent>();

    stateMachine.AddState<PlayerIdleState>(PLAYER_STATE::IDLE);
    //State‚Ì’Ç‰Á‚Í‚±‚±‚©‚ç
    //stateMachine.AddState<PlayerMoveState>(PLAYER_STATE::MOVE);
    //stateMachine.AddState<PlayerJumpState>(PLAYER_STATE::JUMP);
    //stateMachine.AddState<PlayerFallState>(PLAYER_STATE::FALL);

    stateMachine.ChangeState(*this, PLAYER_STATE::IDLE);
}

void PlayerControllerComponent::Update(float dt_)
{
    stateMachine.Update(*this, dt_);
}

DirectX::XMFLOAT3 PlayerControllerComponent::GetMoveInput() const
{
    //“ü—Í‚È‚Ç‚ª‚ ‚é‚È‚ç
    DirectX::XMFLOAT3 input_{ 0, 0, 0 };
    return input_;
}

bool PlayerControllerComponent::IsJumpPressed() const
{
    //“ü—Í‚È‚Ç‚ª‚ ‚é‚È‚ç
    return false;
}

bool PlayerControllerComponent::IsGrounded() const
{
    return collider && collider->isGrounded;
}

void PlayerControllerComponent::SetHorizontalVelocity(
    const DirectX::XMFLOAT3& dir,
    float speed)
{
    if (!collider) return;

    collider->velocity.x = dir.x * speed;
    collider->velocity.z = dir.z * speed;
}

void PlayerControllerComponent::Jump()
{
    if (!collider) return;

    collider->velocity.y = jumpPower;
    collider->isGrounded = false;
}

void PlayerControllerComponent::Gui()
{
    ImGui::Text("State: %s", stateMachine.GetCurrentStateName());
    ImGui::DragFloat("Move Speed", &moveSpeed, 0.1f, 0.0f, 100.0f);
    ImGui::DragFloat("Jump Power", &jumpPower, 0.1f, 0.0f, 100.0f);
}

void PlayerControllerComponent::Serialize(nlohmann::json& j) const
{
    j["MoveSpeed"] = moveSpeed;
    j["JumpPower"] = jumpPower;
}

void PlayerControllerComponent::Deserialize(const nlohmann::json& j)
{
    moveSpeed = j.value("MoveSpeed", moveSpeed);
    jumpPower = j.value("JumpPower", jumpPower);
}


REGISTER_COMPONENT(PlayerControllerComponent,"PlayerController")
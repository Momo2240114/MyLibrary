#pragma once

#include "ComponentBase.h"
#include "Vector.h"


enum class RotationType {
    None,
    LookAtVelocity, // ˆÚ“®•ûŒü‚ðŒü‚­
};

class Movement : public Component
{
public:
    const std::string& GetName() const override { static std::string name = "Movement"; return name; }

    float moveSpeed = 5.0f;
    float rotateSpeed = 10.0f; // •âŠÔ‘¬“x

    void Start() override;
    void Update(float deltaTime) override;

    void SetVelocity(const DirectX::XMFLOAT3& v) { velocity = v; }
    void SetRotationType(RotationType type) { rotationType = type; }

private:
    void ApplyRotation(float deltaTime);

    DirectX::XMFLOAT3 velocity{ 0, 0, 0 };
    RotationType rotationType = RotationType::LookAtVelocity;
};
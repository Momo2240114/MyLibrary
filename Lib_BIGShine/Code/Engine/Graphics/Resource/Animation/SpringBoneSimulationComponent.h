#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "IKMapComponent.h"

class GltfModelCommponennt;

class SpringBoneSimulationComponent : public Component
{
public:
    struct ClothConstraint
    {
        size_t chainIndexA = 0;
        size_t pointIndexA = 0;
        size_t chainIndexB = 0;
        size_t pointIndexB = 0;
        float targetLength = 0.0f;
        bool diagonal = false;
    };

    const std::string& GetName() const override;
    void Start() override;
    void LateUpdate(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    void ResetSimulation();

    static DirectX::XMFLOAT3 windDirection;
    static float windStrength;
    static bool windEnabled;

private:
    bool ResolveDependencies();
    bool ResolveChain(IKMapComponent::IKChain& chain_);
    void SimulateSpringChain(IKMapComponent::IKChain& chain_, float deltaTime_);
    void BuildClothConstraints();
    void SolveClothConstraints();
    void SolveClothConstraint(const ClothConstraint& constraint_);
    void ResolveBodyCollision(IKMapComponent::IKChain& chain_, size_t pointIndex_);
    bool GetBodyCollisionSphereCenter(IKMapComponent::BodyCollisionSphere& sphere_, DirectX::XMFLOAT3& center_);
    void ApplySpringChain(IKMapComponent::IKChain& chain_);
    void DrawBodyCollisionShapes();
    void DrawHairCollisionShapes();

private:
    IKMapComponent* ikMap = nullptr;
    GltfModelCommponennt* model = nullptr;
    std::vector<ClothConstraint> clothConstraints;
    bool clothConstraintsInitialized = false;
    bool showBodyCollisionShapes = true;
    DirectX::XMFLOAT4 bodyCollisionShapeColor = { 1.0f, 0.45f, 0.1f, 1.0f };
    bool showHairCollisionShapes = false;
    DirectX::XMFLOAT4 hairCollisionShapeColor = { 0.2f, 0.8f, 1.0f, 0.6f };
};
#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"

class GltfModelCommponennt;

class IKMapComponent : public Component
{
public:
    enum class IK_TYPE
    {
        SPRING_BONE,
        FOOT,
        CLOTH_BONE
    };

    struct IKChain
    {
        std::string name = "Hair";
        IK_TYPE type = IK_TYPE::SPRING_BONE;
        std::string rootNodeName;
        std::string tipNodeName;
        float gravity = 9.8f;
        float damping = 0.88f;
        float stiffness = 0.02f;
        float bendStiffness = 0.02f;
        float lateralStiffness = 0.35f;
        float diagonalStiffness = 0.15f;
        float tipStiffnessScale = 0.2f;
        float rootRotationFollow = 0.7f;
        float influence = 1.0f;
        float collisionRadius = 0.02f;
        int solverSubsteps = 4;
        int solverIterations = 6;
        int clothGroup = 0;
        int clothOrder = 0;
        bool collideWithBodyColliders = true;
        bool enabled = true;

        float startLift = 0.08f;          // èâä˙ÇæÇØè„Ç…ì¶Ç™Ç∑ó 
        int startCollisionWarmup = 4;     // èâä˙âüÇµèoÇµâÒêî

        std::vector<int> nodeIndices;
        std::vector<float> segmentLengths;
        std::vector<float> bendLengths;
        std::vector<DirectX::XMFLOAT3> simulatedPositions;
        std::vector<DirectX::XMFLOAT3> previousPositions;
        DirectX::XMFLOAT4X4 previousRootTransform = {
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            0, 0, 0, 1
        };
        bool initialized = false;
    };

    struct BodyCollisionSphere
    {
        std::string name = "Head";
        std::string nodeName;
        DirectX::XMFLOAT3 offset = { 0.0f, 0.0f, 0.0f };
        float radius = 0.15f;
        bool connectToNext = false;
        bool enabled = true;
        int nodeIndex = -1;
    };

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
    void Serialize(nlohmann::json& j_) const override;
    void Deserialize(const nlohmann::json& j_) override;

    IKChain& AddSpringChain(const std::string& name_, const std::string& rootNodeName_, const std::string& tipNodeName_);
    std::vector<IKChain>& GetChains() { return chains; }
    const std::vector<IKChain>& GetChains() const { return chains; }
    std::vector<BodyCollisionSphere>& GetBodyCollisionSpheres() { return bodyCollisionSpheres; }
    const std::vector<BodyCollisionSphere>& GetBodyCollisionSpheres() const { return bodyCollisionSpheres; }

    void ResetSimulation();
    void RequestSimulationWarmup()
    {
        simulationWarmupRequested = true;
    }
private:
    bool simulationWarmupRequested = false;
    GltfModelCommponennt* model = nullptr;
    std::vector<IKChain> chains;
    std::vector<BodyCollisionSphere> bodyCollisionSpheres;
    std::vector<ClothConstraint> clothConstraints;
    bool clothConstraintsInitialized = false;
    bool showBodyCollisionShapes = true;
    DirectX::XMFLOAT4 bodyCollisionShapeColor = { 1.0f, 0.45f, 0.1f, 1.0f };
    bool showHairCollisionShapes = false;
    DirectX::XMFLOAT4 hairCollisionShapeColor = { 0.2f, 0.8f, 1.0f, 0.6f };
    bool ResolveChain(IKChain& chain_);
    void SimulateSpringChain(IKChain& chain_, float deltaTime_);
    void BuildClothConstraints();
    void SolveClothConstraints();
    void SolveClothConstraint(const ClothConstraint& constraint_);
    void ResolveBodyCollision(IKChain& chain_, size_t pointIndex_);
    bool GetBodyCollisionSphereCenter(BodyCollisionSphere& sphere_, DirectX::XMFLOAT3& center_);
    void ApplySpringChain(IKChain& chain_);
    void DrawBodyCollisionShapes();
    void DrawHairCollisionShapes();
};

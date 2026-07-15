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
        CLOTH_BONE,
        LOOK_AT,

        DRAGON_NECK,
        DRAGON_TAIL,
        UPPER_BODY_LEAN,
        SHIELD_BLOCK
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

        // FootIKComponent settings (used only when type is FOOT).
        std::string footPoleNodeName;
        float footMaxStepUp = 0.45f;
        float footMaxStepDown = 0.75f;
        float footHeightOffset = 0.03f;
        float footPositionSpeed = 18.0f;
        float footRotationSpeed = 14.0f;
        float footBendWeight = 0.8f;
        int footSolverIterations = 8;
        bool footIncludeDynamicColliders = false;
        bool footAlignToGround = true;

        // HumanLookAtIKComponent settings (used only when type is LOOK_AT).
        DirectX::XMFLOAT3 lookForwardAxis = { 0.0f, 0.0f, 1.0f };
        float lookMaxYawDegrees = 70.0f;
        float lookMaxPitchDegrees = 35.0f;
        float lookRootWeight = 0.18f;
        float lookTipWeight = 1.0f;
        float lookShieldShoulderYawDegrees = 18.0f;
        float lookBlendSpeed = 8.0f;

        // ShieldBlockIKComponent settings (used only when type is SHIELD_BLOCK).
        float shieldElbowWristRatio = 0.7f;
        float shieldSearchRadius = 3.0f;
        float shieldMaxYawDegrees = 80.0f;
        float shieldMaxPitchDegrees = 55.0f;
        std::string shieldPoleNodeName;
        float shieldBlendSpeed = 10.0f;

        float startLift = 0.08f;          // 初期だけ上に逃がす量
        int startCollisionWarmup = 4;     // 初期押し出し回数

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
    bool IsLoadReady() const override;
    void LateUpdate(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& j_) const override;
    void Deserialize(const nlohmann::json& j_) override;

    IKChain& AddSpringChain(const std::string& name_, const std::string& rootNodeName_, const std::string& tipNodeName_);
    IKChain& AddFootChain(const std::string& name_, const std::string& rootNodeName_ = {}, const std::string& tipNodeName_ = {});
    IKChain& AddLookAtChain(const std::string& name_, const std::string& rootNodeName_ = {}, const std::string& tipNodeName_ = {});
    IKChain& AddShieldBlockChain(const std::string& name_, const std::string& rootNodeName_ = {}, const std::string& tipNodeName_ = {});
    IKChain& AddUpperBodyLeanChain(const std::string& name_, const std::string& rootNodeName_ = {}, const std::string& tipNodeName_ = {});
    IKChain& AddDragonNeckChain(
        const std::string& name_,
        const std::string& rootNodeName_ = {},
        const std::string& tipNodeName_ = {});

    IKChain& AddDragonTailChain(
        const std::string& name_,
        const std::string& rootNodeName_ = {},
        const std::string& tipNodeName_ = {});

    IKChain* FindChain(const std::string& name_);
    const IKChain* FindChain(const std::string& name_) const;

    bool ResolveChainByName(const std::string& name_);

    std::vector<IKChain>& GetChains() { return chains; }
    const std::vector<IKChain>& GetChains() const { return chains; }
    std::vector<BodyCollisionSphere>& GetBodyCollisionSpheres() { return bodyCollisionSpheres; }
    const std::vector<BodyCollisionSphere>& GetBodyCollisionSpheres() const { return bodyCollisionSpheres; }

    void ResetSimulation();
    void RequestSimulationWarmup()
    {
        simulationWarmupRequested = true;
    }
    bool ConsumeSimulationWarmupRequest();

    // テスト用グローバル風パラメータ（全インスタンス共通）
    static DirectX::XMFLOAT3 windDirection;   // 正規化済み風向き
    static float              windStrength;    // 風の強さ (m/s^2 スケール)
    static bool               windEnabled;
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
};

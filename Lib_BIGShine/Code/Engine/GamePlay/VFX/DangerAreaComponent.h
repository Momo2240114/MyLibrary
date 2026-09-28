#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/GamePlay/EventSystem/IActionProvider.h"
#include "Engine/Graphics/VFX/MeshVFXResource.h"

#include <DirectXMath.h>
#include <memory>
#include <string>
#include <vector>

class GameObject;
class MeshVFXComponent;

enum class DangerAreaShapeType
{
    // 円形の危険範囲。
    CIRCLE,

    // 前方へ伸びるカプセル形状の危険範囲。
    CAPSULE,

    // 前方へ広がる扇形の危険範囲。
    FAN,
};

struct DangerAreaTemplate
{
    // Editor表示名とActionSequenceから呼び出すAction名。
    std::string name = "DangerArea";
    std::string actionName = "DangerArea_Show";
    DangerAreaShapeType shapeType = DangerAreaShapeType::CAPSULE;

    // VFXMaterialを差し替えることで見た目だけを調整できる。
    std::string materialPath = "Data/VFXMaterials/EnemyDangerArea.vfxmat";
    std::string sourceNodeName;
    DirectX::XMFLOAT3 localOffset = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 localRotation = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 localScale = { 1.0f, 1.0f, 1.0f };

    float radius = 1.0f;
    float length = 4.0f;
    float fanAngleDegrees = 60.0f;
    float lifeTime = 1.0f;
    float groundOffset = 0.03f;
    float playbackSpeed = 1.0f;
    bool projectToOwnerGround = true;
    bool followOwner = false;
};

void to_json(nlohmann::json& json_, const DangerAreaTemplate& template_);
void from_json(const nlohmann::json& json_, DangerAreaTemplate& template_);

// 攻撃予兆として表示する危険範囲をEditorで設定し、ActionSequenceから呼び出せるComponent。
class DangerAreaComponent : public Component, public IActionProvider
{
public:
    const std::string& GetName() const override { return componentName; }
    void Start() override;
    void Update(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;
    void RegisterActions() override;

    // 指定Indexの危険範囲を生成する。
    void Show(size_t templateIndex_);

    // Template名またはAction名から危険範囲を生成する。
    void ShowByName(const std::string& templateName_);

    std::vector<DangerAreaTemplate>& GetTemplates() { return templates; }
    const std::vector<DangerAreaTemplate>& GetTemplates() const { return templates; }

private:
    struct DangerAreaInstance
    {
        std::shared_ptr<GameObject> object;
        std::weak_ptr<MeshVFXComponent> meshVFX;
        size_t templateIndex = 0u;
        float lifeTimer = 0.0f;
    };

    struct EmitFrame
    {
        DirectX::XMMATRIX sourceMatrix = DirectX::XMMatrixIdentity();
        DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
    };

    EmitFrame BuildEmitFrame(const DangerAreaTemplate& template_) const;
    DirectX::XMMATRIX BuildLocalMatrix(
        const DirectX::XMFLOAT3& position_,
        const DirectX::XMFLOAT3& rotation_,
        const DirectX::XMFLOAT3& scale_) const;
    MeshVFXAsset BuildDangerAreaAsset(const DangerAreaTemplate& template_) const;
    void ApplyInstanceTransform(
        const DangerAreaTemplate& template_,
        const EmitFrame& frame_,
        const std::shared_ptr<GameObject>& object_) const;
    void StopInstance(DangerAreaInstance& instance_);

private:
    static inline const std::string componentName = "DangerAreaComponent";

    // Editorで保存する危険範囲テンプレート一覧。
    std::vector<DangerAreaTemplate> templates;
    std::vector<DangerAreaInstance> instances;
};

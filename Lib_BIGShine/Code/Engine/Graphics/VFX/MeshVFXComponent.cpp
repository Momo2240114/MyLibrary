#include "MeshVFXComponent.h"

#include "Editor/AssetSlotGui.h"
#include "Engine/Graphics/Core/CPUResourceLoader.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Graphics/Resource/ModelResource.h"
#include "Engine/Graphics/Resource/ModelResourceManager.h"
#include "Engine/Graphics/Resource/TextureResourceManager.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/Math/Curve/CurveComponent.h"
#include "Engine/Platform/WindowsDialogHelper.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <nlohmann/json.hpp>
#include <cmath>
#include <utility>

namespace
{
    constexpr const char* LEGACY_SHAPE_CYLINDER_MODEL_PATH = "__ShapeCylinder__";
    constexpr const char* LEGACY_SLASH_CRESCENT_MODEL_PATH = "__SlashCrescent__";
    constexpr const char* LEGACY_SLASH_FAN_MODEL_PATH = "__SlashFan__";
    constexpr const char* LEGACY_SLASH_ARC_RIBBON_MODEL_PATH = "__SlashArcRibbon__";
    constexpr const char* LEGACY_SLASH_CROSS_MODEL_PATH = "__SlashCross__";
    constexpr const char* LEGACY_SLASH_GROUND_MODEL_PATH = "__SlashGround__";
    constexpr const char* LEGACY_SLASH_CROSS_CRESCENT_MODEL_PATH = "__SlashCrossCrescent__";
    constexpr const char* LEGACY_SLASH_VERTICAL_CRESCENT_MODEL_PATH = "__SlashVerticalCrescent__";
    constexpr const char* LEGACY_SLASH_RAISED_CRESCENT_MODEL_PATH = "__SlashRaisedCrescent__";

    constexpr const char* INTERNAL_MESH__MODEL_PATH = "__InternalMesh__";
    constexpr const char* INTERNAL_MESH_CYLINDER_MODEL_PATH = "__InternalMeshCylinder__";
    constexpr const char* INTERNAL_MESH_CONE_MODEL_PATH = "__InternalMeshCone__";
    constexpr const char* INTERNAL_MESH_SLASH_CRESCENT_MODEL_PATH = "__InternalMeshSlashCrescent__";
    constexpr const char* INTERNAL_MESH_SLASH_FAN_MODEL_PATH = "__InternalMeshSlashFan__";
    constexpr const char* INTERNAL_MESH_SLASH_ARC_RIBBON_MODEL_PATH = "__InternalMeshSlashArcRibbon__";
    constexpr const char* INTERNAL_MESH_SLASH_CROSS_MODEL_PATH = "__InternalMeshSlashCross__";
    constexpr const char* INTERNAL_MESH_SLASH_GROUND_MODEL_PATH = "__InternalMeshSlashGround__";
    constexpr const char* INTERNAL_MESH_SLASH_CROSS_CRESCENT_MODEL_PATH = "__InternalMeshSlashCrossCrescent__";
    constexpr const char* INTERNAL_MESH_SLASH_VERTICAL_CRESCENT_MODEL_PATH = "__InternalMeshSlashVerticalCrescent__";
    constexpr const char* INTERNAL_MESH_SLASH_RAISED_CRESCENT_MODEL_PATH = "__InternalMeshSlashRaisedCrescent__";

    constexpr int INTERNAL_MESH_CYLINDER_SEGMENTS = 256;
    constexpr int INTERNAL_MESH_SLASH_SEGMENTS = 96;

    bool CreateStaticMeshBuffers(
        ID3D11Device* device_,
        GltfModelCommponennt::Mesh& mesh_)
    {
        if (!device_ || mesh_.vertices.empty() || mesh_.indices.empty()) return false;

        D3D11_BUFFER_DESC vertexDesc_{};
        vertexDesc_.ByteWidth = static_cast<UINT>(
            sizeof(GltfModelCommponennt::Vertex) * mesh_.vertices.size());
        vertexDesc_.Usage = D3D11_USAGE_IMMUTABLE;
        vertexDesc_.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA vertexData_{};
        vertexData_.pSysMem = mesh_.vertices.data();
        if (FAILED(device_->CreateBuffer(
            &vertexDesc_,
            &vertexData_,
            mesh_.vertexBuffer.GetAddressOf())))
        {
            return false;
        }

        D3D11_BUFFER_DESC indexDesc_{};
        indexDesc_.ByteWidth = static_cast<UINT>(sizeof(uint32_t) * mesh_.indices.size());
        indexDesc_.Usage = D3D11_USAGE_IMMUTABLE;
        indexDesc_.BindFlags = D3D11_BIND_INDEX_BUFFER;

        D3D11_SUBRESOURCE_DATA indexData_{};
        indexData_.pSysMem = mesh_.indices.data();
        if (FAILED(device_->CreateBuffer(
            &indexDesc_,
            &indexData_,
            mesh_.indexBuffer.GetAddressOf())))
        {
            return false;
        }

        return true;
    }

    std::shared_ptr<ModelResource> CreateGeneratedModelResource(
        const char* modelName_)
    {
        auto resource_ = std::make_shared<ModelResource>();
        resource_->modelName = modelName_;

        auto& node_ = resource_->nodes.emplace_back();
        node_.name = modelName_;

        auto& material_ = resource_->materials.emplace_back();
        material_.id = 0;
        material_.name = modelName_;

        auto& mesh_ = resource_->meshes.emplace_back();
        mesh_.nodeIndex = 0;
        mesh_.materialIndex = 0;
        mesh_.node = &resource_->nodes[0];
        mesh_.material = &resource_->materials[0];

        return resource_;
    }

    void NormalizeInternalMeshModelPath(std::string& modelPath_)
    {
        if (modelPath_ == LEGACY_SHAPE_CYLINDER_MODEL_PATH)
        {
            modelPath_ = INTERNAL_MESH_CYLINDER_MODEL_PATH;
        }
        else if (modelPath_ == LEGACY_SLASH_CRESCENT_MODEL_PATH)
        {
            modelPath_ = INTERNAL_MESH_SLASH_CRESCENT_MODEL_PATH;
        }
        else if (modelPath_ == LEGACY_SLASH_FAN_MODEL_PATH)
        {
            modelPath_ = INTERNAL_MESH_SLASH_FAN_MODEL_PATH;
        }
        else if (modelPath_ == LEGACY_SLASH_ARC_RIBBON_MODEL_PATH)
        {
            modelPath_ = INTERNAL_MESH_SLASH_ARC_RIBBON_MODEL_PATH;
        }
        else if (modelPath_ == LEGACY_SLASH_CROSS_MODEL_PATH)
        {
            modelPath_ = INTERNAL_MESH_SLASH_CROSS_MODEL_PATH;
        }
        else if (modelPath_ == LEGACY_SLASH_GROUND_MODEL_PATH)
        {
            modelPath_ = INTERNAL_MESH_SLASH_GROUND_MODEL_PATH;
        }
        else if (modelPath_ == LEGACY_SLASH_CROSS_CRESCENT_MODEL_PATH)
        {
            modelPath_ = INTERNAL_MESH_SLASH_CROSS_CRESCENT_MODEL_PATH;
        }
        else if (modelPath_ == LEGACY_SLASH_VERTICAL_CRESCENT_MODEL_PATH)
        {
            modelPath_ = INTERNAL_MESH_SLASH_VERTICAL_CRESCENT_MODEL_PATH;
        }
        else if (modelPath_ == LEGACY_SLASH_RAISED_CRESCENT_MODEL_PATH)
        {
            modelPath_ = INTERNAL_MESH_SLASH_RAISED_CRESCENT_MODEL_PATH;
        }
    }

    GltfModelCommponennt::Vertex CreateInternalMeshSlashVertex(
        const DirectX::XMFLOAT3& position_,
        float u_,
        float v_)
    {
        GltfModelCommponennt::Vertex vertex_{};
        vertex_.position = position_;
        vertex_.normal = { 0.0f, 1.0f, 0.0f };
        vertex_.tangent = { 1.0f, 0.0f, 0.0f, 1.0f };
        vertex_.texcoord = { u_, v_ };
        return vertex_;
    }

    void AddQuadIndices(
        std::vector<uint32_t>& indices_,
        uint32_t leftBottom_,
        uint32_t leftTop_,
        uint32_t rightBottom_,
        uint32_t rightTop_)
    {
        indices_.push_back(leftBottom_);
        indices_.push_back(leftTop_);
        indices_.push_back(rightBottom_);
        indices_.push_back(rightBottom_);
        indices_.push_back(leftTop_);
        indices_.push_back(rightTop_);
    }

    void AddRibbonStrip(
        GltfModelCommponennt::Mesh& mesh_,
        const std::vector<DirectX::XMFLOAT3>& innerPoints_,
        const std::vector<DirectX::XMFLOAT3>& outerPoints_)
    {
        const size_t pointCount_ = (std::min)(innerPoints_.size(), outerPoints_.size());
        if (pointCount_ < 2) return;

        const uint32_t baseIndex_ = static_cast<uint32_t>(mesh_.vertices.size());
        for (size_t index_ = 0; index_ < pointCount_; ++index_)
        {
            const float u_ = static_cast<float>(index_) /
                static_cast<float>(pointCount_ - 1);
            mesh_.vertices.push_back(CreateInternalMeshSlashVertex(innerPoints_[index_], u_, 1.0f));
            mesh_.vertices.push_back(CreateInternalMeshSlashVertex(outerPoints_[index_], u_, 0.0f));
        }

        for (size_t index_ = 0; index_ + 1 < pointCount_; ++index_)
        {
            const uint32_t current_ = baseIndex_ + static_cast<uint32_t>(index_ * 2);
            const uint32_t next_ = current_ + 2;
            AddQuadIndices(
                mesh_.indices,
                current_,
                current_ + 1,
                next_,
                next_ + 1);
        }
    }

    void AddStraightRibbon(
        GltfModelCommponennt::Mesh& mesh_,
        float length_,
        float width_,
        float angleDegrees_,
        const DirectX::XMFLOAT3& offset_)
    {
        const float angle_ = DirectX::XMConvertToRadians(angleDegrees_);
        const float cos_ = std::cos(angle_);
        const float sin_ = std::sin(angle_);
        const DirectX::XMFLOAT2 axis_ = { cos_, sin_ };
        const DirectX::XMFLOAT2 normal_ = { -sin_, cos_ };
        const float halfLength_ = length_ * 0.5f;
        const float halfWidth_ = width_ * 0.5f;

        const DirectX::XMFLOAT3 leftBottom_ = {
            offset_.x - axis_.x * halfLength_ - normal_.x * halfWidth_,
            offset_.y,
            offset_.z - axis_.y * halfLength_ - normal_.y * halfWidth_ };
        const DirectX::XMFLOAT3 leftTop_ = {
            offset_.x - axis_.x * halfLength_ + normal_.x * halfWidth_,
            offset_.y,
            offset_.z - axis_.y * halfLength_ + normal_.y * halfWidth_ };
        const DirectX::XMFLOAT3 rightBottom_ = {
            offset_.x + axis_.x * halfLength_ - normal_.x * halfWidth_,
            offset_.y,
            offset_.z + axis_.y * halfLength_ - normal_.y * halfWidth_ };
        const DirectX::XMFLOAT3 rightTop_ = {
            offset_.x + axis_.x * halfLength_ + normal_.x * halfWidth_,
            offset_.y,
            offset_.z + axis_.y * halfLength_ + normal_.y * halfWidth_ };

        const uint32_t baseIndex_ = static_cast<uint32_t>(mesh_.vertices.size());
        mesh_.vertices.push_back(CreateInternalMeshSlashVertex(leftBottom_, 0.0f, 1.0f));
        mesh_.vertices.push_back(CreateInternalMeshSlashVertex(leftTop_, 0.0f, 0.0f));
        mesh_.vertices.push_back(CreateInternalMeshSlashVertex(rightBottom_, 1.0f, 1.0f));
        mesh_.vertices.push_back(CreateInternalMeshSlashVertex(rightTop_, 1.0f, 0.0f));
        AddQuadIndices(
            mesh_.indices,
            baseIndex_,
            baseIndex_ + 1,
            baseIndex_ + 2,
            baseIndex_ + 3);
    }

    std::shared_ptr<ModelResource> CreateInternalMeshSlashArcModelResource(
        ID3D11Device* device_,
        const char* modelPath_,
        float startAngleDegrees_,
        float endAngleDegrees_,
        float innerRadius_,
        float outerRadius_)
    {
        if (!device_) return nullptr;

        auto resource_ = CreateGeneratedModelResource(modelPath_);
        auto& mesh_ = resource_->meshes[0];
        mesh_.vertices.reserve((INTERNAL_MESH_SLASH_SEGMENTS + 1) * 2);
        mesh_.indices.reserve(INTERNAL_MESH_SLASH_SEGMENTS * 6);

        std::vector<DirectX::XMFLOAT3> innerPoints_;
        std::vector<DirectX::XMFLOAT3> outerPoints_;
        innerPoints_.reserve(INTERNAL_MESH_SLASH_SEGMENTS + 1);
        outerPoints_.reserve(INTERNAL_MESH_SLASH_SEGMENTS + 1);

        for (int index_ = 0; index_ <= INTERNAL_MESH_SLASH_SEGMENTS; ++index_)
        {
            const float rate_ = static_cast<float>(index_) /
                static_cast<float>(INTERNAL_MESH_SLASH_SEGMENTS);
            const float angle_ = DirectX::XMConvertToRadians(
                startAngleDegrees_ + (endAngleDegrees_ - startAngleDegrees_) * rate_);
            const float sin_ = std::sin(angle_);
            const float cos_ = std::cos(angle_);
            innerPoints_.push_back({ sin_ * innerRadius_, 0.0f, cos_ * innerRadius_ });
            outerPoints_.push_back({ sin_ * outerRadius_, 0.0f, cos_ * outerRadius_ });
        }

        AddRibbonStrip(mesh_, innerPoints_, outerPoints_);
        return CreateStaticMeshBuffers(device_, mesh_) ? resource_ : nullptr;
    }

    void BuildSharpCrescentPoints(
        std::vector<DirectX::XMFLOAT3>& innerPoints_,
        std::vector<DirectX::XMFLOAT3>& outerPoints_,
        float startAngleDegrees_,
        float endAngleDegrees_,
        float centerRadius_,
        float maxWidth_,
        float tipWidth_)
    {
        innerPoints_.clear();
        outerPoints_.clear();
        innerPoints_.reserve(INTERNAL_MESH_SLASH_SEGMENTS + 1);
        outerPoints_.reserve(INTERNAL_MESH_SLASH_SEGMENTS + 1);

        for (int index_ = 0; index_ <= INTERNAL_MESH_SLASH_SEGMENTS; ++index_)
        {
            const float rate_ = static_cast<float>(index_) /
                static_cast<float>(INTERNAL_MESH_SLASH_SEGMENTS);

            const float angle_ = DirectX::XMConvertToRadians(
                startAngleDegrees_ + (endAngleDegrees_ - startAngleDegrees_) * rate_);

            const float sin_ = std::sin(angle_);
            const float cos_ = std::cos(angle_);

            // 0 -> 1 -> 0。中央だけ太く、両端を尖らせる。
            const float taper_ = std::sin(rate_ * DirectX::XM_PI);
            const float sharpTaper_ = taper_ * taper_;

            const float width_ = tipWidth_ + maxWidth_ * sharpTaper_;

            // 中心線も少しだけ中央で外へ出して、弓なり感を強める。
            const float currentCenterRadius_ = centerRadius_ + 0.05f * taper_;

            // 内側は浅く、外側は大きく膨らませる。
            const float innerRadius_ = currentCenterRadius_ - width_ * 0.35f;
            const float outerRadius_ = currentCenterRadius_ + width_ * 1.25f;

            innerPoints_.push_back(
                {
                    sin_ * innerRadius_,
                    0.0f,
                    cos_ * innerRadius_
                });

            outerPoints_.push_back(
                {
                    sin_ * outerRadius_,
                    0.0f,
                    cos_ * outerRadius_
                });
        }
    }

    std::shared_ptr<ModelResource> CreateInternalMeshSlashCrescentModelResource(ID3D11Device* device_)
    {
        if (!device_) return nullptr;

        auto resource_ = CreateGeneratedModelResource(INTERNAL_MESH_SLASH_CRESCENT_MODEL_PATH);
        auto& mesh_ = resource_->meshes[0];

        mesh_.vertices.reserve((INTERNAL_MESH_SLASH_SEGMENTS + 1) * 2);
        mesh_.indices.reserve(INTERNAL_MESH_SLASH_SEGMENTS * 6);

        std::vector<DirectX::XMFLOAT3> innerPoints_;
        std::vector<DirectX::XMFLOAT3> outerPoints_;

        BuildSharpCrescentPoints(
            innerPoints_,
            outerPoints_,
            -78.0f,
            78.0f,
            1.22f,
            0.56f,
            0.01f);

        AddRibbonStrip(mesh_, innerPoints_, outerPoints_);
        return CreateStaticMeshBuffers(device_, mesh_) ? resource_ : nullptr;
    }

    GltfModelCommponennt::Vertex CreateInternalMeshSlashVertexFull(
        const DirectX::XMFLOAT3& position_,
        const DirectX::XMFLOAT3& normal_,
        const DirectX::XMFLOAT4& tangent_,
        float u_,
        float v_)
    {
        GltfModelCommponennt::Vertex vertex_{};
        vertex_.position = position_;
        vertex_.normal = normal_;
        vertex_.tangent = tangent_;
        vertex_.texcoord = { u_, v_ };
        return vertex_;
    }

    void AddQuadIndicesDoubleSided(
        std::vector<uint32_t>& indices_,
        uint32_t leftBottom_,
        uint32_t leftTop_,
        uint32_t rightBottom_,
        uint32_t rightTop_)
    {
        indices_.push_back(leftBottom_);
        indices_.push_back(leftTop_);
        indices_.push_back(rightBottom_);

        indices_.push_back(rightBottom_);
        indices_.push_back(leftTop_);
        indices_.push_back(rightTop_);

        indices_.push_back(rightBottom_);
        indices_.push_back(leftTop_);
        indices_.push_back(leftBottom_);

        indices_.push_back(rightTop_);
        indices_.push_back(leftTop_);
        indices_.push_back(rightBottom_);
    }

    void AddVerticalCrescentWall(
        GltfModelCommponennt::Mesh& mesh_,
        float radius_,
        float height_,
        float depth_,
        float startAngleDegrees_,
        float endAngleDegrees_)
    {
        const uint32_t baseIndex_ = static_cast<uint32_t>(mesh_.vertices.size());

        for (int index_ = 0; index_ <= INTERNAL_MESH_SLASH_SEGMENTS; ++index_)
        {
            const float rate_ = static_cast<float>(index_) /
                static_cast<float>(INTERNAL_MESH_SLASH_SEGMENTS);

            const float angleDegrees_ =
                startAngleDegrees_ + (endAngleDegrees_ - startAngleDegrees_) * rate_;

            const float angle_ = DirectX::XMConvertToRadians(angleDegrees_);
            const float sin_ = std::sin(angle_);
            const float cos_ = std::cos(angle_);

            // 中央を高く、両端は細くする。
            const float taper_ = std::sin(rate_ * DirectX::XM_PI);

            const float currentHeight_ = height_ * (0.18f + 0.82f * taper_);
            const float currentDepth_ = depth_ * (0.18f + 0.82f * taper_);

            const DirectX::XMFLOAT3 bottomPosition_ =
            {
                sin_ * radius_,
                -currentDepth_,
                cos_ * radius_
            };

            const DirectX::XMFLOAT3 topPosition_ =
            {
                sin_ * radius_,
                currentHeight_,
                cos_ * radius_
            };

            const DirectX::XMFLOAT3 normal_ =
            {
                sin_,
                0.0f,
                cos_
            };

            const DirectX::XMFLOAT4 tangent_ =
            {
                cos_,
                0.0f,
                -sin_,
                1.0f
            };

            mesh_.vertices.push_back(CreateInternalMeshSlashVertexFull(
                bottomPosition_,
                normal_,
                tangent_,
                rate_,
                1.0f));

            mesh_.vertices.push_back(CreateInternalMeshSlashVertexFull(
                topPosition_,
                normal_,
                tangent_,
                rate_,
                0.0f));
        }

        for (int index_ = 0; index_ < INTERNAL_MESH_SLASH_SEGMENTS; ++index_)
        {
            const uint32_t current_ = baseIndex_ + static_cast<uint32_t>(index_ * 2);
            const uint32_t next_ = current_ + 2;

            AddQuadIndicesDoubleSided(
                mesh_.indices,
                current_,
                current_ + 1,
                next_,
                next_ + 1);
        }
    }

    std::shared_ptr<ModelResource> CreateInternalMeshSlashVerticalCrescentModelResource(ID3D11Device* device_)
    {
        if (!device_) return nullptr;

        auto resource_ = CreateGeneratedModelResource(INTERNAL_MESH_SLASH_VERTICAL_CRESCENT_MODEL_PATH);
        auto& mesh_ = resource_->meshes[0];

        mesh_.vertices.reserve((INTERNAL_MESH_SLASH_SEGMENTS + 1) * 4);
        mesh_.indices.reserve(INTERNAL_MESH_SLASH_SEGMENTS * 18);

        std::vector<DirectX::XMFLOAT3> innerPoints_;
        std::vector<DirectX::XMFLOAT3> outerPoints_;

        BuildSharpCrescentPoints(
            innerPoints_,
            outerPoints_,
            -78.0f,
            78.0f,
            1.22f,
            0.56f,
            0.01f);

        AddRibbonStrip(mesh_, innerPoints_, outerPoints_);

        AddVerticalCrescentWall(
            mesh_,
            1.12f,
            1.0f,
            0.5f,
            -78.0f,
            78.0f);

        return CreateStaticMeshBuffers(device_, mesh_) ? resource_ : nullptr;
    }

    std::shared_ptr<ModelResource> CreateInternalMeshSlashRaisedCrescentModelResource(
        ID3D11Device* device_,
        const MeshVFXInternalMeshSettings& settings_)
    {
        if (!device_) return nullptr;

        auto resource_ = CreateGeneratedModelResource(INTERNAL_MESH_SLASH_RAISED_CRESCENT_MODEL_PATH);
        auto& mesh_ = resource_->meshes[0];

        constexpr int CROSS_SECTION_VERTEX_COUNT = 4;
        const int segmentCount_ = std::clamp(settings_.segments, 3, 512);

        mesh_.vertices.reserve(
            static_cast<size_t>(segmentCount_ + 1) *
            static_cast<size_t>(CROSS_SECTION_VERTEX_COUNT));

        // 3面分:
        // outer -> wallCenter
        // wallCenter -> wallTop
        // wallBottom -> wallCenter
        // double sided なので 1Quad = 12 indices
        mesh_.indices.reserve(
            static_cast<size_t>(segmentCount_) *
            3u *
            12u);

        for (int index_ = 0; index_ <= segmentCount_; ++index_)
        {
            const float rate_ = static_cast<float>(index_) /
                static_cast<float>(segmentCount_);

            const float angleDegrees_ = settings_.startAngleDegrees +
                (settings_.endAngleDegrees - settings_.startAngleDegrees) * rate_;
            const float angle_ = DirectX::XMConvertToRadians(angleDegrees_);
            const float sin_ = std::sin(angle_);
            const float cos_ = std::cos(angle_);

            // 両端を細く、中央を太く・高くする。
            const float taper_ = std::sin(rate_ * DirectX::XM_PI);
            const float sharpTaper_ = std::pow(
                (std::max)(taper_, 0.0f),
                (std::max)(settings_.taperPower, 0.01f));

            const float tipRate_ = std::clamp(settings_.tipRate, 0.0f, 1.0f);
            const float tipFade_ = tipRate_ + (1.0f - tipRate_) * sharpTaper_;

            const float baseRadius_ = (std::max)(settings_.baseRadius, 0.0001f) +
                settings_.radiusTaperAmount * taper_;
            const float width_ = (std::max)(settings_.tipWidth, 0.0f) +
                (std::max)(settings_.centerWidth, 0.0f) * sharpTaper_;

            const float innerRadius_ = baseRadius_ - width_ * settings_.innerWidthScale;
            const float outerRadius_ = baseRadius_ + width_ * settings_.outerWidthScale;

            // 垂直壁の位置。0.45～0.60くらいが使いやすい。
            const float wallPositionRate_ = std::clamp(settings_.wallPositionRate, 0.0f, 1.0f);

            const float wallRadius_ =
                innerRadius_ * (1.0f - wallPositionRate_) +
                outerRadius_ * wallPositionRate_;

            // 端では壁の上下も中央へ集める。
            const float tipGather_ = 1.0f - sharpTaper_;
            const float tipRadius_ =
                innerRadius_ * 0.5f +
                outerRadius_ * 0.5f;

            const float gatheredWallRadius_ =
                wallRadius_ * (1.0f - tipGather_) +
                tipRadius_ * tipGather_;

            const float wallHeight_ = (std::max)(settings_.wallHeight, 0.0f) * tipFade_;
            const float wallDepth_ = (std::max)(settings_.wallDepth, 0.0f) * tipFade_;

            const DirectX::XMFLOAT3 outerEdge_ =
            {
                sin_ * outerRadius_,
                0.0f,
                cos_ * outerRadius_
            };

            const DirectX::XMFLOAT3 wallCenter_ =
            {
                sin_ * gatheredWallRadius_,
                0.0f,
                cos_ * gatheredWallRadius_
            };

            const DirectX::XMFLOAT3 wallTop_ =
            {
                sin_ * gatheredWallRadius_,
                wallHeight_,
                cos_ * gatheredWallRadius_
            };

            const DirectX::XMFLOAT3 wallBottom_ =
            {
                sin_ * gatheredWallRadius_,
                -wallDepth_,
                cos_ * gatheredWallRadius_
            };

            // 円弧方向の接線。
            const DirectX::XMFLOAT4 arcTangent_ =
            {
                cos_,
                0.0f,
                -sin_,
                1.0f
            };

            // 床面用。MeshVFXなら上向きで十分。
            const DirectX::XMFLOAT3 floorNormal_ =
            {
                0.0f,
                1.0f,
                0.0f
            };

            // 壁面用。円の外側方向。
            const DirectX::XMFLOAT3 wallNormal_ =
            {
                sin_,
                0.0f,
                cos_
            };

            // UV:
            // U = 円弧方向
            // V = 断面方向
            // 0.0: 壁上端
            // 0.5: 中央
            // 1.0: 外側・下端
            const float u_ = rate_;

            mesh_.vertices.push_back(CreateInternalMeshSlashVertexFull(
                outerEdge_,
                floorNormal_,
                arcTangent_,
                u_,
                1.0f));

            mesh_.vertices.push_back(CreateInternalMeshSlashVertexFull(
                wallCenter_,
                floorNormal_,
                arcTangent_,
                u_,
                0.5f));

            mesh_.vertices.push_back(CreateInternalMeshSlashVertexFull(
                wallTop_,
                wallNormal_,
                arcTangent_,
                u_,
                0.0f));

            mesh_.vertices.push_back(CreateInternalMeshSlashVertexFull(
                wallBottom_,
                wallNormal_,
                arcTangent_,
                u_,
                1.0f));
        }

        for (int index_ = 0; index_ < segmentCount_; ++index_)
        {
            const uint32_t current_ =
                static_cast<uint32_t>(index_ * CROSS_SECTION_VERTEX_COUNT);
            const uint32_t next_ = current_ + CROSS_SECTION_VERTEX_COUNT;

            const uint32_t currentOuter_ = current_ + 0;
            const uint32_t currentWallCenter_ = current_ + 1;
            const uint32_t currentWallTop_ = current_ + 2;
            const uint32_t currentWallBottom_ = current_ + 3;

            const uint32_t nextOuter_ = next_ + 0;
            const uint32_t nextWallCenter_ = next_ + 1;
            const uint32_t nextWallTop_ = next_ + 2;
            const uint32_t nextWallBottom_ = next_ + 3;

            // 下の半月板部分。
            AddQuadIndicesDoubleSided(
                mesh_.indices,
                currentOuter_,
                currentWallCenter_,
                nextOuter_,
                nextWallCenter_);

            // 上に立ち上がる壁部分。
            AddQuadIndicesDoubleSided(
                mesh_.indices,
                currentWallCenter_,
                currentWallTop_,
                nextWallCenter_,
                nextWallTop_);

            // 下に貫通する壁部分。
            AddQuadIndicesDoubleSided(
                mesh_.indices,
                currentWallBottom_,
                currentWallCenter_,
                nextWallBottom_,
                nextWallCenter_);
        }

        return CreateStaticMeshBuffers(device_, mesh_) ? resource_ : nullptr;
    }

    std::shared_ptr<ModelResource> CreateInternalMeshSlashFanModelResource(ID3D11Device* device_)
    {
        return CreateInternalMeshSlashArcModelResource(
            device_,
            INTERNAL_MESH_SLASH_FAN_MODEL_PATH,
            -55.0f,
            55.0f,
            0.0f,
            1.55f);
    }

    std::shared_ptr<ModelResource> CreateInternalMeshSlashArcRibbonModelResource(ID3D11Device* device_)
    {
        return CreateInternalMeshSlashArcModelResource(
            device_,
            INTERNAL_MESH_SLASH_ARC_RIBBON_MODEL_PATH,
            -82.0f,
            82.0f,
            1.0f,
            1.3f);
    }

    std::shared_ptr<ModelResource> CreateInternalMeshSlashCrossModelResource(ID3D11Device* device_)
    {
        if (!device_) return nullptr;

        auto resource_ = CreateGeneratedModelResource(INTERNAL_MESH_SLASH_CROSS_MODEL_PATH);
        auto& mesh_ = resource_->meshes[0];
        mesh_.vertices.reserve(8);
        mesh_.indices.reserve(12);
        AddStraightRibbon(mesh_, 2.4f, 0.32f, 42.0f, { 0.0f, 0.0f, 0.0f });
        AddStraightRibbon(mesh_, 2.4f, 0.32f, -42.0f, { 0.0f, 0.0f, 0.0f });
        return CreateStaticMeshBuffers(device_, mesh_) ? resource_ : nullptr;
    }

    std::shared_ptr<ModelResource> CreateInternalMeshSlashGroundModelResource(ID3D11Device* device_)
    {
        if (!device_) return nullptr;

        auto resource_ = CreateGeneratedModelResource(INTERNAL_MESH_SLASH_GROUND_MODEL_PATH);
        auto& mesh_ = resource_->meshes[0];
        mesh_.vertices.reserve((INTERNAL_MESH_SLASH_SEGMENTS + 1) * 2);
        mesh_.indices.reserve(INTERNAL_MESH_SLASH_SEGMENTS * 6);

        std::vector<DirectX::XMFLOAT3> innerPoints_;
        std::vector<DirectX::XMFLOAT3> outerPoints_;
        innerPoints_.reserve(INTERNAL_MESH_SLASH_SEGMENTS + 1);
        outerPoints_.reserve(INTERNAL_MESH_SLASH_SEGMENTS + 1);

        for (int index_ = 0; index_ <= INTERNAL_MESH_SLASH_SEGMENTS; ++index_)
        {
            const float rate_ = static_cast<float>(index_) /
                static_cast<float>(INTERNAL_MESH_SLASH_SEGMENTS);
            const float x_ = -1.7f + 3.4f * rate_;
            const float centerZ_ = 0.12f * std::sin(rate_ * DirectX::XM_2PI);
            const float width_ = 0.08f + 0.28f * std::sin(rate_ * DirectX::XM_PI);
            innerPoints_.push_back({ x_, 0.0f, centerZ_ - width_ });
            outerPoints_.push_back({ x_, 0.0f, centerZ_ + width_ });
        }

        AddRibbonStrip(mesh_, innerPoints_, outerPoints_);
        return CreateStaticMeshBuffers(device_, mesh_) ? resource_ : nullptr;
    }

    std::shared_ptr<ModelResource> CreateInternalMeshCylinderModelResource(
        ID3D11Device* device_,
        float radius_)
    {
        if (!device_) return nullptr;
        const float safeRadius_ = (std::max)(radius_, 0.0001f);

        auto resource_ = std::make_shared<ModelResource>();
        resource_->modelName = INTERNAL_MESH_CYLINDER_MODEL_PATH;

        auto& node_ = resource_->nodes.emplace_back();
        node_.name = "InternalMeshCylinder";

        auto& material_ = resource_->materials.emplace_back();
        material_.id = 0;
        material_.name = "InternalMeshCylinder";

        auto& mesh_ = resource_->meshes.emplace_back();
        mesh_.nodeIndex = 0;
        mesh_.materialIndex = 0;
        mesh_.node = &resource_->nodes[0];
        mesh_.material = &resource_->materials[0];
        mesh_.vertices.reserve(static_cast<size_t>(INTERNAL_MESH_CYLINDER_SEGMENTS) * 2);
        mesh_.indices.reserve(static_cast<size_t>(INTERNAL_MESH_CYLINDER_SEGMENTS) * 6);

        for (int index_ = 0; index_ < INTERNAL_MESH_CYLINDER_SEGMENTS; ++index_)
        {
            const float angle_ = DirectX::XM_2PI * static_cast<float>(index_)
                / static_cast<float>(INTERNAL_MESH_CYLINDER_SEGMENTS);
            const float x_ = std::sin(angle_) * safeRadius_;
            const float z_ = std::cos(angle_) * safeRadius_;
            const float u_ = static_cast<float>(index_)
                / static_cast<float>(INTERNAL_MESH_CYLINDER_SEGMENTS);

            for (int heightIndex_ = 0; heightIndex_ < 2; ++heightIndex_)
            {
                const float y_ = heightIndex_ == 0 ? -1.0f : 1.0f;
                GltfModelCommponennt::Vertex vertex_{};
                vertex_.position = { x_, y_, z_ };
                vertex_.normal = { -x_, 0.0f, -z_ };
                vertex_.tangent = { z_, 0.0f, -x_, 1.0f };
                vertex_.texcoord = { u_, heightIndex_ == 0 ? 1.0f : 0.0f };
                mesh_.vertices.push_back(vertex_);
            }
        }

        for (int index_ = 0; index_ < INTERNAL_MESH_CYLINDER_SEGMENTS; ++index_)
        {
            const uint32_t bottomCurrent_ = static_cast<uint32_t>(index_ * 2);
            const uint32_t topCurrent_ = bottomCurrent_ + 1;
            const uint32_t bottomNext_ = static_cast<uint32_t>(
                ((index_ + 1) % INTERNAL_MESH_CYLINDER_SEGMENTS) * 2);
            const uint32_t topNext_ = bottomNext_ + 1;

            mesh_.indices.push_back(bottomCurrent_);
            mesh_.indices.push_back(topCurrent_);
            mesh_.indices.push_back(bottomNext_);
            mesh_.indices.push_back(bottomNext_);
            mesh_.indices.push_back(topCurrent_);
            mesh_.indices.push_back(topNext_);
        }

        return CreateStaticMeshBuffers(device_, mesh_) ? resource_ : nullptr;
    }

    std::shared_ptr<ModelResource> CreateInternalMeshConeModelResource(
        ID3D11Device* device_,
        float radius_,
        const MeshVFXInternalMeshSettings& settings_)
    {
        if (!device_) return nullptr;

        const float baseRadius_ = (std::max)(radius_, 0.0001f);
        const float tipRadius_ = baseRadius_ *
            std::clamp(settings_.coneTipRadiusRate, 0.0f, 1.0f);
        const float coneLength_ = (std::max)(settings_.coneLength, 0.0001f);
        const float radiusSlope_ = (baseRadius_ - tipRadius_) / coneLength_;
        const int segmentCount_ = std::clamp(settings_.segments, 3, 512);
        auto resource_ = CreateGeneratedModelResource(INTERNAL_MESH_CONE_MODEL_PATH);
        auto& mesh_ = resource_->meshes[0];
        mesh_.vertices.reserve(static_cast<size_t>(segmentCount_) * 2);
        mesh_.indices.reserve(static_cast<size_t>(segmentCount_) * 6);

        for (int index_ = 0; index_ < segmentCount_; ++index_)
        {
            const float rate_ = static_cast<float>(index_) /
                static_cast<float>(segmentCount_);
            const float angle_ = DirectX::XM_2PI * rate_;
            const float sin_ = std::sin(angle_);
            const float cos_ = std::cos(angle_);
            const DirectX::XMVECTOR normalVector_ = DirectX::XMVector3Normalize(
                DirectX::XMVectorSet(sin_, cos_, -radiusSlope_, 0.0f));
            DirectX::XMFLOAT3 normal_{};
            DirectX::XMStoreFloat3(&normal_, normalVector_);

            GltfModelCommponennt::Vertex tipVertex_{};
            tipVertex_.position = { sin_ * tipRadius_, cos_ * tipRadius_, 0.0f };
            tipVertex_.normal = normal_;
            tipVertex_.tangent = { cos_, -sin_, 0.0f, 1.0f };
            tipVertex_.texcoord = { rate_, 1.0f };
            mesh_.vertices.push_back(tipVertex_);

            GltfModelCommponennt::Vertex baseVertex_ = tipVertex_;
            baseVertex_.position = { sin_ * baseRadius_, cos_ * baseRadius_, coneLength_ };
            baseVertex_.texcoord = { rate_, 0.0f };
            mesh_.vertices.push_back(baseVertex_);
        }

        for (int index_ = 0; index_ < segmentCount_; ++index_)
        {
            const uint32_t tipCurrent_ = static_cast<uint32_t>(index_ * 2);
            const uint32_t baseCurrent_ = tipCurrent_ + 1;
            const uint32_t tipNext_ = static_cast<uint32_t>(
                ((index_ + 1) % segmentCount_) * 2);
            const uint32_t baseNext_ = tipNext_ + 1;
            mesh_.indices.insert(
                mesh_.indices.end(),
                { tipCurrent_, baseCurrent_, tipNext_, tipNext_, baseCurrent_, baseNext_ });
        }

        return CreateStaticMeshBuffers(device_, mesh_) ? resource_ : nullptr;
    }

    GltfModelCommponennt::Vertex CreateInternalMeshSlashVertexTransformed(
        const DirectX::XMFLOAT3& position_,
        float u_,
        float v_,
        const DirectX::XMMATRIX& transform_)
    {
        using namespace DirectX;

        const XMVECTOR positionVector_ = XMLoadFloat3(&position_);
        const XMVECTOR normalVector_ = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        const XMVECTOR tangentVector_ = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);

        XMFLOAT3 transformedPosition_{};
        XMFLOAT3 transformedNormal_{};
        XMFLOAT3 transformedTangent_{};

        XMStoreFloat3(
            &transformedPosition_,
            XMVector3TransformCoord(positionVector_, transform_));

        XMStoreFloat3(
            &transformedNormal_,
            XMVector3Normalize(XMVector3TransformNormal(normalVector_, transform_)));

        XMStoreFloat3(
            &transformedTangent_,
            XMVector3Normalize(XMVector3TransformNormal(tangentVector_, transform_)));

        GltfModelCommponennt::Vertex vertex_{};
        vertex_.position = transformedPosition_;
        vertex_.normal = transformedNormal_;
        vertex_.tangent = { transformedTangent_.x, transformedTangent_.y, transformedTangent_.z, 1.0f };
        vertex_.texcoord = { u_, v_ };
        return vertex_;
    }

    void AddCrossCrescentRibbon(
        GltfModelCommponennt::Mesh& mesh_,
        const DirectX::XMMATRIX& transform_)
    {
        std::vector<DirectX::XMFLOAT3> innerPoints_;
        std::vector<DirectX::XMFLOAT3> outerPoints_;

        BuildSharpCrescentPoints(
            innerPoints_,
            outerPoints_,
            -78.0f,
            78.0f,
            1.22f,
            0.56f,
            0.01f);

        const size_t pointCount_ = (std::min)(innerPoints_.size(), outerPoints_.size());
        if (pointCount_ < 2) return;

        const uint32_t baseIndex_ = static_cast<uint32_t>(mesh_.vertices.size());

        for (size_t index_ = 0; index_ < pointCount_; ++index_)
        {
            const float u_ = static_cast<float>(index_) /
                static_cast<float>(pointCount_ - 1);

            mesh_.vertices.push_back(CreateInternalMeshSlashVertexTransformed(
                innerPoints_[index_],
                u_,
                1.0f,
                transform_));

            mesh_.vertices.push_back(CreateInternalMeshSlashVertexTransformed(
                outerPoints_[index_],
                u_,
                0.0f,
                transform_));
        }

        for (size_t index_ = 0; index_ + 1 < pointCount_; ++index_)
        {
            const uint32_t current_ = baseIndex_ + static_cast<uint32_t>(index_ * 2);
            const uint32_t next_ = current_ + 2;

            AddQuadIndicesDoubleSided(
                mesh_.indices,
                current_,
                current_ + 1,
                next_,
                next_ + 1);
        }
    }

    std::shared_ptr<ModelResource> CreateInternalMeshSlashCrossCrescentModelResource(ID3D11Device* device_)
    {
        if (!device_) return nullptr;

        auto resource_ = CreateGeneratedModelResource(INTERNAL_MESH_SLASH_CROSS_CRESCENT_MODEL_PATH);
        auto& mesh_ = resource_->meshes[0];

        mesh_.vertices.reserve((INTERNAL_MESH_SLASH_SEGMENTS + 1) * 4);
        mesh_.indices.reserve(INTERNAL_MESH_SLASH_SEGMENTS * 24);


        const DirectX::XMMATRIX verticalMatrix_ =
            DirectX::XMMatrixRotationZ(DirectX::XM_PIDIV2);

        AddCrossCrescentRibbon(mesh_, verticalMatrix_);

        return CreateStaticMeshBuffers(device_, mesh_) ? resource_ : nullptr;
    }


    bool HasExtension(const std::filesystem::path& path_, const std::vector<std::string>& extensions_)
    {
        std::string extension_ = path_.extension().string();
        std::transform(extension_.begin(), extension_.end(), extension_.begin(),
            [](unsigned char character_) { return static_cast<char>(std::tolower(character_)); });
        return std::find(extensions_.begin(), extensions_.end(), extension_) != extensions_.end();
    }

    std::string GetAssetLabel(const std::string& path_)
    {
        const std::filesystem::path assetPath_ = WindowsDialogHelper::Utf8ToPath(path_);
        return WindowsDialogHelper::PathToUtf8(assetPath_.filename()) + "  [" + path_ + "]";
    }

    template<typename T>
    void ReadFloatArray(const nlohmann::json& json_, const char* key_, T& value_, size_t count_)
    {
        if (!json_.contains(key_) || !json_[key_].is_array()) return;
        float* values_ = reinterpret_cast<float*>(&value_);
        const auto& array_ = json_[key_];
        for (size_t index_ = 0; index_ < count_ && index_ < array_.size(); ++index_)
        {
            values_[index_] = array_[index_].get<float>();
        }
    }

    bool DrawCurveTrackCombo(
        const char* label_,
        std::string& curveName_,
        const std::vector<std::string>& curveNames_)
    {
        bool changed_ = ImGui::InputText(label_, &curveName_);
        if (curveNames_.empty()) return changed_;

        ImGui::SameLine();
        const std::string comboLabel_ = std::string("##") + label_ + "CurveSelect";
        const char* preview_ = curveName_.empty() ? "None" : curveName_.c_str();
        if (ImGui::BeginCombo(comboLabel_.c_str(), preview_))
        {
            if (ImGui::Selectable("None", curveName_.empty()))
            {
                curveName_.clear();
                changed_ = true;
            }
            for (const std::string& name_ : curveNames_)
            {
                if (ImGui::Selectable(name_.c_str(), curveName_ == name_))
                {
                    curveName_ = name_;
                    changed_ = true;
                }
            }
            ImGui::EndCombo();
        }
        return changed_;
    }


}

void MeshVFXComponent::Start()
{
    // Assetに書かれたTexture/Modelを実行用RuntimePartへ読み込む。
    RefreshAssetLists();
    LoadPartResources();
}

void MeshVFXComponent::Update(float deltaTime_)
{
    // 再生中だけComponent全体の時間を進める。Part個別の時間はRenderSystem側で計算する。
    if (isPlaying) time += deltaTime_ * playbackSpeed;
}


std::vector<std::string> MeshVFXComponent::GetCurveTrackNames() const
{
    const std::shared_ptr<GameObject> owner_ =
        const_cast<MeshVFXComponent*>(this)->GetGameObject();
    if (!owner_) return {};

    const std::shared_ptr<CurveComponent> curveComponent_ = owner_->GetComponent<CurveComponent>();
    return curveComponent_ ? curveComponent_->GetTrackNames() : std::vector<std::string>{};
}

float MeshVFXComponent::EvaluateCurveValue(
    const std::string& curveName_,
    float normalizedTime_,
    float fallbackValue_) const
{
    if (curveName_.empty()) return fallbackValue_;

    const std::shared_ptr<GameObject> owner_ =
        const_cast<MeshVFXComponent*>(this)->GetGameObject();
    if (!owner_) return fallbackValue_;

    const std::shared_ptr<CurveComponent> curveComponent_ = owner_->GetComponent<CurveComponent>();
    if (!curveComponent_) return fallbackValue_;

    if (!curveComponent_->HasTrack(curveName_))
    {
        return fallbackValue_;
    }

    const float clampedTime_ = std::clamp(normalizedTime_, 0.0f, 1.0f);
    const float value_ = curveComponent_->GetValue(curveName_, clampedTime_);
    return std::isfinite(value_) ? value_ : fallbackValue_;
}

void MeshVFXComponent::Gui()
{
    // MeshVFX Asset、Part、Texture、UV、Noise、Edgeなどの編集UIをまとめる。
    ImGui::SeparatorText("Mesh VFX");
    if (ImGui::Button(isPlaying ? "Stop" : "Play")) isPlaying ? Stop() : Play();
    ImGui::SameLine();
    if (ImGui::Button("Reset")) Reset();
    ImGui::SameLine();
    if (ImGui::Button("Refresh Assets")) RefreshAssetLists();
    ImGui::Text("Time: %.2f", time);
    ImGui::DragFloat("Playback Speed", &playbackSpeed, 0.01f, 0.0f, 10.0f);
    const std::vector<std::string> curveNames_ = GetCurveTrackNames();

    std::string selectedAssetPath_ = assetPath;
    if (AssetSlotGui::Draw(
        "Mesh VFX JSON Asset",
        AssetSlotType::JSON,
        selectedAssetPath_,
        nullptr,
        "Data/VFX"))
    {
        if (selectedAssetPath_.empty()) assetPath.clear();
        else if (LoadAsset(selectedAssetPath_)) assetPath = selectedAssetPath_;
    }
    if (ImGui::Button("Save Mesh VFX JSON..."))
    {
        const std::string initialPath_ = assetPath.empty()
            ? "Data/VFX/MeshVFX.json"
            : assetPath;
        const std::string selectedPath_ = WindowsDialogHelper::SaveJsonDialog(
            L"Save Mesh VFX",
            initialPath_,
            GraphicsDeviceManager::Instance().GetWindowHandle());
        if (!selectedPath_.empty() && SaveAsset(selectedPath_)) assetPath = selectedPath_;
    }

    if (ImGui::Button("Add Part"))
    {
        asset.parts.emplace_back();
        runtimeParts.emplace_back();
    }

    for (int partIndex_ = 0; partIndex_ < static_cast<int>(asset.parts.size()); ++partIndex_)
    {
        ImGui::PushID(partIndex_);
        MeshVFXPart& part_ = asset.parts[partIndex_];
        bool removePart_ = false;

        if (ImGui::TreeNode("Part"))
        {
            if (AssetSlotGui::Draw(
                "Part JSON Asset",
                AssetSlotType::JSON,
                part_.partAssetPath,
                nullptr,
                "Data/VFX/Parts"))
            {
                if (!part_.partAssetPath.empty())
                {
                    LoadPartAsset(static_cast<size_t>(partIndex_), part_.partAssetPath);
                }
            }
            if (ImGui::Button("Save Part JSON..."))
            {
                const std::string initialPath_ = part_.partAssetPath.empty()
                    ? "Data/VFX/Parts/MeshVFXPart.json"
                    : part_.partAssetPath;
                const std::string selectedPath_ = WindowsDialogHelper::SaveJsonDialog(
                    L"Save Mesh VFX Part",
                    initialPath_,
                    GraphicsDeviceManager::Instance().GetWindowHandle());
                if (!selectedPath_.empty() &&
                    SavePartAsset(static_cast<size_t>(partIndex_), selectedPath_))
                {
                    part_.partAssetPath = selectedPath_;
                }
            }

            ImGui::Checkbox("Render Part", &part_.renderEnabled);
            if (!part_.renderEnabled)
            {
                ImGui::SameLine();
                ImGui::TextDisabled("Model information only");
            }

            ImGui::Spacing();
            const MeshVFXRuntimePart* runtimePart_ =
                partIndex_ < static_cast<int>(runtimeParts.size()) ? &runtimeParts[partIndex_] : nullptr;

            if (ImGui::BeginTabBar("MeshVFXPartTabs"))
            {
                if (ImGui::BeginTabItem("Model"))
                {
            const bool modelChanged_ = AssetSlotGui::Draw(
                "Model Asset", AssetSlotType::MODEL, part_.modelPath, nullptr, "Data/Model");

            if (modelChanged_)
            {
                LoadPartResource(static_cast<size_t>(partIndex_));
                const MeshVFXRuntimePart& loadedPart_ = runtimeParts[partIndex_];
                part_.meshIndex = loadedPart_.modelResource && !loadedPart_.modelResource->meshes.empty()
                    ? 0
                    : -1;
            }
            if (ImGui::Button("Use Internal Mesh"))
            {
                part_.modelPath = INTERNAL_MESH_CYLINDER_MODEL_PATH;
                part_.meshIndex = 0;
                part_.shapeCylinderRadius = (std::max)(part_.shapeCylinderRadius, 0.0001f);
                LoadPartResource(static_cast<size_t>(partIndex_));
            }
            ImGui::SameLine();
            if (ImGui::Button("Use Internal Cone"))
            {
                part_.modelPath = INTERNAL_MESH_CONE_MODEL_PATH;
                part_.meshIndex = 0;
                part_.shapeCylinderRadius = (std::max)(part_.shapeCylinderRadius, 0.0001f);
                LoadPartResource(static_cast<size_t>(partIndex_));
            }

            const char* slashPresetPreview_ = "Select Slash Preset";
            if (part_.modelPath == INTERNAL_MESH_SLASH_CRESCENT_MODEL_PATH) slashPresetPreview_ = "Crescent";
            else if (part_.modelPath == INTERNAL_MESH_SLASH_CROSS_CRESCENT_MODEL_PATH) slashPresetPreview_ = "Cross Crescent";
            else if (part_.modelPath == INTERNAL_MESH_SLASH_VERTICAL_CRESCENT_MODEL_PATH) slashPresetPreview_ = "Vertical Crescent";
            else if (part_.modelPath == INTERNAL_MESH_SLASH_RAISED_CRESCENT_MODEL_PATH) slashPresetPreview_ = "Raised Crescent";
            else if (part_.modelPath == INTERNAL_MESH_SLASH_FAN_MODEL_PATH) slashPresetPreview_ = "Fan";
            else if (part_.modelPath == INTERNAL_MESH_SLASH_ARC_RIBBON_MODEL_PATH) slashPresetPreview_ = "Arc Ribbon";
            else if (part_.modelPath == INTERNAL_MESH_SLASH_CROSS_MODEL_PATH) slashPresetPreview_ = "Cross Slash";
            else if (part_.modelPath == INTERNAL_MESH_SLASH_GROUND_MODEL_PATH) slashPresetPreview_ = "Ground Slash";

            if (ImGui::BeginCombo("Internal Slash Mesh", slashPresetPreview_))
            {
                struct SlashPresetEntry
                {
                    const char* label;
                    const char* modelPath;
                };

                const SlashPresetEntry presets_[] =
                {
                    { "Crescent", INTERNAL_MESH_SLASH_CRESCENT_MODEL_PATH },
                    { "Cross Crescent", INTERNAL_MESH_SLASH_CROSS_CRESCENT_MODEL_PATH },
                    { "Vertical Crescent", INTERNAL_MESH_SLASH_VERTICAL_CRESCENT_MODEL_PATH },
                    { "Raised Crescent", INTERNAL_MESH_SLASH_RAISED_CRESCENT_MODEL_PATH },
                    { "Fan", INTERNAL_MESH_SLASH_FAN_MODEL_PATH },
                    { "Arc Ribbon", INTERNAL_MESH_SLASH_ARC_RIBBON_MODEL_PATH },
                    { "Cross Slash", INTERNAL_MESH_SLASH_CROSS_MODEL_PATH },
                    { "Ground Slash", INTERNAL_MESH_SLASH_GROUND_MODEL_PATH },
                };

                for (const SlashPresetEntry& preset_ : presets_)
                {
                    const bool selected_ = part_.modelPath == preset_.modelPath;
                    if (ImGui::Selectable(preset_.label, selected_))
                    {
                        part_.modelPath = preset_.modelPath;
                        part_.meshIndex = 0;
                        LoadPartResource(static_cast<size_t>(partIndex_));
                    }

                    if (selected_)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }

                ImGui::EndCombo();
            }
            const std::shared_ptr<ModelResource> modelResource_ =
                runtimePart_ ? runtimePart_->modelResource : nullptr;

            if (modelResource_)
            {
                ImGui::Text(
                    "Loaded: %s | Meshes: %zu | Nodes: %zu | Materials: %zu",
                    modelResource_->modelName.c_str(),
                    modelResource_->meshes.size(),
                    modelResource_->nodes.size(),
                    modelResource_->materials.size());

                std::string meshPreview_ = "All Meshes";
                if (part_.meshIndex >= 0 &&
                    part_.meshIndex < static_cast<int>(modelResource_->meshes.size()))
                {
                    const auto& mesh_ = modelResource_->meshes[part_.meshIndex];
                    const std::string nodeName_ = mesh_.node ? mesh_.node->name : "No Node";
                    const std::string materialName_ = mesh_.material ? mesh_.material->name : "No Material";
                    meshPreview_ = std::to_string(part_.meshIndex) + ": " + nodeName_ + " / " + materialName_;
                }

                if (ImGui::BeginCombo("Mesh", meshPreview_.c_str()))
                {
                    if (ImGui::Selectable("All Meshes", part_.meshIndex == -1)) part_.meshIndex = -1;
                    for (int meshIndex_ = 0; meshIndex_ < static_cast<int>(modelResource_->meshes.size()); ++meshIndex_)
                    {
                        const auto& mesh_ = modelResource_->meshes[meshIndex_];
                        const std::string nodeName_ = mesh_.node ? mesh_.node->name : "No Node";
                        const std::string materialName_ = mesh_.material ? mesh_.material->name : "No Material";
                        const std::string meshLabel_ =
                            std::to_string(meshIndex_) + ": " + nodeName_ + " / " + materialName_;
                        if (ImGui::Selectable(meshLabel_.c_str(), part_.meshIndex == meshIndex_))
                        {
                            part_.meshIndex = meshIndex_;
                        }
                    }
                    ImGui::EndCombo();
                }

                if (part_.meshIndex >= 0 &&
                    part_.meshIndex < static_cast<int>(modelResource_->meshes.size()))
                {
                    const auto& selectedMesh_ = modelResource_->meshes[part_.meshIndex];
                    ImGui::Text(
                        "Vertices: %zu | Indices: %zu | Bones: %zu",
                        selectedMesh_.vertices.size(),
                        selectedMesh_.indices.size(),
                        selectedMesh_.bones.size());
                }
            }
            else
            {
                ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.2f, 1.0f), "Model resource is not loaded.");
            }

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Textures"))
                {

            ImGui::SeparatorText("Textures");
            // Base texture selection
            bool baseTextureChanged_ = AssetSlotGui::Draw(
                "Base Texture Asset",
                AssetSlotType::TEXTURE,
                part_.baseTexturePath,
                runtimePart_ ? runtimePart_->baseTexture.Get() : nullptr,
                "Data/Texture");

            ImGui::Checkbox("Use Material Base Texture", &part_.useMaterialBaseTexture);

            // Normal texture selection
            bool normalTextureChanged_ = AssetSlotGui::Draw(
                "Normal Texture Asset",
                AssetSlotType::TEXTURE,
                part_.normalTexturePath,
                runtimePart_ ? runtimePart_->normalTexture.Get() : nullptr,
                "Data/Texture");

            ImGui::Checkbox("Use Material Normal Texture", &part_.useMaterialNormalTexture);

            bool layeredTextureChanged_ = false;
            if (ImGui::TreeNode("Mask Texture Layers"))
            {
                if (ImGui::Button("Add Mask Layer"))
                {
                    part_.maskLayers.emplace_back();
                    layeredTextureChanged_ = true;
                }
                int removeLayerIndex_ = -1;
                for (int layerIndex_ = 0; layerIndex_ < static_cast<int>(part_.maskLayers.size()); ++layerIndex_)
                {
                    ImGui::PushID(layerIndex_);
                    MeshVFXMaskLayer& layer_ = part_.maskLayers[layerIndex_];
                    ID3D11ShaderResourceView* previewTexture_ = runtimePart_ &&
                        layerIndex_ < static_cast<int>(runtimePart_->maskTextures.size())
                        ? runtimePart_->maskTextures[layerIndex_].Get()
                        : nullptr;
                    layeredTextureChanged_ |= AssetSlotGui::Draw(
                        "Mask Texture Asset", AssetSlotType::TEXTURE, layer_.path, previewTexture_, "Data/Texture");
                    const char* blendItems_[] = { "Multiply / Narrow", "Union / Expand" };
                    int blendMode_ = static_cast<int>(layer_.blendMode);
                    if (ImGui::Combo("Blend", &blendMode_, blendItems_, IM_ARRAYSIZE(blendItems_)))
                    {
                        layer_.blendMode = static_cast<MeshVFXMaskBlendMode>(blendMode_);
                    }
                    ImGui::Checkbox("Invert", &layer_.invert);
                    if (ImGui::Button("Remove Mask Layer")) removeLayerIndex_ = layerIndex_;
                    ImGui::Separator();
                    ImGui::PopID();
                }
                if (removeLayerIndex_ >= 0)
                {
                    part_.maskLayers.erase(part_.maskLayers.begin() + removeLayerIndex_);
                    layeredTextureChanged_ = true;
                }
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Noise Texture Layers"))
            {
                if (ImGui::Button("Add Noise Layer"))
                {
                    part_.noiseTexturePaths.emplace_back();
                    layeredTextureChanged_ = true;
                }
                int removeLayerIndex_ = -1;
                for (int layerIndex_ = 0; layerIndex_ < static_cast<int>(part_.noiseTexturePaths.size()); ++layerIndex_)
                {
                    ImGui::PushID(layerIndex_);
                    std::string& path_ = part_.noiseTexturePaths[layerIndex_];
                    ID3D11ShaderResourceView* previewTexture_ = runtimePart_ &&
                        layerIndex_ < static_cast<int>(runtimePart_->noiseTextures.size())
                        ? runtimePart_->noiseTextures[layerIndex_].Get()
                        : nullptr;
                    layeredTextureChanged_ |= AssetSlotGui::Draw(
                        "Noise Texture Asset", AssetSlotType::TEXTURE, path_, previewTexture_, "Data/Texture");
                    if (ImGui::Button("Remove Noise Layer")) removeLayerIndex_ = layerIndex_;
                    ImGui::Separator();
                    ImGui::PopID();
                }
                if (removeLayerIndex_ >= 0)
                {
                    part_.noiseTexturePaths.erase(part_.noiseTexturePaths.begin() + removeLayerIndex_);
                    layeredTextureChanged_ = true;
                }
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Distortion Noise Texture"))
            {
                layeredTextureChanged_ |= AssetSlotGui::Draw(
                    "Distortion Noise Texture Asset",
                    AssetSlotType::TEXTURE,
                    part_.distortionNoiseTexturePath,
                    runtimePart_ ? runtimePart_->distortionNoiseTexture.Get() : nullptr,
                    "Data/Texture");
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Vertex Deformation Noise Texture"))
            {
                layeredTextureChanged_ |= AssetSlotGui::Draw(
                    "Vertex Noise Texture Asset",
                    AssetSlotType::TEXTURE,
                    part_.vertexNoiseTexturePath,
                    runtimePart_ ? runtimePart_->vertexNoiseTexture.Get() : nullptr,
                    "Data/Texture");
                ImGui::TextDisabled("Used only for vertex displacement. Empty uses procedural noise.");
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Edge Pattern Texture"))
            {
                layeredTextureChanged_ |= AssetSlotGui::Draw(
                    "Edge Pattern Texture Asset",
                    AssetSlotType::TEXTURE,
                    part_.edgePatternTexturePath,
                    runtimePart_ ? runtimePart_->edgePatternTexture.Get() : nullptr,
                    "Data/Texture");
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Emissive Texture"))
            {
                layeredTextureChanged_ |= AssetSlotGui::Draw(
                    "Emissive Texture Asset",
                    AssetSlotType::TEXTURE,
                    part_.emissiveTexturePath,
                    runtimePart_ ? runtimePart_->emissiveTexture.Get() : nullptr,
                    "Data/Texture");
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Gradient Texture"))
            {
                layeredTextureChanged_ |= AssetSlotGui::Draw(
                    "Gradient Texture Asset",
                    AssetSlotType::TEXTURE,
                    part_.gradientTexturePath,
                    runtimePart_ ? runtimePart_->gradientTexture.Get() : nullptr,
                    "Data/Texture");
                ImGui::Checkbox("Use Gradient Texture", &part_.useGradientTexture);
                if (part_.useGradientTexture)
                {
                    ImGui::TextDisabled("Gradient enabled: Base RGB is ignored. Base Alpha is still used.");
                    const char* sampleModeNames_[] = { "Horizontal", "Vertical" };
                    int sampleMode_ = static_cast<int>(part_.gradientSampleMode);
                    if (ImGui::Combo(
                        "Gradient Sample Mode",
                        &sampleMode_,
                        sampleModeNames_,
                        IM_ARRAYSIZE(sampleModeNames_)))
                    {
                        part_.gradientSampleMode =
                            static_cast<MESH_VFX_GRADIENT_SAMPLE_MODE>(sampleMode_);
                    }

                    const char* directionNames_[] = { "Forward", "Reverse" };
                    int direction_ = static_cast<int>(part_.gradientDirection);
                    if (ImGui::Combo(
                        "Gradient Direction",
                        &direction_,
                        directionNames_,
                        IM_ARRAYSIZE(directionNames_)))
                    {
                        part_.gradientDirection =
                            static_cast<MESH_VFX_GRADIENT_DIRECTION>(direction_);
                    }
                    ImGui::SliderFloat("Gradient Fixed UV", &part_.gradientFixedUv, 0.0f, 1.0f);
                    ImGui::SliderFloat("Gradient Start UV", &part_.gradientStartUv, 0.0f, 1.0f);
                    ImGui::SliderFloat("Gradient End UV", &part_.gradientEndUv, 0.0f, 1.0f);
                }
                ImGui::TreePop();
            }

            if (baseTextureChanged_ || normalTextureChanged_ || layeredTextureChanged_)
            {
                LoadPartResource(static_cast<size_t>(partIndex_));
            }

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Shape"))
                {

            ImGui::DragFloat3("Position", &part_.position.x, 0.01f);
            ImGui::DragFloat3("Rotation", &part_.rotation.x, 0.5f);
            ImGui::DragFloat3("Scale", &part_.scale.x, 0.01f);
            if (part_.modelPath == INTERNAL_MESH_CYLINDER_MODEL_PATH ||
                part_.modelPath == INTERNAL_MESH_CONE_MODEL_PATH)
            {
                const char* radiusLabel_ = part_.modelPath == INTERNAL_MESH_CONE_MODEL_PATH
                    ? "Internal Cone Base Radius"
                    : "Internal Cylinder Radius";
                if (ImGui::DragFloat(radiusLabel_, &part_.shapeCylinderRadius, 0.01f, 0.0001f, 100.0f))
                {
                    LoadPartResource(static_cast<size_t>(partIndex_));
                }
            }

            const bool isEditableInternalMesh_ =
                part_.modelPath == INTERNAL_MESH_CONE_MODEL_PATH ||
                part_.modelPath == INTERNAL_MESH_SLASH_RAISED_CRESCENT_MODEL_PATH;
            if (isEditableInternalMesh_ && ImGui::TreeNode("Internal Mesh Shape"))
            {
                MeshVFXInternalMeshSettings& settings_ = part_.internalMeshSettings;
                bool shapeChanged_ = false;
                shapeChanged_ |= ImGui::SliderInt("Segments", &settings_.segments, 3, 256);

                if (part_.modelPath == INTERNAL_MESH_CONE_MODEL_PATH)
                {
                    shapeChanged_ |= ImGui::DragFloat(
                        "Tip Radius Rate",
                        &settings_.coneTipRadiusRate,
                        0.005f,
                        0.0f,
                        1.0f);
                    shapeChanged_ |= ImGui::DragFloat(
                        "Length",
                        &settings_.coneLength,
                        0.01f,
                        0.001f,
                        100.0f);
                }
                else
                {
                    shapeChanged_ |= ImGui::DragFloat(
                        "Start Angle",
                        &settings_.startAngleDegrees,
                        0.5f,
                        -360.0f,
                        360.0f);
                    shapeChanged_ |= ImGui::DragFloat(
                        "End Angle",
                        &settings_.endAngleDegrees,
                        0.5f,
                        -360.0f,
                        360.0f);
                    shapeChanged_ |= ImGui::DragFloat(
                        "Taper Power",
                        &settings_.taperPower,
                        0.05f,
                        0.01f,
                        16.0f);
                    shapeChanged_ |= ImGui::DragFloat(
                        "Tip Fade Rate",
                        &settings_.tipRate,
                        0.005f,
                        0.0f,
                        1.0f);
                    shapeChanged_ |= ImGui::DragFloat(
                        "Base Radius",
                        &settings_.baseRadius,
                        0.01f,
                        0.001f,
                        100.0f);
                    shapeChanged_ |= ImGui::DragFloat(
                        "Radius Bulge",
                        &settings_.radiusTaperAmount,
                        0.005f,
                        -100.0f,
                        100.0f);
                    shapeChanged_ |= ImGui::DragFloat(
                        "Tip Width",
                        &settings_.tipWidth,
                        0.005f,
                        0.0f,
                        100.0f);
                    shapeChanged_ |= ImGui::DragFloat(
                        "Center Width",
                        &settings_.centerWidth,
                        0.01f,
                        0.0f,
                        100.0f);
                    shapeChanged_ |= ImGui::DragFloat(
                        "Inner Width Scale",
                        &settings_.innerWidthScale,
                        0.01f,
                        -10.0f,
                        10.0f);
                    shapeChanged_ |= ImGui::DragFloat(
                        "Outer Width Scale",
                        &settings_.outerWidthScale,
                        0.01f,
                        -10.0f,
                        10.0f);
                    shapeChanged_ |= ImGui::SliderFloat(
                        "Wall Position",
                        &settings_.wallPositionRate,
                        0.0f,
                        1.0f);
                    shapeChanged_ |= ImGui::DragFloat(
                        "Wall Height",
                        &settings_.wallHeight,
                        0.01f,
                        0.0f,
                        100.0f);
                    shapeChanged_ |= ImGui::DragFloat(
                        "Wall Depth",
                        &settings_.wallDepth,
                        0.01f,
                        0.0f,
                        100.0f);
                }

                if (ImGui::Button("Reset Shape Defaults"))
                {
                    settings_ = MeshVFXInternalMeshSettings{};
                    shapeChanged_ = true;
                }
                if (shapeChanged_)
                {
                    LoadPartResource(static_cast<size_t>(partIndex_));
                }
                ImGui::TreePop();
            }

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Animation"))
                {

            if (ImGui::TreeNode("Animation"))
            {
                ImGui::DragFloat("Duration", &part_.duration, 0.05f, 0.0f, 600.0f);
                ImGui::Checkbox("Loop", &part_.loop);
                ImGui::DragFloat3("Mesh Rotation Speed", &part_.meshRotationSpeed.x, 1.0f);

                ImGui::Checkbox("Animate Scale", &part_.animateScale);
                if (part_.animateScale)
                {
                    ImGui::DragFloat3("Scale Start", &part_.scaleStart.x, 0.01f);
                    ImGui::DragFloat3("Scale End", &part_.scaleEnd.x, 0.01f);
                }

                ImGui::Checkbox("Animate Alpha", &part_.animateAlpha);
                if (part_.animateAlpha)
                {
                    ImGui::DragFloat("Alpha Start", &part_.alphaStart, 0.01f, 0.0f, 1.0f);
                    ImGui::DragFloat("Alpha End", &part_.alphaEnd, 0.01f, 0.0f, 1.0f);
                }

                ImGui::DragFloat2("UV Rotation Center", &part_.uvRotationCenter.x, 0.01f);
                ImGui::SeparatorText("Initial UV Rotation (Degrees)");
                ImGui::DragFloat("Base UV Rotation", &part_.baseUVRotation, 1.0f, -360.0f, 360.0f);
                ImGui::DragFloat("Mask UV Rotation", &part_.maskUVRotation, 1.0f, -360.0f, 360.0f);
                ImGui::DragFloat("Noise UV Rotation", &part_.noiseUVRotation, 1.0f, -360.0f, 360.0f);
                ImGui::SeparatorText("UV Rotation Speed (Degrees / Second)");
                ImGui::DragFloat("Base UV Rotation Speed", &part_.baseUVRotationSpeed, 1.0f);
                ImGui::DragFloat("Mask UV Rotation Speed", &part_.maskUVRotationSpeed, 1.0f);
                ImGui::DragFloat("Noise UV Rotation Speed", &part_.noiseUVRotationSpeed, 1.0f);
                ImGui::SeparatorText("Radial UV Animation");
                ImGui::TextDisabled("Positive speed expands the Base / Mask texture from the UV center.");
                ImGui::DragFloat("UV Radial Scale", &part_.uvRadialScale, 0.01f, 0.001f, 100.0f);
                ImGui::DragFloat("UV Radial Scale Speed", &part_.uvRadialScaleSpeed, 0.01f, -10.0f, 10.0f);
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Time Curves"))
            {
                ImGui::TextDisabled("Uses CurveComponent tracks on this GameObject.");
                ImGui::TextDisabled("X is normalized part time. Duration 0 keeps curve time at 0.");
                ImGui::TextDisabled("UV curves multiply speed. They are not absolute UV offsets.");
                if (curveNames_.empty())
                {
                    ImGui::TextDisabled("No CurveComponent tracks found. Typed names will work after adding matching tracks.");
                }
                DrawCurveTrackCombo("Alpha Curve", part_.alphaCurveName, curveNames_);
                DrawCurveTrackCombo("Scale Curve", part_.scaleCurveName, curveNames_);
                DrawCurveTrackCombo("Dissolve Curve", part_.dissolveCurveName, curveNames_);
                DrawCurveTrackCombo("Emissive Curve", part_.emissiveCurveName, curveNames_);
                DrawCurveTrackCombo("UV Scroll Speed Curve", part_.uvScrollCurveName, curveNames_);
                DrawCurveTrackCombo("UV Rotation Speed Curve", part_.uvRotationCurveName, curveNames_);
                ImGui::TreePop();
            }

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Material"))
                {

            ImGui::ColorEdit4("Color", &part_.color.x);
            ImGui::DragFloat("Emissive Strength", &part_.emissiveStrength, 0.05f, 0.0f, 50.0f);
            ImGui::DragFloat("Emissive Pulse Speed", &part_.emissivePulseSpeed, 0.05f, 0.0f, 30.0f);
            ImGui::DragFloat("Emissive Pulse Amount", &part_.emissivePulseAmount, 0.01f, 0.0f, 2.0f);
            ImGui::DragFloat2("Base UV Speed", &part_.baseUVSpeed.x, 0.01f);
            ImGui::DragFloat2("Mask UV Speed", &part_.maskUVSpeed.x, 0.01f);
            ImGui::DragFloat2("Noise UV Speed", &part_.noiseUVSpeed.x, 0.01f);
            const char* samplerModes_[] = { "Wrap", "Clamp", "Border" };
            int baseSamplerMode_ = static_cast<int>(part_.baseSamplerMode);
            int normalSamplerMode_ = static_cast<int>(part_.normalSamplerMode);
            int maskSamplerMode_ = static_cast<int>(part_.maskSamplerMode);
            int noiseSamplerMode_ = static_cast<int>(part_.noiseSamplerMode);
            int distortionSamplerMode_ = static_cast<int>(part_.distortionSamplerMode);
            int edgePatternSamplerMode_ = static_cast<int>(part_.edgePatternSamplerMode);
            int emissiveSamplerMode_ = static_cast<int>(part_.emissiveSamplerMode);
            int gradientSamplerMode_ = static_cast<int>(part_.gradientSamplerMode);
            if (ImGui::Combo("Base Sampler", &baseSamplerMode_, samplerModes_, IM_ARRAYSIZE(samplerModes_)))
            {
                part_.baseSamplerMode = static_cast<MESH_VFX_SAMPLER_MODE>(baseSamplerMode_);
            }
            if (ImGui::Combo("Normal Sampler", &normalSamplerMode_, samplerModes_, IM_ARRAYSIZE(samplerModes_)))
            {
                part_.normalSamplerMode = static_cast<MESH_VFX_SAMPLER_MODE>(normalSamplerMode_);
            }
            if (ImGui::Combo("Mask Sampler", &maskSamplerMode_, samplerModes_, IM_ARRAYSIZE(samplerModes_)))
            {
                part_.maskSamplerMode = static_cast<MESH_VFX_SAMPLER_MODE>(maskSamplerMode_);
            }
            if (ImGui::Combo("Noise Sampler", &noiseSamplerMode_, samplerModes_, IM_ARRAYSIZE(samplerModes_)))
            {
                part_.noiseSamplerMode = static_cast<MESH_VFX_SAMPLER_MODE>(noiseSamplerMode_);
            }
            if (ImGui::Combo("Distortion Sampler", &distortionSamplerMode_, samplerModes_, IM_ARRAYSIZE(samplerModes_)))
            {
                part_.distortionSamplerMode = static_cast<MESH_VFX_SAMPLER_MODE>(distortionSamplerMode_);
            }
            if (ImGui::Combo("Edge Pattern Sampler", &edgePatternSamplerMode_, samplerModes_, IM_ARRAYSIZE(samplerModes_)))
            {
                part_.edgePatternSamplerMode = static_cast<MESH_VFX_SAMPLER_MODE>(edgePatternSamplerMode_);
            }
            if (ImGui::Combo("Emissive Sampler", &emissiveSamplerMode_, samplerModes_, IM_ARRAYSIZE(samplerModes_)))
            {
                part_.emissiveSamplerMode = static_cast<MESH_VFX_SAMPLER_MODE>(emissiveSamplerMode_);
            }
            if (ImGui::Combo("Gradient Sampler", &gradientSamplerMode_, samplerModes_, IM_ARRAYSIZE(samplerModes_)))
            {
                part_.gradientSamplerMode = static_cast<MESH_VFX_SAMPLER_MODE>(gradientSamplerMode_);
            }
            ImGui::DragFloat2("Base UV Scale", &part_.baseUVScale.x, 0.01f, 0.001f, 100.0f);
            ImGui::DragFloat2("Mask UV Scale", &part_.maskUVScale.x, 0.01f, 0.001f, 100.0f);
            ImGui::DragFloat2("Noise UV Scale", &part_.noiseUVScale.x, 0.01f, 0.001f, 100.0f);
            ImGui::DragFloat2("Normal UV Scale", &part_.normalUVScale.x, 0.01f, 0.001f, 100.0f);
            ImGui::DragFloat2("Distortion Noise UV Scale", &part_.distortionNoiseUVScale.x, 0.01f, 0.001f, 100.0f);
            ImGui::DragFloat2("Edge Pattern UV Scale", &part_.edgePatternUVScale.x, 0.01f, 0.001f, 100.0f);
            ImGui::DragFloat2("Emissive UV Scale", &part_.emissiveUVScale.x, 0.01f, 0.001f, 100.0f);
            ImGui::DragFloat("Alpha", &part_.alpha, 0.01f, 0.0f, 1.0f);
            ImGui::DragFloat("Normal Strength", &part_.normalStrength, 0.01f, 0.0f, 10.0f);
            ImGui::DragFloat("Noise Strength", &part_.noiseStrength, 0.001f, 0.0f, 1.0f);
            if (ImGui::TreeNode("Vertex Noise Deformation"))
            {
                ImGui::Checkbox("Use Vertex Noise", &part_.useVertexNoise);
                ImGui::TextDisabled("Displaces vertices along their normals. Dense meshes produce smoother results.");
                ImGui::DragFloat(
                    "Vertex Noise Horizontal XZ Strength",
                    &part_.vertexNoiseHorizontalStrength,
                    0.001f,
                    0.0f,
                    100.0f);
                ImGui::DragFloat(
                    "Vertex Noise Vertical Y Strength",
                    &part_.vertexNoiseVerticalStrength,
                    0.001f,
                    0.0f,
                    100.0f);
                ImGui::DragFloat(
                    "Vertex Noise Horizontal XZ Scale",
                    &part_.vertexNoiseHorizontalScale,
                    0.01f,
                    0.001f,
                    100.0f);
                ImGui::DragFloat(
                    "Vertex Noise Vertical Y Scale",
                    &part_.vertexNoiseVerticalScale,
                    0.01f,
                    0.001f,
                    100.0f);
                ImGui::DragFloat3("Vertex Noise Speed", &part_.vertexNoiseSpeed.x, 0.01f);
                ImGui::TreePop();
            }
            ImGui::DragFloat("Fresnel Power", &part_.fresnelPower, 0.01f, 0.1f, 10.0f);
            ImGui::DragFloat("Fresnel Strength", &part_.fresnelStrength, 0.01f, 0.0f, 20.0f);
            ImGui::DragFloat("Dissolve Threshold", &part_.dissolveThreshold, 0.01f, 0.0f, 1.0f);
            if (ImGui::TreeNode("Dissolve Edge"))
            {
                ImGui::Checkbox("Use Dissolve Edge", &part_.useDissolveEdge);
                ImGui::ColorEdit4("Dissolve Edge Color", &part_.dissolveEdgeColor.x);
                ImGui::DragFloat("Dissolve Edge Width", &part_.dissolveEdgeWidth, 0.001f, 0.0f, 1.0f);
                ImGui::DragFloat("Dissolve Edge Strength", &part_.dissolveEdgeStrength, 0.05f, 0.0f, 50.0f);
                ImGui::TreePop();
            }
            if (ImGui::TreeNode("Fresnel Noise"))
            {
                ImGui::Checkbox("Use Fresnel Noise", &part_.useFresnelNoise);
                ImGui::TextDisabled("Uses the Noise Texture and Noise UV settings.");
                ImGui::DragFloat("Fresnel Noise Strength", &part_.fresnelNoiseStrength, 0.01f, 0.0f, 1.0f);
                ImGui::TreePop();
            }
            if (ImGui::TreeNode("Back Face Boost"))
            {
                ImGui::Checkbox("Use Back Face Boost", &part_.useBackFaceBoost);
                ImGui::ColorEdit4("Back Face Color", &part_.backFaceColor.x);
                ImGui::DragFloat(
                    "Back Face Emissive Strength",
                    &part_.backFaceEmissiveStrength,
                    0.05f,
                    0.0f,
                    50.0f);
                ImGui::TreePop();
            }
            ImGui::Checkbox("Use Mask", &part_.useMask);
            ImGui::Checkbox("Use Noise", &part_.useNoise);
            ImGui::Checkbox("Use Normal", &part_.useNormal);
            ImGui::Checkbox("Use Fresnel", &part_.useFresnel);

            if (ImGui::TreeNode("Masked Area Distortion"))
            {
                // Maskで消える領域に背景歪みを重ね、熱気や空間歪みのような表現を作る。
                ImGui::Checkbox("Use Mask Distortion", &part_.useMaskDistortion);
                ImGui::TextDisabled("Distorts the background where the mask becomes transparent. Uses Distortion Noise Resource.");
                ImGui::DragFloat(
                    "Mask Distortion Strength",
                    &part_.maskDistortionStrength,
                    0.1f,
                    0.0f,
                    100.0f,
                    "%.1f px");
                ImGui::DragFloat(
                    "Mask Distortion Opacity",
                    &part_.maskDistortionOpacity,
                    0.01f,
                    0.0f,
                    1.0f);
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Edge Highlight"))
            {
                // 頂点を膨らませず、Mesh面の内側だけでシルエットや交差部分を強調する。
                ImGui::Checkbox("Use Edge Highlight", &part_.useEdgeHighlight);
                ImGui::TextDisabled("Highlights the mesh silhouette/intersection inside the mesh surface. It does not expand vertices.");
                ImGui::ColorEdit4("Edge Color", &part_.edgeColor.x);
                ImGui::DragFloat(
                    "Silhouette Edge Strength",
                    &part_.silhouetteEdgeStrength,
                    0.01f,
                    0.0f,
                    10.0f);
                ImGui::DragFloat(
                    "Silhouette Edge Power",
                    &part_.silhouetteEdgePower,
                    0.05f,
                    0.1f,
                    20.0f);
                ImGui::DragFloat(
                    "Intersection Edge Strength",
                    &part_.intersectionEdgeStrength,
                    0.01f,
                    0.0f,
                    10.0f);
                ImGui::DragFloat(
                    "Intersection Edge Width",
                    &part_.intersectionEdgeWidth,
                    0.0001f,
                    0.0001f,
                    0.1f,
                    "%.4f");
                const char* edgePatternModes_[] = { "None", "Base Texture", "Mask Texture" };
                int edgePatternMode_ = static_cast<int>(part_.edgePatternMode);
                if (ImGui::Combo("Edge Pattern Source", &edgePatternMode_, edgePatternModes_, IM_ARRAYSIZE(edgePatternModes_)))
                {
                    part_.edgePatternMode = static_cast<MESH_VFX_EDGE_PATTERN_MODE>(
                        std::clamp(edgePatternMode_, 0, IM_ARRAYSIZE(edgePatternModes_) - 1));
                }
                ImGui::DragFloat(
                    "Edge Pattern Strength",
                    &part_.edgePatternStrength,
                    0.01f,
                    0.0f,
                    1.0f);
                ImGui::TreePop();
            }

            const char* blendItems_[] = { "Alpha", "Additive" };
            int blendMode_ = static_cast<int>(part_.blendMode);
            if (ImGui::Combo("Blend Mode", &blendMode_, blendItems_, IM_ARRAYSIZE(blendItems_)))
            {
                part_.blendMode = static_cast<MeshVFXBlendMode>(blendMode_);
            }

                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }

            if (ImGui::Button("Reload Part Resource"))
            {
                LoadPartResource(static_cast<size_t>(partIndex_));
            }
            ImGui::SameLine();
            removePart_ = ImGui::Button("Remove Part");
            ImGui::TreePop();
        }
        ImGui::PopID();

        if (removePart_)
        {
            asset.parts.erase(asset.parts.begin() + partIndex_);
            runtimeParts.erase(runtimeParts.begin() + partIndex_);
            break;
        }
    }

    if (ImGui::Button("Reload All Resources")) LoadPartResources();
}
void MeshVFXComponent::Play() { isPlaying = true; }
void MeshVFXComponent::Stop() { isPlaying = false; }
void MeshVFXComponent::Reset() { time = 0.0f; }

void MeshVFXComponent::RefreshAssetLists()
{
    modelAssetPaths.clear();
    textureAssetPaths.clear();

    auto collectAssets_ = [](const std::filesystem::path& rootPath_,
        const std::vector<std::string>& extensions_,
        std::vector<std::string>& assetPaths_)
        {
            std::error_code error_;
            if (!std::filesystem::is_directory(rootPath_, error_)) return;

            for (const auto& entry_ : std::filesystem::recursive_directory_iterator(rootPath_, error_))
            {
                if (error_) break;
                if (!entry_.is_regular_file(error_) || !HasExtension(entry_.path(), extensions_)) continue;
                assetPaths_.push_back(WindowsDialogHelper::PathToUtf8(entry_.path().lexically_normal()));
            }
            std::sort(assetPaths_.begin(), assetPaths_.end());
        };

    collectAssets_("Data/Model", { ".gltf", ".glb" }, modelAssetPaths);
    collectAssets_("Data/Texture", { ".png", ".jpg", ".jpeg", ".bmp", ".tga", ".dds", ".hdr" }, textureAssetPaths);
}

void MeshVFXComponent::Serialize(nlohmann::json& json_) const
{
    // MeshVFX全体をJSONへ保存する。Part単体保存でもこの形式から該当Partだけ抜き出す。
    json_["IsPlaying"] = isPlaying;
    json_["PlaybackSpeed"] = playbackSpeed;
    json_["Parts"] = nlohmann::json::array();
    for (const MeshVFXPart& part_ : asset.parts)
    {
        nlohmann::json partJson_;
        partJson_["ModelPath"] = part_.modelPath;
        partJson_["MeshIndex"] = part_.meshIndex;
        partJson_["RenderEnabled"] = part_.renderEnabled;
        partJson_["UseMaterialBaseTexture"] = part_.useMaterialBaseTexture;
        partJson_["UseMaterialNormalTexture"] = part_.useMaterialNormalTexture;
        partJson_["Position"] = { part_.position.x, part_.position.y, part_.position.z };
        partJson_["Rotation"] = { part_.rotation.x, part_.rotation.y, part_.rotation.z };
        partJson_["Scale"] = { part_.scale.x, part_.scale.y, part_.scale.z };
        partJson_["InternalMeshCylinderRadius"] = part_.shapeCylinderRadius;
        const MeshVFXInternalMeshSettings& internalMesh_ = part_.internalMeshSettings;
        partJson_["InternalMeshSettings"] =
        {
            { "Segments", internalMesh_.segments },
            { "StartAngleDegrees", internalMesh_.startAngleDegrees },
            { "EndAngleDegrees", internalMesh_.endAngleDegrees },
            { "TaperPower", internalMesh_.taperPower },
            { "TipRate", internalMesh_.tipRate },
            { "BaseRadius", internalMesh_.baseRadius },
            { "RadiusTaperAmount", internalMesh_.radiusTaperAmount },
            { "TipWidth", internalMesh_.tipWidth },
            { "CenterWidth", internalMesh_.centerWidth },
            { "InnerWidthScale", internalMesh_.innerWidthScale },
            { "OuterWidthScale", internalMesh_.outerWidthScale },
            { "WallPositionRate", internalMesh_.wallPositionRate },
            { "WallHeight", internalMesh_.wallHeight },
            { "WallDepth", internalMesh_.wallDepth },
            { "ConeTipRadiusRate", internalMesh_.coneTipRadiusRate },
            { "ConeLength", internalMesh_.coneLength },
        };
        partJson_["MeshRotationSpeed"] =
        {
            part_.meshRotationSpeed.x,
            part_.meshRotationSpeed.y,
            part_.meshRotationSpeed.z,
        };
        partJson_["Duration"] = part_.duration;
        partJson_["Loop"] = part_.loop;
        partJson_["AnimateScale"] = part_.animateScale;
        partJson_["ScaleStart"] = { part_.scaleStart.x, part_.scaleStart.y, part_.scaleStart.z };
        partJson_["ScaleEnd"] = { part_.scaleEnd.x, part_.scaleEnd.y, part_.scaleEnd.z };
        partJson_["AnimateAlpha"] = part_.animateAlpha;
        partJson_["AlphaStart"] = part_.alphaStart;
        partJson_["AlphaEnd"] = part_.alphaEnd;
        partJson_["BaseTexturePath"] = part_.baseTexturePath;
        partJson_["NormalTexturePath"] = part_.normalTexturePath;
        partJson_["MaskTexturePath"] = part_.maskTexturePath;
        partJson_["NoiseTexturePath"] = part_.noiseTexturePath;
        partJson_["DistortionNoiseTexturePath"] = part_.distortionNoiseTexturePath;
        partJson_["VertexNoiseTexturePath"] = part_.vertexNoiseTexturePath;
        partJson_["EdgePatternTexturePath"] = part_.edgePatternTexturePath;
        partJson_["EmissiveTexturePath"] = part_.emissiveTexturePath;
        partJson_["GradientTexturePath"] = part_.gradientTexturePath;
        partJson_["MaskLayers"] = nlohmann::json::array();
        for (const MeshVFXMaskLayer& layer_ : part_.maskLayers)
        {
            partJson_["MaskLayers"].push_back(
                {
                    { "Path", layer_.path },
                    { "BlendMode", static_cast<int>(layer_.blendMode) },
                    { "Invert", layer_.invert },
                });
        }
        partJson_["NoiseTexturePaths"] = part_.noiseTexturePaths;
        partJson_["Color"] = { part_.color.x, part_.color.y, part_.color.z, part_.color.w };
        partJson_["BaseUVSpeed"] = { part_.baseUVSpeed.x, part_.baseUVSpeed.y };
        partJson_["MaskUVSpeed"] = { part_.maskUVSpeed.x, part_.maskUVSpeed.y };
        partJson_["NoiseUVSpeed"] = { part_.noiseUVSpeed.x, part_.noiseUVSpeed.y };
        partJson_["BaseSamplerMode"] = static_cast<int>(part_.baseSamplerMode);
        partJson_["NormalSamplerMode"] = static_cast<int>(part_.normalSamplerMode);
        partJson_["MaskSamplerMode"] = static_cast<int>(part_.maskSamplerMode);
        partJson_["NoiseSamplerMode"] = static_cast<int>(part_.noiseSamplerMode);
        partJson_["DistortionSamplerMode"] = static_cast<int>(part_.distortionSamplerMode);
        partJson_["EdgePatternSamplerMode"] = static_cast<int>(part_.edgePatternSamplerMode);
        partJson_["EmissiveSamplerMode"] = static_cast<int>(part_.emissiveSamplerMode);
        partJson_["GradientSamplerMode"] = static_cast<int>(part_.gradientSamplerMode);
        partJson_["BaseUVScale"] = { part_.baseUVScale.x, part_.baseUVScale.y };
        partJson_["MaskUVScale"] = { part_.maskUVScale.x, part_.maskUVScale.y };
        partJson_["NoiseUVScale"] = { part_.noiseUVScale.x, part_.noiseUVScale.y };
        partJson_["NormalUVScale"] = { part_.normalUVScale.x, part_.normalUVScale.y };
        partJson_["DistortionNoiseUVScale"] = { part_.distortionNoiseUVScale.x, part_.distortionNoiseUVScale.y };
        partJson_["EdgePatternUVScale"] = { part_.edgePatternUVScale.x, part_.edgePatternUVScale.y };
        partJson_["EmissiveUVScale"] = { part_.emissiveUVScale.x, part_.emissiveUVScale.y };
        partJson_["UVRotationCenter"] = { part_.uvRotationCenter.x, part_.uvRotationCenter.y };
        partJson_["BaseUVRotation"] = part_.baseUVRotation;
        partJson_["MaskUVRotation"] = part_.maskUVRotation;
        partJson_["NoiseUVRotation"] = part_.noiseUVRotation;
        partJson_["BaseUVRotationSpeed"] = part_.baseUVRotationSpeed;
        partJson_["MaskUVRotationSpeed"] = part_.maskUVRotationSpeed;
        partJson_["NoiseUVRotationSpeed"] = part_.noiseUVRotationSpeed;
        partJson_["UVRadialScale"] = part_.uvRadialScale;
        partJson_["UVRadialScaleSpeed"] = part_.uvRadialScaleSpeed;
        partJson_["Alpha"] = part_.alpha;
        partJson_["EmissiveStrength"] = part_.emissiveStrength;
        partJson_["EmissivePulseSpeed"] = part_.emissivePulseSpeed;
        partJson_["EmissivePulseAmount"] = part_.emissivePulseAmount;
        partJson_["NormalStrength"] = part_.normalStrength;
        partJson_["NoiseStrength"] = part_.noiseStrength;
        partJson_["VertexNoiseHorizontalStrength"] = part_.vertexNoiseHorizontalStrength;
        partJson_["VertexNoiseVerticalStrength"] = part_.vertexNoiseVerticalStrength;
        partJson_["VertexNoiseHorizontalScale"] = part_.vertexNoiseHorizontalScale;
        partJson_["VertexNoiseVerticalScale"] = part_.vertexNoiseVerticalScale;
        partJson_["VertexNoiseSpeed"] =
        {
            part_.vertexNoiseSpeed.x,
            part_.vertexNoiseSpeed.y,
            part_.vertexNoiseSpeed.z,
        };
        partJson_["MaskDistortionStrength"] = part_.maskDistortionStrength;
        partJson_["MaskDistortionOpacity"] = part_.maskDistortionOpacity;
        partJson_["EdgeColor"] =
        {
            part_.edgeColor.x,
            part_.edgeColor.y,
            part_.edgeColor.z,
            part_.edgeColor.w,
        };
        partJson_["SilhouetteEdgeStrength"] = part_.silhouetteEdgeStrength;
        partJson_["SilhouetteEdgePower"] = part_.silhouetteEdgePower;
        partJson_["IntersectionEdgeStrength"] = part_.intersectionEdgeStrength;
        partJson_["IntersectionEdgeWidth"] = part_.intersectionEdgeWidth;
        partJson_["EdgePatternMode"] = static_cast<int>(part_.edgePatternMode);
        partJson_["EdgePatternStrength"] = part_.edgePatternStrength;
        partJson_["FresnelPower"] = part_.fresnelPower;
        partJson_["FresnelStrength"] = part_.fresnelStrength;
        partJson_["DissolveThreshold"] = part_.dissolveThreshold;
        partJson_["DissolveEdgeColor"] =
        {
            part_.dissolveEdgeColor.x,
            part_.dissolveEdgeColor.y,
            part_.dissolveEdgeColor.z,
            part_.dissolveEdgeColor.w,
        };
        partJson_["BackFaceColor"] =
        {
            part_.backFaceColor.x,
            part_.backFaceColor.y,
            part_.backFaceColor.z,
            part_.backFaceColor.w,
        };
        partJson_["DissolveEdgeWidth"] = part_.dissolveEdgeWidth;
        partJson_["DissolveEdgeStrength"] = part_.dissolveEdgeStrength;
        partJson_["FresnelNoiseStrength"] = part_.fresnelNoiseStrength;
        partJson_["BackFaceEmissiveStrength"] = part_.backFaceEmissiveStrength;
        partJson_["GradientFixedUv"] = part_.gradientFixedUv;
        partJson_["GradientStartUv"] = part_.gradientStartUv;
        partJson_["GradientEndUv"] = part_.gradientEndUv;
        partJson_["AlphaCurveName"] = part_.alphaCurveName;
        partJson_["ScaleCurveName"] = part_.scaleCurveName;
        partJson_["DissolveCurveName"] = part_.dissolveCurveName;
        partJson_["EmissiveCurveName"] = part_.emissiveCurveName;
        partJson_["UVScrollCurveName"] = part_.uvScrollCurveName;
        partJson_["UVRotationCurveName"] = part_.uvRotationCurveName;
        partJson_["UseMask"] = part_.useMask;
        partJson_["UseNoise"] = part_.useNoise;
        partJson_["UseVertexNoise"] = part_.useVertexNoise;
        partJson_["UseMaskDistortion"] = part_.useMaskDistortion;
        partJson_["UseEdgeHighlight"] = part_.useEdgeHighlight;
        partJson_["UseNormal"] = part_.useNormal;
        partJson_["UseFresnel"] = part_.useFresnel;
        partJson_["UseGradientTexture"] = part_.useGradientTexture;
        partJson_["UseDissolveEdge"] = part_.useDissolveEdge;
        partJson_["UseFresnelNoise"] = part_.useFresnelNoise;
        partJson_["UseBackFaceBoost"] = part_.useBackFaceBoost;
        partJson_["GradientSampleMode"] = static_cast<int>(part_.gradientSampleMode);
        partJson_["GradientDirection"] = static_cast<int>(part_.gradientDirection);
        partJson_["BlendMode"] = static_cast<int>(part_.blendMode);
        json_["Parts"].push_back(partJson_);
    }
}

void MeshVFXComponent::Deserialize(const nlohmann::json& json_)
{
    // JSONからPart設定を復元する。旧形式のMask/Noise単体パスもLayer配列へ補完する。
    asset.parts.clear();
    runtimeParts.clear();
    isPlaying = json_.value("IsPlaying", true);
    playbackSpeed = (std::max)(json_.value("PlaybackSpeed", 1.0f), 0.0f);
    time = 0.0f;
    if (!json_.contains("Parts") || !json_["Parts"].is_array()) return;

    for (const auto& partJson_ : json_["Parts"])
    {
        MeshVFXPart part_;
        part_.modelPath = partJson_.value("ModelPath", "");
        NormalizeInternalMeshModelPath(part_.modelPath);
        part_.meshIndex = partJson_.value("MeshIndex", -1);
        part_.renderEnabled = partJson_.value("RenderEnabled", false);
        part_.useMaterialBaseTexture = partJson_.value("UseMaterialBaseTexture", true);
        part_.useMaterialNormalTexture = partJson_.value("UseMaterialNormalTexture", true);
        ReadFloatArray(partJson_, "Position", part_.position, 3);
        ReadFloatArray(partJson_, "Rotation", part_.rotation, 3);
        ReadFloatArray(partJson_, "Scale", part_.scale, 3);
        const float internalMeshCylinderRadius_ = partJson_.value(
            "InternalMeshCylinderRadius",
            partJson_.value("ShapeCylinderRadius", 1.0f));
        part_.shapeCylinderRadius = (std::max)(internalMeshCylinderRadius_, 0.0001f);
        if (partJson_.contains("InternalMeshSettings") &&
            partJson_["InternalMeshSettings"].is_object())
        {
            const nlohmann::json& internalMeshJson_ = partJson_["InternalMeshSettings"];
            MeshVFXInternalMeshSettings& internalMesh_ = part_.internalMeshSettings;
            internalMesh_.segments = std::clamp(
                internalMeshJson_.value("Segments", internalMesh_.segments),
                3,
                512);
            internalMesh_.startAngleDegrees = internalMeshJson_.value(
                "StartAngleDegrees",
                internalMesh_.startAngleDegrees);
            internalMesh_.endAngleDegrees = internalMeshJson_.value(
                "EndAngleDegrees",
                internalMesh_.endAngleDegrees);
            internalMesh_.taperPower = (std::max)(
                internalMeshJson_.value("TaperPower", internalMesh_.taperPower),
                0.01f);
            internalMesh_.tipRate = std::clamp(
                internalMeshJson_.value("TipRate", internalMesh_.tipRate),
                0.0f,
                1.0f);
            internalMesh_.baseRadius = (std::max)(
                internalMeshJson_.value("BaseRadius", internalMesh_.baseRadius),
                0.0001f);
            internalMesh_.radiusTaperAmount = internalMeshJson_.value(
                "RadiusTaperAmount",
                internalMesh_.radiusTaperAmount);
            internalMesh_.tipWidth = (std::max)(
                internalMeshJson_.value("TipWidth", internalMesh_.tipWidth),
                0.0f);
            internalMesh_.centerWidth = (std::max)(
                internalMeshJson_.value("CenterWidth", internalMesh_.centerWidth),
                0.0f);
            internalMesh_.innerWidthScale = internalMeshJson_.value(
                "InnerWidthScale",
                internalMesh_.innerWidthScale);
            internalMesh_.outerWidthScale = internalMeshJson_.value(
                "OuterWidthScale",
                internalMesh_.outerWidthScale);
            internalMesh_.wallPositionRate = std::clamp(
                internalMeshJson_.value("WallPositionRate", internalMesh_.wallPositionRate),
                0.0f,
                1.0f);
            internalMesh_.wallHeight = (std::max)(
                internalMeshJson_.value("WallHeight", internalMesh_.wallHeight),
                0.0f);
            internalMesh_.wallDepth = (std::max)(
                internalMeshJson_.value("WallDepth", internalMesh_.wallDepth),
                0.0f);
            internalMesh_.coneTipRadiusRate = std::clamp(
                internalMeshJson_.value(
                    "ConeTipRadiusRate",
                    internalMesh_.coneTipRadiusRate),
                0.0f,
                1.0f);
            internalMesh_.coneLength = (std::max)(
                internalMeshJson_.value("ConeLength", internalMesh_.coneLength),
                0.0001f);
        }
        ReadFloatArray(partJson_, "MeshRotationSpeed", part_.meshRotationSpeed, 3);
        part_.duration = (std::max)(partJson_.value("Duration", 0.0f), 0.0f);
        part_.loop = partJson_.value("Loop", true);
        part_.animateScale = partJson_.value("AnimateScale", false);
        ReadFloatArray(partJson_, "ScaleStart", part_.scaleStart, 3);
        ReadFloatArray(partJson_, "ScaleEnd", part_.scaleEnd, 3);
        part_.animateAlpha = partJson_.value("AnimateAlpha", false);
        part_.alphaStart = partJson_.value("AlphaStart", 1.0f);
        part_.alphaEnd = partJson_.value("AlphaEnd", 1.0f);
        part_.baseTexturePath = partJson_.value("BaseTexturePath", "");
        part_.normalTexturePath = partJson_.value("NormalTexturePath", "");
        part_.maskTexturePath = partJson_.value("MaskTexturePath", "");
        part_.noiseTexturePath = partJson_.value("NoiseTexturePath", "");
        part_.distortionNoiseTexturePath = partJson_.value("DistortionNoiseTexturePath", "");
        part_.vertexNoiseTexturePath = partJson_.value("VertexNoiseTexturePath", "");
        part_.edgePatternTexturePath = partJson_.value("EdgePatternTexturePath", "");
        part_.emissiveTexturePath = partJson_.value("EmissiveTexturePath", "");
        part_.gradientTexturePath = partJson_.value("GradientTexturePath", "");
        if (partJson_.contains("MaskLayers") && partJson_["MaskLayers"].is_array())
        {
            for (const auto& layerJson_ : partJson_["MaskLayers"])
            {
                MeshVFXMaskLayer layer_;
                layer_.path = layerJson_.value("Path", "");
                layer_.blendMode = static_cast<MeshVFXMaskBlendMode>(
                    std::clamp(layerJson_.value("BlendMode", 0), 0, 1));
                layer_.invert = layerJson_.value("Invert", false);
                part_.maskLayers.push_back(layer_);
            }
        }
        if (partJson_.contains("NoiseTexturePaths") && partJson_["NoiseTexturePaths"].is_array())
        {
            for (const auto& pathJson_ : partJson_["NoiseTexturePaths"])
            {
                part_.noiseTexturePaths.push_back(pathJson_.get<std::string>());
            }
        }
        if (part_.maskLayers.empty() && !part_.maskTexturePath.empty())
        {
            part_.maskLayers.push_back({ part_.maskTexturePath, MeshVFXMaskBlendMode::MULTIPLY, false });
        }
        if (part_.noiseTexturePaths.empty() && !part_.noiseTexturePath.empty())
        {
            part_.noiseTexturePaths.push_back(part_.noiseTexturePath);
        }
        ReadFloatArray(partJson_, "Color", part_.color, 4);
        ReadFloatArray(partJson_, "BaseUVSpeed", part_.baseUVSpeed, 2);
        ReadFloatArray(partJson_, "MaskUVSpeed", part_.maskUVSpeed, 2);
        ReadFloatArray(partJson_, "NoiseUVSpeed", part_.noiseUVSpeed, 2);
        part_.baseSamplerMode = static_cast<MESH_VFX_SAMPLER_MODE>(
            std::clamp(partJson_.value("BaseSamplerMode", 0), 0, 2));
        part_.normalSamplerMode = static_cast<MESH_VFX_SAMPLER_MODE>(
            std::clamp(partJson_.value("NormalSamplerMode", 0), 0, 2));
        part_.maskSamplerMode = static_cast<MESH_VFX_SAMPLER_MODE>(
            std::clamp(partJson_.value("MaskSamplerMode", 0), 0, 2));
        part_.noiseSamplerMode = static_cast<MESH_VFX_SAMPLER_MODE>(
            std::clamp(partJson_.value("NoiseSamplerMode", 0), 0, 2));
        part_.distortionSamplerMode = static_cast<MESH_VFX_SAMPLER_MODE>(
            std::clamp(partJson_.value("DistortionSamplerMode", 0), 0, 2));
        part_.edgePatternSamplerMode = static_cast<MESH_VFX_SAMPLER_MODE>(
            std::clamp(partJson_.value("EdgePatternSamplerMode", 0), 0, 2));
        part_.emissiveSamplerMode = static_cast<MESH_VFX_SAMPLER_MODE>(
            std::clamp(partJson_.value("EmissiveSamplerMode", 0), 0, 2));
        part_.gradientSamplerMode = static_cast<MESH_VFX_SAMPLER_MODE>(
            std::clamp(partJson_.value("GradientSamplerMode", 0), 0, 2));
        ReadFloatArray(partJson_, "BaseUVScale", part_.baseUVScale, 2);
        ReadFloatArray(partJson_, "MaskUVScale", part_.maskUVScale, 2);
        ReadFloatArray(partJson_, "NoiseUVScale", part_.noiseUVScale, 2);
        ReadFloatArray(partJson_, "NormalUVScale", part_.normalUVScale, 2);
        ReadFloatArray(partJson_, "DistortionNoiseUVScale", part_.distortionNoiseUVScale, 2);
        ReadFloatArray(partJson_, "EdgePatternUVScale", part_.edgePatternUVScale, 2);
        ReadFloatArray(partJson_, "EmissiveUVScale", part_.emissiveUVScale, 2);
        ReadFloatArray(partJson_, "UVRotationCenter", part_.uvRotationCenter, 2);
        part_.baseUVRotation = partJson_.value("BaseUVRotation", 0.0f);
        part_.maskUVRotation = partJson_.value("MaskUVRotation", 0.0f);
        part_.noiseUVRotation = partJson_.value("NoiseUVRotation", 0.0f);
        part_.baseUVRotationSpeed = partJson_.value("BaseUVRotationSpeed", 0.0f);
        part_.maskUVRotationSpeed = partJson_.value("MaskUVRotationSpeed", 0.0f);
        part_.noiseUVRotationSpeed = partJson_.value("NoiseUVRotationSpeed", 0.0f);
        part_.uvRadialScale = (std::max)(partJson_.value("UVRadialScale", 1.0f), 0.001f);
        part_.uvRadialScaleSpeed = partJson_.value("UVRadialScaleSpeed", 0.0f);
        part_.alpha = partJson_.value("Alpha", 1.0f);
        part_.emissiveStrength = (std::max)(partJson_.value("EmissiveStrength", 3.0f), 0.0f);
        part_.emissivePulseSpeed = (std::max)(partJson_.value("EmissivePulseSpeed", 2.0f), 0.0f);
        part_.emissivePulseAmount = (std::max)(partJson_.value("EmissivePulseAmount", 0.35f), 0.0f);
        part_.normalStrength = partJson_.value("NormalStrength", 1.0f);
        part_.noiseStrength = partJson_.value("NoiseStrength", 0.05f);
        const float legacyVertexNoiseStrength_ =
            (std::max)(partJson_.value("VertexNoiseStrength", 0.1f), 0.0f);
        part_.vertexNoiseHorizontalStrength = (std::max)(
            partJson_.value("VertexNoiseHorizontalStrength", legacyVertexNoiseStrength_),
            0.0f);
        part_.vertexNoiseVerticalStrength = (std::max)(
            partJson_.value("VertexNoiseVerticalStrength", legacyVertexNoiseStrength_),
            0.0f);
        const float legacyVertexNoiseScale_ =
            (std::max)(partJson_.value("VertexNoiseScale", 1.0f), 0.001f);
        part_.vertexNoiseHorizontalScale = (std::max)(
            partJson_.value("VertexNoiseHorizontalScale", legacyVertexNoiseScale_),
            0.001f);
        part_.vertexNoiseVerticalScale = (std::max)(
            partJson_.value("VertexNoiseVerticalScale", legacyVertexNoiseScale_),
            0.001f);
        ReadFloatArray(partJson_, "VertexNoiseSpeed", part_.vertexNoiseSpeed, 3);
        part_.maskDistortionStrength = (std::max)(partJson_.value("MaskDistortionStrength", 8.0f), 0.0f);
        part_.maskDistortionOpacity = std::clamp(partJson_.value("MaskDistortionOpacity", 0.5f), 0.0f, 1.0f);
        ReadFloatArray(partJson_, "EdgeColor", part_.edgeColor, 4);
        part_.silhouetteEdgeStrength = (std::max)(partJson_.value("SilhouetteEdgeStrength", 0.0f), 0.0f);
        part_.silhouetteEdgePower = (std::max)(partJson_.value("SilhouetteEdgePower", 3.0f), 0.1f);
        part_.intersectionEdgeStrength = (std::max)(partJson_.value("IntersectionEdgeStrength", 0.0f), 0.0f);
        part_.intersectionEdgeWidth = (std::max)(partJson_.value("IntersectionEdgeWidth", 0.002f), 0.0001f);
        part_.edgePatternMode = static_cast<MESH_VFX_EDGE_PATTERN_MODE>(
            std::clamp(partJson_.value("EdgePatternMode", 0), 0, 2));
        part_.edgePatternStrength = std::clamp(partJson_.value("EdgePatternStrength", 0.0f), 0.0f, 1.0f);
        part_.fresnelPower = partJson_.value("FresnelPower", 2.0f);
        part_.fresnelStrength = partJson_.value("FresnelStrength", 1.0f);
        part_.dissolveThreshold = partJson_.value("DissolveThreshold", 0.0f);
        ReadFloatArray(partJson_, "DissolveEdgeColor", part_.dissolveEdgeColor, 4);
        ReadFloatArray(partJson_, "BackFaceColor", part_.backFaceColor, 4);
        part_.dissolveEdgeWidth = std::clamp(partJson_.value("DissolveEdgeWidth", 0.05f), 0.0f, 1.0f);
        part_.dissolveEdgeStrength = (std::max)(partJson_.value("DissolveEdgeStrength", 3.0f), 0.0f);
        part_.fresnelNoiseStrength = std::clamp(partJson_.value("FresnelNoiseStrength", 0.0f), 0.0f, 1.0f);
        part_.backFaceEmissiveStrength = (std::max)(partJson_.value("BackFaceEmissiveStrength", 0.0f), 0.0f);
        part_.gradientFixedUv = std::clamp(partJson_.value("GradientFixedUv", 0.5f), 0.0f, 1.0f);
        part_.gradientStartUv = std::clamp(partJson_.value("GradientStartUv", 0.0f), 0.0f, 1.0f);
        part_.gradientEndUv = std::clamp(partJson_.value("GradientEndUv", 1.0f), 0.0f, 1.0f);
        part_.alphaCurveName = partJson_.value("AlphaCurveName", std::string{});
        part_.scaleCurveName = partJson_.value("ScaleCurveName", std::string{});
        part_.dissolveCurveName = partJson_.value("DissolveCurveName", std::string{});
        part_.emissiveCurveName = partJson_.value("EmissiveCurveName", std::string{});
        part_.uvScrollCurveName = partJson_.value("UVScrollCurveName", std::string{});
        part_.uvRotationCurveName = partJson_.value("UVRotationCurveName", std::string{});
        part_.useMask = partJson_.value("UseMask", false);
        part_.useNoise = partJson_.value("UseNoise", false);
        part_.useVertexNoise = partJson_.value("UseVertexNoise", false);
        part_.useMaskDistortion = partJson_.value("UseMaskDistortion", false);
        part_.useEdgeHighlight = partJson_.value("UseEdgeHighlight", false);
        part_.useNormal = partJson_.value("UseNormal", false);
        part_.useFresnel = partJson_.value("UseFresnel", false);
        part_.useGradientTexture = partJson_.value("UseGradientTexture", false);
        part_.useDissolveEdge = partJson_.value("UseDissolveEdge", false);
        part_.useFresnelNoise = partJson_.value("UseFresnelNoise", false);
        part_.useBackFaceBoost = partJson_.value("UseBackFaceBoost", false);
        part_.gradientSampleMode = static_cast<MESH_VFX_GRADIENT_SAMPLE_MODE>(
            std::clamp(partJson_.value("GradientSampleMode", 0), 0, 1));
        part_.gradientDirection = static_cast<MESH_VFX_GRADIENT_DIRECTION>(
            std::clamp(partJson_.value("GradientDirection", 0), 0, 1));
        part_.blendMode = static_cast<MeshVFXBlendMode>(std::clamp(partJson_.value("BlendMode", 0), 0, 1));
        asset.parts.push_back(part_);
    }
    LoadPartResources();
}

bool MeshVFXComponent::SaveAsset(const std::string& path_) const
{
    // 現在のMeshVFX全体をAsset JSONとして保存する。
    if (path_.empty()) return false;
    nlohmann::json json_;
    json_["AssetType"] = "MeshVFX";
    json_["AssetVersion"] = 1;
    Serialize(json_["MeshVFX"]);

    const std::filesystem::path assetPath_ = WindowsDialogHelper::Utf8ToPath(path_);
    std::error_code error_;
    if (assetPath_.has_parent_path())
    {
        std::filesystem::create_directories(assetPath_.parent_path(), error_);
        if (error_) return false;
    }
    std::ofstream file_(assetPath_, std::ios::out | std::ios::trunc);
    if (!file_.is_open()) return false;
    file_ << json_.dump(4);
    return file_.good();
}

bool MeshVFXComponent::LoadAsset(const std::string& path_)
{
    // MeshVFX Asset JSONを読み込み、関連Runtimeリソースを再構築する。
    if (path_.empty()) return false;
    const std::filesystem::path assetPath_ = WindowsDialogHelper::Utf8ToPath(path_);
    std::ifstream file_(assetPath_);
    if (!file_.is_open()) return false;

    nlohmann::json json_;
    try
    {
        file_ >> json_;
        const nlohmann::json& meshVFXJson_ =
            json_.contains("MeshVFX") ? json_["MeshVFX"] : json_;
        if (!meshVFXJson_.contains("Parts") || !meshVFXJson_["Parts"].is_array()) return false;
        Deserialize(meshVFXJson_);
        assetPath = path_;
    }
    catch (const nlohmann::json::exception&)
    {
        return false;
    }
    return true;
}

bool MeshVFXComponent::SavePartAsset(size_t partIndex_, const std::string& path_) const
{
    // 選択中Partだけを単体Assetとして保存し、他のMeshVFXへ再利用できるようにする。
    if (path_.empty() || partIndex_ >= asset.parts.size()) return false;

    nlohmann::json componentJson_;
    Serialize(componentJson_);
    nlohmann::json json_;
    json_["AssetType"] = "MeshVFXPart";
    json_["AssetVersion"] = 1;
    json_["Part"] = componentJson_["Parts"][partIndex_];

    const std::filesystem::path assetPath_ = WindowsDialogHelper::Utf8ToPath(path_);
    std::error_code error_;
    if (assetPath_.has_parent_path())
    {
        std::filesystem::create_directories(assetPath_.parent_path(), error_);
        if (error_) return false;
    }
    std::ofstream file_(assetPath_, std::ios::out | std::ios::trunc);
    if (!file_.is_open()) return false;
    file_ << json_.dump(4);
    return file_.good();
}

bool MeshVFXComponent::LoadPartAsset(size_t partIndex_, const std::string& path_)
{
    // 単体Part Assetを読み込み、現在のPartスロットへ差し替える。
    if (path_.empty() || partIndex_ >= asset.parts.size()) return false;
    std::ifstream file_(WindowsDialogHelper::Utf8ToPath(path_));
    if (!file_.is_open()) return false;

    try
    {
        nlohmann::json json_;
        file_ >> json_;
        if (json_.contains("AssetType") && json_.value("AssetType", "") != "MeshVFXPart")
        {
            return false;
        }
        const nlohmann::json& partJson_ = json_.contains("Part") ? json_["Part"] : json_;
        if (!partJson_.is_object() || partJson_.contains("Parts") || partJson_.contains("MeshVFX"))
        {
            return false;
        }

        nlohmann::json componentJson_;
        componentJson_["IsPlaying"] = isPlaying;
        componentJson_["Parts"] = nlohmann::json::array({ partJson_ });
        MeshVFXComponent parsedComponent_;
        parsedComponent_.Deserialize(componentJson_);
        if (parsedComponent_.asset.parts.empty()) return false;

        asset.parts[partIndex_] = std::move(parsedComponent_.asset.parts.front());
        asset.parts[partIndex_].partAssetPath = path_;
        LoadPartResource(partIndex_);
    }
    catch (const nlohmann::json::exception&)
    {
        return false;
    }
    return true;
}

void MeshVFXComponent::SetAsset(const MeshVFXAsset& asset_)
{
    // 外部から渡されたAsset設定を適用し、Texture/Modelを再読み込みする。
    asset = asset_;
    time = 0.0f;
    RebuildRuntimeResources();
}

void MeshVFXComponent::RebuildRuntimeResources()
{
    // Editor変更後など、保存データから実行時GPUリソースを作り直す。
    LoadPartResources();
}

bool MeshVFXComponent::HasRuntimeResources() const
{
    if (asset.parts.empty() || runtimeParts.size() != asset.parts.size())
    {
        return false;
    }

    for (size_t partIndex_ = 0; partIndex_ < asset.parts.size(); ++partIndex_)
    {
        const MeshVFXPart& part_ = asset.parts[partIndex_];
        if (!part_.renderEnabled) continue;
        if (runtimeParts[partIndex_].modelResource) return true;
    }
    return false;
}

void MeshVFXComponent::LoadPartResources()
{
    // 全PartのRuntimeリソースを再読み込みする。
    runtimeParts.resize(asset.parts.size());
    for (size_t partIndex_ = 0; partIndex_ < asset.parts.size(); ++partIndex_)
    {
        LoadPartResource(partIndex_);
    }
}

void MeshVFXComponent::LoadPartResource(size_t partIndex_)
{
    // 指定PartのModel/Textureを読み込み、描画時に参照するRuntimePartへ詰める。
    if (partIndex_ >= asset.parts.size()) return;
    if (runtimeParts.size() < asset.parts.size()) runtimeParts.resize(asset.parts.size());

    ID3D11Device* device_ = GraphicsDeviceManager::Instance().GetDevice();
    if (!device_) return;

    MeshVFXPart& part_ = asset.parts[partIndex_];
    MeshVFXRuntimePart& runtimePart_ = runtimeParts[partIndex_];
    runtimePart_.modelResource.reset();
    NormalizeInternalMeshModelPath(part_.modelPath);
    if (part_.modelPath == INTERNAL_MESH_CYLINDER_MODEL_PATH)
    {
        runtimePart_.modelResource = CreateInternalMeshCylinderModelResource(device_, part_.shapeCylinderRadius);
    }
    else if (part_.modelPath == INTERNAL_MESH_CONE_MODEL_PATH)
    {
        runtimePart_.modelResource = CreateInternalMeshConeModelResource(
            device_,
            part_.shapeCylinderRadius,
            part_.internalMeshSettings);
    }
    else if (part_.modelPath == INTERNAL_MESH_SLASH_CRESCENT_MODEL_PATH)
    {
        runtimePart_.modelResource = CreateInternalMeshSlashCrescentModelResource(device_);
    }
    else if (part_.modelPath == INTERNAL_MESH_SLASH_CROSS_CRESCENT_MODEL_PATH)
    {
        runtimePart_.modelResource = CreateInternalMeshSlashCrossCrescentModelResource(device_);
    }
    else if (part_.modelPath == INTERNAL_MESH_SLASH_VERTICAL_CRESCENT_MODEL_PATH)
    {
        runtimePart_.modelResource = CreateInternalMeshSlashVerticalCrescentModelResource(device_);
    }
    else if (part_.modelPath == INTERNAL_MESH_SLASH_FAN_MODEL_PATH)
    {
        runtimePart_.modelResource = CreateInternalMeshSlashFanModelResource(device_);
    }
    else if (part_.modelPath == INTERNAL_MESH_SLASH_ARC_RIBBON_MODEL_PATH)
    {
        runtimePart_.modelResource = CreateInternalMeshSlashArcRibbonModelResource(device_);
    }
    else if (part_.modelPath == INTERNAL_MESH_SLASH_CROSS_MODEL_PATH)
    {
        runtimePart_.modelResource = CreateInternalMeshSlashCrossModelResource(device_);
    }
    else if (part_.modelPath == INTERNAL_MESH_SLASH_GROUND_MODEL_PATH)
    {
        runtimePart_.modelResource = CreateInternalMeshSlashGroundModelResource(device_);
    }
    else if (part_.modelPath == INTERNAL_MESH_SLASH_RAISED_CRESCENT_MODEL_PATH)
    {
        runtimePart_.modelResource = CreateInternalMeshSlashRaisedCrescentModelResource(
            device_,
            part_.internalMeshSettings);
    }
    else if (!part_.modelPath.empty())
    {
        runtimePart_.modelResource = ModelResourceManager::Instance().GetOrCreateResource(device_, part_.modelPath);
    }

    LoadTexture(device_, part_.baseTexturePath, 0xFFFFFFFF, runtimePart_.baseTexture);
    LoadTexture(device_, part_.normalTexturePath, 0xFFFF7F7F, runtimePart_.normalTexture);
    runtimePart_.maskTextures.clear();
    runtimePart_.noiseTextures.clear();
    for (const MeshVFXMaskLayer& layer_ : part_.maskLayers)
    {
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture_;
        LoadTexture(device_, layer_.path, 0xFFFFFFFF, texture_);
        runtimePart_.maskTextures.push_back(texture_);
    }
    for (const std::string& path_ : part_.noiseTexturePaths)
    {
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture_;
        LoadTexture(device_, path_, 0xFF808080, texture_);
        runtimePart_.noiseTextures.push_back(texture_);
    }
    if (runtimePart_.maskTextures.empty())
    {
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture_;
        LoadTexture(device_, part_.maskTexturePath, 0xFFFFFFFF, texture_);
        runtimePart_.maskTextures.push_back(texture_);
    }
    if (runtimePart_.noiseTextures.empty())
    {
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture_;
        LoadTexture(device_, part_.noiseTexturePath, 0xFF808080, texture_);
        runtimePart_.noiseTextures.push_back(texture_);
    }
    runtimePart_.distortionNoiseTexture.Reset();
    if (!part_.distortionNoiseTexturePath.empty())
    {
        LoadTexture(
            device_,
            part_.distortionNoiseTexturePath,
            0xFF808080,
            runtimePart_.distortionNoiseTexture);
    }
    runtimePart_.vertexNoiseTexture.Reset();
    if (!part_.vertexNoiseTexturePath.empty())
    {
        LoadTexture(
            device_,
            part_.vertexNoiseTexturePath,
            0xFF808080,
            runtimePart_.vertexNoiseTexture);
    }
    runtimePart_.edgePatternTexture.Reset();
    if (!part_.edgePatternTexturePath.empty())
    {
        LoadTexture(
            device_,
            part_.edgePatternTexturePath,
            0xFFFFFFFF,
            runtimePart_.edgePatternTexture);
    }
    runtimePart_.emissiveTexture.Reset();
    if (!part_.emissiveTexturePath.empty())
    {
        LoadTexture(
            device_,
            part_.emissiveTexturePath,
            0xFFFFFFFF,
            runtimePart_.emissiveTexture);
    }
    LoadTexture(device_, part_.gradientTexturePath, 0xFFFFFFFF, runtimePart_.gradientTexture);
}

void MeshVFXComponent::LoadTexture(
    ID3D11Device* device_,
    const std::string& path_,
    UINT fallbackColor_,
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& texture_)
{
    // Texture未設定や読み込み失敗時もShaderが安全に動くよう、指定色のDummy Textureへフォールバックする。
    texture_.Reset();
    if (!path_.empty() && std::filesystem::exists(WindowsDialogHelper::Utf8ToPath(path_)))
    {
        ID3D11ShaderResourceView* loadedTexture_ =
            TextureResourceManager::Instance().LoadTexture(device_, path_);
        if (loadedTexture_)
        {
            texture_ = loadedTexture_;
            return;
        }
    }
    CPUResourceLoader::CreateDummyTexture(device_, fallbackColor_, texture_.GetAddressOf());
}

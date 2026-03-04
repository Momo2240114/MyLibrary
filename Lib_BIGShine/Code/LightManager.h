#pragma once

#include <d3d11.h>
#include <DirectXMath.h>
#include "ComponentBase.h"
#include "GameObject.h"



struct DirectionalLight
{
    DirectX::XMFLOAT3	direction = { 0, -1, 0 };
    DirectX::XMFLOAT3	color = { 1, 1, 1 };
    float intensity;            // 強度
};

struct PointLight 
{
    DirectX::XMFLOAT3 position; // 位置
    float range;                // 影響範囲（半径）
    DirectX::XMFLOAT3 color;    // 色
    float intensity;            // 強度
};

struct SpotLight
{
    DirectX::XMFLOAT3 position;  // 位置
    float range;                 // 範囲
    DirectX::XMFLOAT3 direction; // 向き
    float intensity;             // 強度
    DirectX::XMFLOAT3 color;     // 色
    float innerAngle;            // 内側の角度（100%照らす範囲）
    float outerAngle;            // 外側の角度（減衰しきる範囲）
    float padding;               //（必要に応じてアライメント調整用）
};

class LightManager
{
public:
    // ディレクショナルライト設定
    void SetDirectionalLight(DirectionalLight& light) { directionalLight = light; }
    // ディレクショナルライト取得
    const DirectionalLight& GetDirectionalLight() const { return directionalLight; }
    // 点光源の管理（とりあえず1つ、あるいは固定数で実装する場合）
    void AddPointLight(const PointLight& light) { pointLights.push_back(light); }
    const std::vector<PointLight>& GetPointLights() const { return pointLights; }

    // スポットライト用の管理を追加
    void AddSpotLight(const SpotLight& light) { spotLights.push_back(light); }
    const std::vector<SpotLight>& GetSpotLights() const { return spotLights; }

    // 毎フレームのクリア処理（Componentから集計する場合に必要）
    void ClearLights() {
        pointLights.clear();
        spotLights.clear();
    }

private:
    DirectionalLight	directionalLight;
    std::vector<PointLight> pointLights;
    std::vector<SpotLight> spotLights;
};


// ===========================
// LightManager
// ===========================
class LightComponent : public Component
{
public:
    enum class Type { Directional, Point, Spot };
    Type type = Type::Point;

    DirectX::XMFLOAT3 color{ 1, 1, 1 };
    float intensity = 1.0f;
    float range = 10.0f;

    bool showDebugBounds = false;

    // --- 追加：オフセットとローカル回転 ---
    DirectX::XMFLOAT3 offsetPosition{ 0, 0, 0 };
    DirectX::XMFLOAT3 localRotation{ 0, 0, 0 }; // オイラー角（度数法）

    // スポットライト用
    float innerAngle = 20.0f;
    float outerAngle = 30.0f;

    // Managerに渡すための最終的な計算結果
    DirectX::XMFLOAT3 finalWorldPosition{ 0, 0, 0 };
    DirectX::XMFLOAT3 finalWorldDirection{ 0, -1, 0 };

    virtual const std::string& GetName() const override
    {
        static std::string name = "LightComponent";
        return name;
    }

    void Update(float et) override;
   

    void Gui() override
    {
        const char* types[] = { "Directional", "Point", "Spot" };
        int currentType = static_cast<int>(type);
        if (ImGui::Combo("Light Type", &currentType, types, IM_ARRAYSIZE(types)))
        {
            type = static_cast<Type>(currentType);
        }

        ImGui::Separator();

        // オフセット位置の編集
        if (type != Type::Directional) // Directionalは位置関係なし
        {
            ImGui::DragFloat3("Offset Position", &offsetPosition.x, 0.1f);
        }

        // ローカル回転の編集
        if (type == Type::Directional || type == Type::Spot)
        {
            ImGui::DragFloat3("Local Rotation", &localRotation.x, 0.5f);
        }

        

        ImGui::Separator();

        ImGui::ColorEdit3("Light Color", &color.x);
        ImGui::DragFloat("Intensity", &intensity, 0.1f, 0.0f, 100.0f);

        if (type == Type::Point || type == Type::Spot)
        {
            ImGui::DragFloat("Range", &range, 0.1f, 0.0f, 1000.0f);
        }
        if (type == Type::Point)
        {
            ImGui::Checkbox("showDebugBounds", &showDebugBounds);
        }
        if (type == Type::Spot)
        {
            if (ImGui::TreeNode("Spotlight Settings"))
            {
                ImGui::DragFloat("Inner Angle", &innerAngle, 0.5f, 0.0f, outerAngle);
                ImGui::DragFloat("Outer Angle", &outerAngle, 0.5f, innerAngle, 180.0f);
                ImGui::TreePop();
            }
        }
    }

    void Serialize(nlohmann::json& j) const override {
        // 共通
        j["Type"] = static_cast<int>(type);
        j["Color"] = { color.x, color.y, color.z };
        j["Intensity"] = intensity;
        j["Range"] = range;

        // オフセット・回転
        j["OffsetPosition"] = { offsetPosition.x, offsetPosition.y, offsetPosition.z };
        j["LocalRotation"] = { localRotation.x, localRotation.y, localRotation.z };

        // スポットライト固有
        j["InnerAngle"] = innerAngle;
        j["OuterAngle"] = outerAngle;
    }

    void Deserialize(const nlohmann::json& j) override {
        // ライトの種類
        if (j.contains("Type")) {
            type = static_cast<Type>(j["Type"].get<int>());
        }

        // 色と基本パラメータ
        if (j.contains("Color")) {
            color.x = j["Color"][0];
            color.y = j["Color"][1];
            color.z = j["Color"][2];
        }
        intensity = j.value("Intensity", 1.0f);
        range = j.value("Range", 10.0f);

        // オフセット位置
        if (j.contains("OffsetPosition")) {
            offsetPosition.x = j["OffsetPosition"][0];
            offsetPosition.y = j["OffsetPosition"][1];
            offsetPosition.z = j["OffsetPosition"][2];
        }

        // ローカル回転
        if (j.contains("LocalRotation")) {
            localRotation.x = j["LocalRotation"][0];
            localRotation.y = j["LocalRotation"][1];
            localRotation.z = j["LocalRotation"][2];
        }

        // スポットライト角度
        innerAngle = j.value("InnerAngle", 20.0f);
        outerAngle = j.value("OuterAngle", 30.0f);
    }
};
#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"

class GltfModelCommponennt;

class ModelAttachmentComponent : public Component
{
public:
    ModelAttachmentComponent() = default;

    const std::string& GetName() const override {
        static std::string name = "ModelAttachment";
        return name;
    }

    // アタッチ先のノード名を設定
    void SetTargetNode(std::shared_ptr<GltfModelCommponennt> modelComp, const std::string& nodeName);
    void LateUpdate(float deltaTime) override;
    void Gui() override;

    // セーブ用：引数の json オブジェクトに自分の変数を書き込む
    void Serialize(nlohmann::json& j) const;
    // ロード用：引数の json オブジェクトから自分の変数を読み出す
    void Deserialize(const nlohmann::json& j);
private:
    std::weak_ptr<GltfModelCommponennt> targetModel;
    std::string targetGameObjectName; // アタッチ先GameObjectの名前（保存用）
    std::string targetNodeName;

    // --- ポップアップでの一時選択用 (temp) ---
    std::shared_ptr<GltfModelCommponennt> tempTargetModel; // モデルの仮置き

    int targetNodeIndex = -1;
    int tempSelectedIdx = -1;
};
#include "ModelAttachmentComponent.h"
#include "Engine/Graphics/Resource/GlTFModel.h"

#include <directxmath.h>
#include "Engine/GamePlay/Transform/Transform.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"


DirectX::XMFLOAT3 QuaternionToEuler(const DirectX::XMFLOAT4& q_in)
{
    DirectX::XMFLOAT3 euler;

    // 数式で計算するために各成分を取り出す
    float x = q_in.x;
    float y = q_in.y;
    float z = q_in.z;
    float w = q_in.w;

    // Pitch (x-axis rotation)
    float sinp = 2.0f * (w * x - y * z);
    if (std::abs(sinp) >= 1.0f)
        euler.x = std::copysign(DirectX::XM_PIDIV2, sinp); // 90度でロック（ジンバルロック対策）
    else
        euler.x = std::asin(sinp);

    // Yaw (y-axis rotation)
    float siny_cosp = 2.0f * (w * y + z * x);
    float cosy_cosp = 1.0f - 2.0f * (x * x + y * y);
    euler.y = std::atan2(siny_cosp, cosy_cosp);

    // Roll (z-axis rotation)
    float sinr_cosp = 2.0f * (w * z + x * y);
    float cosr_cosp = 1.0f - 2.0f * (z * z + x * x);
    euler.z = std::atan2(sinr_cosp, cosr_cosp);

    // ラジアンから度数法(Degrees)へ変換
    euler.x = DirectX::XMConvertToDegrees(euler.x);
    euler.y = DirectX::XMConvertToDegrees(euler.y);
    euler.z = DirectX::XMConvertToDegrees(euler.z);

    return euler;
}

void ModelAttachmentComponent::SetTargetNode(std::shared_ptr<GltfModelCommponennt> modelComp, const std::string& nodeName)
{
    targetModel = modelComp;
    targetStartNodeName = nodeName;
    targetNodeIndex = -1; // インデックスをリセットして再検索を促す

    if (modelComp) {
        if (auto targetGO = modelComp->GetGameObject()) {
            targetGameObjectName = targetGO->GetName();
        }
    }
}
std::shared_ptr<GltfModelCommponennt> ModelAttachmentComponent::ResolveTargetModel()
{
    auto model = targetModel.lock();
    if (model) return model;

    auto ownerGO = GetGameObject();
    if (!ownerGO) return nullptr;

    if (!targetGameObjectName.empty() && ownerGO->manager) {
        auto targetObject = ownerGO->GetManager().Find(targetGameObjectName);
        if (targetObject) {
            model = targetObject->GetComponent<GltfModelCommponennt>();
            if (model) {
                targetModel = model;
                return model;
            }
        }
    }

    auto currentParent = ownerGO->transform->parent.lock();
    while (currentParent) {
        auto parentGO = currentParent->gameObject.lock();
        if (parentGO) {
            model = parentGO->GetComponent<GltfModelCommponennt>();
            if (model) {
                targetModel = model;
                targetGameObjectName = parentGO->GetName();
                return model;
            }
        }
        currentParent = currentParent->parent.lock();
    }

    return nullptr;
}

bool ModelAttachmentComponent::ResolveTargetNodeIndex(const std::shared_ptr<GltfModelCommponennt>& model)
{
    if (!model) {
        targetNodeIndex = -1;
        return false;
    }

    const auto& nodes = model->GetNodes();
    const bool hasValidIndex = targetNodeIndex >= 0 && targetNodeIndex < static_cast<int>(nodes.size());

    if (!targetStartNodeName.empty()) {
        if (hasValidIndex && nodes[targetNodeIndex].name == targetStartNodeName) {
            return true;
        }

        for (int i = 0; i < static_cast<int>(nodes.size()); ++i) {
            if (nodes[i].name == targetStartNodeName) {
                targetNodeIndex = i;
                return true;
            }
        }
    }

    if (hasValidIndex) {
        if (targetStartNodeName.empty()) {
            targetStartNodeName = nodes[targetNodeIndex].name;
        }
        return true;
    }

    targetNodeIndex = -1;
    return false;
}

void ModelAttachmentComponent::Update(float deltaTime_)
{
    (void)deltaTime_;
    SyncAttachment();
}

void ModelAttachmentComponent::LateUpdate(float deltaTime_)
{
    (void)deltaTime_;
    SyncAttachment();
}

void ModelAttachmentComponent::SyncAttachment()
{
    auto myGo_ = GetGameObject();
    if (!myGo_) return;

    auto myTransform_ = myGo_->transform;
    auto model_ = ResolveTargetModel();
    if (model_)
    {
        model_->UpdateTransform();
    }

    if (ResolveTargetNodeIndex(model_)) {
        const auto& node_ = model_->GetNodes()[targetNodeIndex];
        DirectX::XMMATRIX nodeMat_ = DirectX::XMLoadFloat4x4(&node_.worldTransform);

        // Transform に「これを使って！」と渡す
        myTransform_->SetParentOverride(true, nodeMat_);
        myTransform_->UpdateWorldMatrix();

        if (auto attachedModel_ = myGo_->GetComponent<GltfModelCommponennt>())
        {
            attachedModel_->UpdateTransform();
        }
    }
    else {
        myTransform_->SetParentOverride(false, DirectX::XMMatrixIdentity());
        myTransform_->UpdateWorldMatrix();
    }
}
// --- 再帰的なツリー表示関数 (引数に const 修飾などを追加) ---
void ShowNodeTree(const GltfModelCommponennt::Node* node,
    const std::vector<GltfModelCommponennt::Node>& allNodes,
    int& tempSelectedIdx) {
    if (!node) return;

    int nodeIdx = static_cast<int>(node - allNodes.data());

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;
    if (tempSelectedIdx == nodeIdx) flags |= ImGuiTreeNodeFlags_Selected;
    if (node->children.empty()) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

    std::string label = (node->name.empty() ? "Node_" + std::to_string(nodeIdx) : node->name);

    bool isOpen = ImGui::TreeNodeEx((void*)(intptr_t)nodeIdx, flags, "%s", label.c_str());

    if (ImGui::IsItemClicked()) {
        tempSelectedIdx = nodeIdx;
    }

    if (isOpen && !node->children.empty()) {
        for (auto* child : node->children) {
            ShowNodeTree(child, allNodes, tempSelectedIdx);
        }
        ImGui::TreePop();
    }
}

void ShowNodeTree(const GltfModelCommponennt::Node* node,
    const std::vector<GltfModelCommponennt::Node>& allNodes,
    int& tempSelectedIdx,
    std::shared_ptr<GltfModelCommponennt>& tempTargetModel,
    const std::shared_ptr<GltfModelCommponennt>& currentProcessingModel)
{
    // 1. ポインタの妥当性確認
    if (!node) return;

    // 2. インデックスを安全に特定 (allNodes の範囲内にあるか確認)
    auto it = std::find_if(allNodes.begin(), allNodes.end(),
        [node](const GltfModelCommponennt::Node& n) { return &n == node; });

    if (it == allNodes.end()) return; // 範囲外なら何もしない
    int nodeIdx = static_cast<int>(std::distance(allNodes.begin(), it));

    //// node からインデックスを逆算する際、ポインタの計算が正しいか確認
    //int nodeIdx = static_cast<int>(node - allNodes.data());

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (tempTargetModel == currentProcessingModel && tempSelectedIdx == nodeIdx) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    // 子がいなければ Leaf
    if (node->children.empty()) flags |= ImGuiTreeNodeFlags_Leaf;

    std::string label = (node->name.empty() ? "Node_" + std::to_string(nodeIdx) : node->name);

    // ツリーのノード描画
    bool isOpen = ImGui::TreeNodeEx((void*)(intptr_t)nodeIdx, flags, "%s", label.c_str());

    // クリック判定 (IDを工夫して誤動作を防ぐ)
    if (ImGui::IsItemClicked()) {
        tempSelectedIdx = nodeIdx;
        tempTargetModel = currentProcessingModel;
    }

    // 子の再帰処理
    if (isOpen) {
        if (!node->children.empty()) {
            for (auto* child : node->children) {
                ShowNodeTree(child, allNodes, tempSelectedIdx, tempTargetModel, currentProcessingModel);
            }
        }
        ImGui::TreePop();
    }
}

void ModelAttachmentComponent::Gui()
{
    // --- 現在の状態表示 ---
    auto currentModel = targetModel.lock();
    const auto* currentNodes = currentModel ? &currentModel->GetNodes() : nullptr;
    bool hasCurrentNode = currentNodes && targetNodeIndex >= 0 && targetNodeIndex < static_cast<int>(currentNodes->size());
    std::string currentLabel = hasCurrentNode
        ? (*currentNodes)[targetNodeIndex].name
        : "(None)";

    ImGui::Text("Attachment Node: %s", currentLabel.c_str());

    if (ImGui::Button("Select Node from Scene...")) {
        ImGui::OpenPopup("GlobalNodeSelectorPopup");
    }

    // --- ポップアップウィンドウ ---
    if (ImGui::BeginPopupModal("GlobalNodeSelectorPopup", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Select a Node from Parent Models:");
        ImGui::Separator();

        ImGui::BeginChild("SelectorScroll", ImVec2(500, 400), true);

        // --- 修正：自身の親階層を遡り、モデルコンポーネントを探す ---
        auto ownerGO = GetGameObject();
        std::shared_ptr<Transform> currentParent = ownerGO->transform->parent.lock();

        bool foundAny = false;
        while (currentParent) {
            auto parentGO = currentParent->gameObject.lock();
            if (parentGO) {
                auto modelComp = parentGO->GetComponent<GltfModelCommponennt>();
                if (modelComp) {
                    foundAny = true;
                    // 親モデルのノードツリーを表示
                    ImGui::PushID(parentGO.get());
                    if (ImGui::TreeNode(parentGO->GetName().c_str())) {
                        const auto& nodes = modelComp->GetNodes();
                        for (const auto& node : nodes) {
                            if (node.parent == nullptr) {
                                ShowNodeTree(&node, nodes, tempSelectedIdx, tempTargetModel, modelComp);
                            }
                        }
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }
            }
            // さらにその親へ遡る
            currentParent = currentParent->parent.lock();
        }

        if (!foundAny) {
            ImGui::TextDisabled("No model found in parent hierarchy.");
        }

        ImGui::EndChild();
        ImGui::Separator();

        // --- 確定・キャンセル ---
        if (ImGui::Button("OK", ImVec2(120, 0))) {
            targetModel = tempTargetModel;
            targetNodeIndex = tempSelectedIdx;
            // 決定時に名前をキャッシュしておく（ノード検索用）
            if (auto m = targetModel.lock()) {
                const auto& nodes = m->GetNodes();
                if (targetNodeIndex >= 0 && targetNodeIndex < static_cast<int>(nodes.size())) {
                    targetStartNodeName = nodes[targetNodeIndex].name;
                }

                targetGameObjectName.clear();
                if (auto targetGO = m->GetGameObject()) {
                    targetGameObjectName = targetGO->GetName();
                }
                else if (auto ownerGO = GetGameObject()) {
                    auto currentParent = ownerGO->transform->parent.lock();
                    while (currentParent) {
                        auto parentGO = currentParent->gameObject.lock();
                        if (parentGO && parentGO->GetComponent<GltfModelCommponennt>() == m) {
                            targetGameObjectName = parentGO->GetName();
                            break;
                        }
                        currentParent = currentParent->parent.lock();
                    }
                }
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void ModelAttachmentComponent::Serialize(nlohmann::json& j) const
{
    // 名前とインデックスを保存
    j["targetStartNodeName"] = targetStartNodeName;
    j["targetGameObjectName"] = targetGameObjectName;
    j["targetNodeIndex"] = targetNodeIndex;
}

void ModelAttachmentComponent::Deserialize(const nlohmann::json& j)
{
    // 名前を優先的に復元
    if (j.contains("targetStartNodeName")) {
        targetStartNodeName = j.at("targetStartNodeName").get<std::string>();
    }

    // 名前を優先的に復元
    if (j.contains("targetGameObjectName")) {
        targetGameObjectName = j.at("targetGameObjectName").get<std::string>();
    }

    // インデックスも読み出すが、モデルロード後に名前から再検索することを推奨
    if (j.contains("targetNodeIndex")) {
        targetNodeIndex = j.at("targetNodeIndex").get<int>();
    }
}

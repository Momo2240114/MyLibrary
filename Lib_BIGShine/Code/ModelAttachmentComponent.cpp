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
    targetNodeName = nodeName;
    targetNodeIndex = -1; // インデックスをリセットして再検索を促す
}

void ModelAttachmentComponent::LateUpdate(float deltaTime)
{
    auto myGO = GetGameObject();
    auto myTransform = myGO->transform;

    // ターゲットモデルの取得
    auto model = targetModel.lock();
    if (!model) {
        // 親がモデルを持っている前提で取得を試みる
        ////targetModel = parentTransform->gameObject.lock()->GetComponent<GltfModelCommponennt>();
        auto targetObject = GameObjectManager::Instance().Find(targetGameObjectName.c_str());
        if(targetObject)
        {
            targetModel = targetObject->GetComponent<GltfModelCommponennt>();
            model = targetModel.lock();
        }
    }

    if (!model || targetNodeIndex == -1) return;

    auto& nodes = model->GetNodes();
    if (targetNodeIndex >= (int)nodes.size()) return;

    // 以前解決した「親の二重足し算」を防ぐための pureOffsetMat 抽出
    DirectX::XMMATRIX nodeMat = DirectX::XMLoadFloat4x4(&nodes[targetNodeIndex].worldTransform);
    auto modelTransform = model->GetGameObject()->transform;

    // 親（モデル自身）のワールド行列を逆行列にして掛ける
    DirectX::XMMATRIX modelWorldMat = modelTransform->GetWorldMatrix();
    DirectX::XMVECTOR det;
    DirectX::XMMATRIX invModelWorldMat = DirectX::XMMatrixInverse(&det, modelWorldMat);

    // これで「モデルの原点から見たノードの純粋なズレ」が出る
    DirectX::XMMATRIX relativeMat = nodeMat * invModelWorldMat;

    DirectX::XMVECTOR s, r, t;
    if (DirectX::XMMatrixDecompose(&s, &r, &t, relativeMat)) {
        DirectX::XMStoreFloat3(&myTransform->localPosition,t);

        // スケールの適用
        DirectX::XMStoreFloat3(&myTransform->localScale, s);

        // 回転の適用 (クォータニオンをオイラー角に変換)
        DirectX::XMFLOAT4 q;
        DirectX::XMStoreFloat4(&q, r);
        myTransform->localRotation = QuaternionToEuler(q);

        myTransform->MarkDirty();
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
    if (!node) return;
    int nodeIdx = static_cast<int>(node - allNodes.data());

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;

    // モデルとインデックスの両方が一致する場合のみ Selected フラグを立てる
    if (tempTargetModel == currentProcessingModel && tempSelectedIdx == nodeIdx) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    if (node->children.empty()) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

    std::string label = (node->name.empty() ? "Node_" + std::to_string(nodeIdx) : node->name);

    bool isOpen = ImGui::TreeNodeEx((void*)(intptr_t)nodeIdx, flags, "%s", label.c_str());

    if (ImGui::IsItemClicked()) {
        tempSelectedIdx = nodeIdx;
    }

    if (ImGui::IsItemClicked()) {
        tempSelectedIdx = nodeIdx;
        tempTargetModel = currentProcessingModel; // どのモデルのノードか記録
    }

    if (isOpen && !node->children.empty()) {
        for (auto* child : node->children) {
            ShowNodeTree(child, allNodes, tempSelectedIdx);
        }
        ImGui::TreePop();
    }

}

void ModelAttachmentComponent::Gui()
{
    // --- 現在の状態表示 ---
    auto currentModel = targetModel.lock();
    std::string currentLabel = (currentModel && targetNodeIndex != -1)
        ? currentModel->GetNodes()[targetNodeIndex].name
        : "(None)";

    ImGui::Text("Attachment Node: %s", currentLabel.c_str());

    if (ImGui::Button("Select Node from Scene...")) {
        ImGui::OpenPopup("GlobalNodeSelectorPopup");
    }

    // --- ポップアップウィンドウ ---
    if (ImGui::BeginPopupModal("GlobalNodeSelectorPopup", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Select a Model and its Node:");
        ImGui::Separator();

        ImGui::BeginChild("SelectorScroll", ImVec2(500, 400), true);

        // 1. GameObjectManager から全オブジェクトを取得
        auto& allObjects = GameObjectManager::Instance().GetObjects();

        for (auto& go : allObjects) {
            if (!go) continue;

            // 2. そのオブジェクトが GltfModelComponent を持っているか確認
            // ※Tはプロジェクトで定義しているモデルコンポーネント名に書き換えてください
            auto modelComp = go->GetComponent<GltfModelCommponennt>();
            if (!modelComp) continue;

            // 3. オブジェクト名をツリーの親にする
            ImGui::PushID(go.get());
            if (ImGui::TreeNode(go->GetName().c_str())) {
                const auto& nodes = modelComp->GetNodes();

                // 4. モデル内の全ノードを走査
                for (const auto& node : nodes) {
                    // ルートノードから再帰開始（親がいないノードを起点にする）
                    if (node.parent == nullptr) {
                        ShowNodeTree(&node, nodes, tempSelectedIdx, tempTargetModel, modelComp);
                    }
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }

        ImGui::EndChild();
        ImGui::Separator();

        // --- 確定・キャンセル ---
        if (ImGui::Button("OK", ImVec2(120, 0))) {
            targetModel = tempTargetModel;
            targetNodeIndex = tempSelectedIdx;
            // 決定時に名前をキャッシュしておく（ノード検索用）
            if (auto m = targetModel.lock()) {
                targetNodeName = m->GetNodes()[targetNodeIndex].name;
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
    j["targetNodeName"] = targetNodeName;
    j["targetGameObjectName"] = targetGameObjectName;
    j["targetNodeIndex"] = targetNodeIndex;
}

void ModelAttachmentComponent::Deserialize(const nlohmann::json& j)
{
    // 名前を優先的に復元
    if (j.contains("targetNodeName")) {
        targetNodeName = j.at("targetNodeName").get<std::string>();
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
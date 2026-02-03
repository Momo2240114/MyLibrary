#include "ModelAttachmentComponent.h"
#include "GlTFModel.h"

#include <directxmath.h>
#include "Transform.h"


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
    auto parentTransform = myTransform->parent.lock();
    if (!parentTransform) return;

    // ターゲットモデルの取得
    auto model = targetModel.lock();
    if (!model) {
        // 親がモデルを持っている前提で取得を試みる
        targetModel = parentTransform->gameObject.lock()->GetComponent<GltfModelCommponennt>();
        model = targetModel.lock();
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
void ShowNodeTree(const GltfModelCommponennt::Node* node, const std::vector<GltfModelCommponennt::Node>& allNodes, int& tempSelectedIdx) {
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


void ModelAttachmentComponent::Gui()
{

    auto model = targetModel.lock();
    if (!model) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Target Model is not set!");
        return;
    }

    auto& nodes = model->GetNodes();
    const char* previewName = (targetNodeIndex >= 0 && targetNodeIndex < (int)nodes.size())
        ? nodes[targetNodeIndex].name.c_str() : "(None)";

    ImGui::Text("Target Node: %s", previewName);

    // --- ノード選択を開始するボタン ---
    if (ImGui::Button("Select Attachment Node...")) {
        ImGui::OpenPopup("NodeSelectorPopup");
        // ポップアップを開くときに一時的な選択用インデックスを初期化
        tempSelectedIdx = targetNodeIndex;
    }

    // --- ポップアップウィンドウの設定 ---
    if (ImGui::BeginPopupModal("NodeSelectorPopup", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Select a node from the hierarchy:");
        ImGui::Separator();

        // スクロールエリアの確保
        ImGui::BeginChild("TreeChild", ImVec2(600, 400), true);

        // ルートノードを探して描画（親がいないノードを起点にする）
        // ※GLTFモデル構造に合わせ、ルートを順に描画
        for (const auto& node : nodes) {
            // モデル全体のルートのみを起点とする場合、親子関係の構築ロジックに合わせて呼び出し
            // 今回は簡易的に「allNodes[0]」などを起点にするか、全ノードをスキャン
            // ここでは仮に最初の数件がルートであると想定、または全部表示
            if (/* nodeがルートである条件 */ true) {
                // ShowNodeTree を呼び出し（モデル構造に合わせて調整）
            }
        }
        // 簡易実装用：全ノードから親を持たないものを探して表示
        for (int i = 0; i < (int)nodes.size(); ++i) {
            // node->parent が nullptr のものだけを ShowNodeTree の起点にする
            // (GltfModelの構造に依存します)
        }

        // --- 実装例: allNodes[0] をルートとする場合 ---
        ShowNodeTree(&nodes[0], nodes, tempSelectedIdx);

        ImGui::EndChild();

        ImGui::Separator();

        // --- 確定・キャンセルボタン ---
        if (ImGui::Button("OK", ImVec2(120, 0))) {
            targetNodeIndex = tempSelectedIdx;
            if (targetNodeIndex != -1) targetNodeName = nodes[targetNodeIndex].name;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    if (targetNodeIndex != -1) {
        ImGui::Text("Node Index: %d", targetNodeIndex);
    }
}

void ModelAttachmentComponent::Serialize(nlohmann::json& j) const
{
    // 名前とインデックスを保存
    j["targetNodeName"] = targetNodeName;
    j["targetNodeIndex"] = targetNodeIndex;
}

void ModelAttachmentComponent::Deserialize(const nlohmann::json& j)
{
    // 名前を優先的に復元
    if (j.contains("targetNodeName")) {
        targetNodeName = j.at("targetNodeName").get<std::string>();
    }

    // インデックスも読み出すが、モデルロード後に名前から再検索することを推奨
    if (j.contains("targetNodeIndex")) {
        targetNodeIndex = j.at("targetNodeIndex").get<int>();
    }
}
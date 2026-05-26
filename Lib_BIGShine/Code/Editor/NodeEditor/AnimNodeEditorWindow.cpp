//#include "AnimNodeEditorWindow.h"
//#include "Engine/GamePlay/Core/Object/GameObject.h"
//#include "Engine/GamePlay/Core/Object/GameObjectManager.h" 
//#include "Engine/Graphics/Resource/GlTFModel.h" 
//#include "Engine/Graphics/Resource/Animation/AnimationControllerComponent.h"
//#include "Engine/Graphics/Resource/Animation/PoseLibraryResourceManager.h"
//#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
//#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
//#include <algorithm>
//
//AnimNodeEditorWindow::AnimNodeEditorWindow(const std::string& title, GameObjectManager* objManager)
//    : EditorWindow(title, 800, 600)
//    , m_pObjManager(objManager)
//{
//    m_nodeEditorContext = ed::CreateEditor(&m_nodeEditorConfig);
//}
//
//AnimNodeEditorWindow::~AnimNodeEditorWindow()
//{
//    if (m_nodeEditorContext)
//    {
//        ed::DestroyEditor(m_nodeEditorContext);
//        m_nodeEditorContext = nullptr;
//    }
//}
//
//uintptr_t AnimNodeEditorWindow::GetNextId()
//{
//    if (m_targetController)
//    {
//        return m_targetController->GetNextID();
//    }
//    return 0;
//}
//
//void AnimNodeEditorWindow::Update(float deltaTime)
//{
//    if (!m_pObjManager) return;
//
//    auto activeObj = m_pObjManager->GetSelectedObject();
//    if (activeObj)
//    {
//        auto animController = activeObj->GetComponent<AnimationControllerComponent>();
//        if (animController)
//        {
//            // ★修正：コンポーネントのポインタではなく、ゲームオブジェクトのポインタそのものが変わったかで判定する
//            // もしくは、ターゲットが変わっていないならフラグを立てないようにガードを徹底する
//            if (m_targetController != animController.get())
//            {
//                m_targetController = animController.get();
//                m_needRestorePositions = true; // オブジェクトが本当に切り替わった時だけ1回走る
//            }
//        }
//        else
//        {
//            m_targetController = nullptr;
//            m_needRestorePositions = false;
//        }
//    }
//    else
//    {
//        m_targetController = nullptr;
//        m_needRestorePositions = false;
//    }
//}
//
//
//void AnimNodeEditorWindow::RenderScene(ID3D11DeviceContext* context) {}
//
//void AnimNodeEditorWindow::DrawGUI()
//{
//    ImGui::SetNextWindowSize(ImVec2((float)m_width, (float)m_height), ImGuiCond_FirstUseEver);
//
//    if (!ImGui::Begin(m_title.c_str(), &m_isOpen))
//    {
//        ImGui::End();
//        return;
//    }
//
//    if (!m_targetController)
//    {
//        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "SelectObject Has AnimationControllerComponent");
//        ImGui::End();
//        return;
//    }
//
//    ImGui::Columns(2, "EditorColumns", true);
//
//    DrawCanvas();
//
//    ImGui::NextColumn();
//
//    DrawInspector();
//
//    ImGui::Columns(1);
//    ImGui::End();
//}
//
//
//void AnimNodeEditorWindow::DrawCanvas()
//{
//    ed::SetCurrentEditor(m_nodeEditorContext);
//    ed::Begin("NodeCanvas");
//
//    if (!m_targetController) { ed::End(); ed::SetCurrentEditor(nullptr); return; }
//
//    auto& graphNodes = m_targetController->GetGraphNodes();
//    auto& states = m_targetController->GetStates();
//
//    // 1. 位置の復元（オブジェクト切り替え時の1回のみ実行される）
//    if (m_needRestorePositions)
//    {
//        for (auto& node : graphNodes)
//        {
//            ed::SetNodePosition(node.id, node.pos);
//        }
//        ed::NavigateToContent(0.0f);
//        m_needRestorePositions = false;
//    }
//
//    // 2. すべてのノードを描画する
//    for (auto& node : graphNodes)
//    {
//        ed::BeginNode(node.id);
//
//        if (node.stateName.empty()) {
//            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "[Empty Node]");
//        }
//        else {
//            ImGui::Text("State: %s", node.stateName.c_str());
//        }
//
//        ImGui::Spacing();
//
//        // 入力ピン
//        ed::BeginPin(node.inputPin, ed::PinKind::Input);
//        ImGui::Text("-> In");
//        ed::EndPin();
//
//        ImGui::SameLine();
//        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 40);
//
//        // 出力ピン
//        ed::BeginPin(node.outputPin, ed::PinKind::Output);
//        ImGui::Text("Out ->");
//        ed::EndPin();
//
//        ImGui::Spacing();
//
//        // ドロップダウン（ステート割り当て）
//        std::vector<const char*> stateNames;
//        stateNames.push_back("--- None  ---");
//        int currentIdx = 0;
//        int counter = 1;
//        for (const auto& pair : states) {
//            stateNames.push_back(pair.first.c_str());
//            if (pair.first == node.stateName) currentIdx = counter;
//            counter++;
//        }
//
//        ImGui::PushItemWidth(120.0f);
//        std::string comboId = "##Combo_" + std::to_string(node.id.Get());
//        if (ImGui::Combo(comboId.c_str(), &currentIdx, stateNames.data(), (int)stateNames.size()))
//            node.stateName = (currentIdx == 0) ? "" : stateNames[currentIdx];
//        ImGui::PopItemWidth();
//
//        ed::EndNode();
//    }
//
//    // 3. 遷移リンクの描画
//    auto& graphTransitions = m_targetController->GetTransitions();
//    int baseLinkId = 6000; // ベースとなるID
//
//    // ★修正：リンクのIDを「配列のインデックス」と紐付けることで一意に固定する
//    for (size_t i = 0; i < graphTransitions.size(); ++i)
//    {
//        auto& trans = graphTransitions[i];
//        auto fromNode = std::find_if(graphNodes.begin(), graphNodes.end(), [&](const AnimGraphNode& n) { return n.stateName == trans.fromStateName; });
//        auto toNode = std::find_if(graphNodes.begin(), graphNodes.end(), [&](const AnimGraphNode& n) { return n.stateName == trans.toStateName; });
//
//        if (fromNode != graphNodes.end() && toNode != graphNodes.end())
//        {
//            if (!fromNode->stateName.empty() && !toNode->stateName.empty())
//            {
//                // baseLinkId にインデックスを足したものをLinkIdとする
//                ed::Link(ed::LinkId(baseLinkId + i), fromNode->outputPin, toNode->inputPin);
//            }
//        }
//    }
//
//    // 4. 選択状態の更新
//    // ★修正：ノードだけでなく、リンク(線)が選択された場合の判定も追加する
//    if (ed::GetSelectedObjectCount() > 0)
//    {
//        ed::NodeId selectedNodes[1];
//        ed::LinkId selectedLinks[1];
//
//        if (ed::GetSelectedNodes(selectedNodes, 1) > 0)
//        {
//            m_selectedNodeId = selectedNodes[0];
//            m_selectedLinkId = 0; // ノード選択時はリンク選択を解除
//        }
//        else if (ed::GetSelectedLinks(selectedLinks, 1) > 0)
//        {
//            m_selectedLinkId = selectedLinks[0];
//            m_selectedNodeId = 0; // リンク選択時はノード選択を解除
//        }
//    }
//    else
//    {
//        m_selectedNodeId = 0;
//        m_selectedLinkId = 0;
//    }
//
//    HandleCreation();
//    HandleDeletion();
//
//    ed::End();
//    ed::SetCurrentEditor(nullptr);
//}
//
//void AnimNodeEditorWindow::DrawInspector()
//{
//    ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "=== Inspector ===");
//    ImGui::Separator();
//
//    if (m_selectedNodeId.Get() == 0)
//    {
//        ImGui::TextWrapped("Node Selected Is Parameter");
//        return;
//    }
//
//    auto& graphNodes = m_targetController->GetGraphNodes();
//    auto& states = m_targetController->GetStates();
//
//    auto it = std::find_if(graphNodes.begin(), graphNodes.end(), [&](const AnimGraphNode& n) { return n.id == m_selectedNodeId; });
//    if (it == graphNodes.end())
//    {
//        ImGui::Text("don't get Node Data");
//        return;
//    }
//
//    ImGui::Text("Node ID: %llu", (unsigned long long)it->id.Get());
//    ImGui::Text("Linked State: %s", it->stateName.empty() ? "None" : it->stateName.c_str());
//    ImGui::Separator();
//
//    if (!it->stateName.empty() && states.find(it->stateName) != states.end())
//    {
//        auto& animState = states[it->stateName];
//        ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.3f, 1.0f), "State ");
//
//        ImGui::DragFloat("Playback Speed", &animState->playbackSpeed, 0.05f, 0.0f, 10.0f, "%.2f");
//        ImGui::Checkbox("Is Loop", &animState->isLoop);
//
//        char curveBuf[64];
//        strcpy_s(curveBuf, animState->speedCurveName.c_str());
//        if (ImGui::InputText("Speed Curve Name", curveBuf, sizeof(curveBuf))) {
//            animState->speedCurveName = curveBuf;
//        }
//    }
//    else
//    {
//        ImGui::TextDisabled("Assign an AnimState to this node to edit parameters.");
//    }
//}
//
//void AnimNodeEditorWindow::HandleCreation()
//{
//    ed::Suspend();
//
//    ed::PinId startPinId, endPinId;
//    if (ed::BeginCreate())
//    {
//        if (ed::QueryNewLink(&startPinId, &endPinId))
//        {
//            if (ed::AcceptNewItem())
//            {
//                auto& graphNodes = m_targetController->GetGraphNodes();
//
//                auto fromIt = std::find_if(graphNodes.begin(), graphNodes.end(), [&](const AnimGraphNode& n) {
//                    return n.outputPin == startPinId || n.inputPin == startPinId;
//                    });
//
//                auto toIt = std::find_if(graphNodes.begin(), graphNodes.end(), [&](const AnimGraphNode& n) {
//                    return n.outputPin == endPinId || n.inputPin == endPinId;
//                    });
//
//                if (fromIt != graphNodes.end() && toIt != graphNodes.end() && fromIt != toIt)
//                {
//                    std::string fromName = "";
//                    std::string toName = "";
//
//                    if (fromIt->outputPin == startPinId && toIt->inputPin == endPinId)
//                    {
//                        fromName = fromIt->stateName;
//                        toName = toIt->stateName;
//                    }
//                    else if (fromIt->inputPin == startPinId && toIt->outputPin == endPinId)
//                    {
//                        fromName = toIt->stateName;
//                        toName = fromIt->stateName;
//                    }
//
//                    if (!fromName.empty() && !toName.empty())
//                    {
//                        AnimTransition newTrans;
//                        newTrans.fromStateName = fromName;
//                        newTrans.toStateName = toName;
//                        newTrans.duration = 0.2f;
//
//                        m_targetController->GetTransitions().push_back(newTrans);
//                    }
//                }
//            }
//        }
//    }
//    ed::EndCreate();
//
//    if (ed::ShowBackgroundContextMenu())
//    {
//        ImGui::OpenPopup("CanvasContextMenu");
//    }
//
//    if (ImGui::BeginPopup("CanvasContextMenu"))
//    {
//        if (ImGui::MenuItem("Add Empty Node"))
//        {
//            AnimGraphNode newNode;
//            newNode.id = ed::NodeId(GetNextId());
//            newNode.inputPin = ed::PinId(GetNextId());
//            newNode.outputPin = ed::PinId(GetNextId());
//            newNode.stateName = "[Empty Node]"; // 交遊野の要求通り、初期設定も英語へと統一
//
//            ImVec2 canvasPos = ed::ScreenToCanvas(ImGui::GetMousePos());
//            newNode.pos = canvasPos;
//
//            ed::SetNodePosition(newNode.id, canvasPos);
//            m_targetController->GetGraphNodes().push_back(newNode);
//        }
//        ImGui::EndPopup();
//    }
//
//    ed::Resume();
//}
//
//void AnimNodeEditorWindow::HandleDeletion()
//{
//    if (ed::BeginDelete())
//    {
//        ed::NodeId deletedNodeId;
//        while (ed::QueryDeletedNode(&deletedNodeId))
//        {
//            if (ed::AcceptDeletedItem())
//            {
//                auto& graphNodes = m_targetController->GetGraphNodes();
//                graphNodes.erase(
//                    std::remove_if(graphNodes.begin(), graphNodes.end(), [&](const AnimGraphNode& n) { return n.id == deletedNodeId; }),
//                    graphNodes.end()
//                );
//            }
//        }
//    }
//    ed::EndDelete();
//}
//// =================================================================
//// AnimationEditorWindow.cpp (動的結合版・全貌)
//// =================================================================
//#include "AnimationEditorWindow.h"
//
//// あなたの世界の真実の定義群
//#include "Engine/GamePlay/Core/Object/GameObject.h"
//#include "Engine/GamePlay/Core/Object/GameObjectManager.h" //  【追加】マネージャーの定義
//#include "Engine/Graphics/Resource/GlTFModel.h" 
//#include "Engine/Graphics/Resource/Animation/AnimationControllerComponent.h"
//#include "Engine/Graphics/Resource/Animation/PoseLibraryResourceManager.h"
//#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
//#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
//#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
//
//#include <imgui.h>
//#include <imgui_stdlib.h>
//#include <algorithm>
//
////  コンストラクタ：独自のダミー生成を廃止し、マネージャーのポインタを結びつける
//AnimationEditorWindow::AnimationEditorWindow(const std::string& title, GameObjectManager* objManager)
//    : EditorWindow(title, 800, 600)
//    , m_pObjManager(objManager)
//{
//}
//
//// Update：現在階層（Hierarchy）で選択されているオブジェクトを毎フレーム監視する
//void AnimationEditorWindow::Update(float deltaTime)
//{
//    if (!m_pObjManager) return;
//
//    // 1. マネージャーから今選択されているオブジェクトを乞う
//    auto activeObj = m_pObjManager->GetSelectedObject();
//
//    // 2. 選択が変わった瞬間を検知する
//    if (activeObj != m_targetObject)
//    {
//        m_targetObject = activeObj;
//
//        // ターゲットが変わったら、古いオブジェクトのコンポーネントへの参照を安全にリセット
//        m_modelComp = nullptr;
//        m_animController = nullptr;
//        m_selectedPoseIdx = -1;
//        m_selectedAnimIdx = -1;
//
//        if (m_targetObject)
//        {
//            // 3. 新たに選ばれたオブジェクトから、エディタに必要なコンポーネントを安全に抽出
//            m_modelComp = m_targetObject->GetComponentByNameAs<GltfModelCommponennt>("GltfModel").get();
//            m_animController = m_targetObject->GetComponentByNameAs<AnimationControllerComponent>("AnimationControllerComponent").get();
//        }
//    }
//
//}
//
//void AnimationEditorWindow::RenderScene(ID3D11DeviceContext* context)
//{
//    // メインビュー（あるいはゲーム画面）で動いている姿をそのまま見るため、ここは空で問題ありません。
//}
//
//// 【変更】DrawGUI：対象が選択されていない、あるいはコンポーネントがない場合の防壁（ガード句）を構築
//void AnimationEditorWindow::DrawGUI()
//{
//    ImGui::Begin(m_title.c_str(), &m_isOpen);
//    {
//        // 安全の防壁：対象が存在しない場合の警告
//        if (!m_targetObject)
//        {
//            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "GameObjectManager (Hierarchy) からオブジェクトを選択してください...");
//            ImGui::End();
//            return;
//        }
//
//        //  安全の防壁：アニメーションを扱えないオブジェクトの場合の警告
//        if (!m_animController || !m_modelComp)
//        {
//            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "選択中の [%s] は以下のコンポーネントが不足しています：", m_targetObject->name.c_str());
//            if (!m_modelComp)      ImGui::Text(" -> GltfModel (肉体)");
//            if (!m_animController) ImGui::Text(" -> AnimationControllerComponent (魂)");
//            ImGui::End();
//            return;
//        }
//
//        // ここから下は、選ばれた実体（m_targetObject）のデータを用いてリアルタイムに動作します！
//        PoseLibraryResource* poseLib = m_animController->GetPoseLibrary().get();
//        if (!poseLib)
//        {
//            ImGui::Text("ポーズライブラリがロードされていません。");
//            ImGui::End();
//            return;
//        }
//
//        // 現在編集中のオブジェクト名を誇り高く表示
//        ImGui::Text("Editing Object: %s (Model: %s)", m_targetObject->name.c_str(), m_modelComp->GetModelName().c_str());
//        ImGui::Separator();
//
//        // 左右のスプリッター配置
//        ImGui::Columns(2, "AnimEditorColumns", true);
//
//        // 左パネルの描画
//        DrawLeftPanel(poseLib);
//        ImGui::NextColumn();
//
//        // 右パネル of 描画
//        DrawRightPanel(poseLib);
//        ImGui::Columns(1);
//    }
//    ImGui::End();
//}
//
////  ポーズのキャプチャ：選択された実体の肉体（m_modelComp）から直接ポーズを切り取る
//void AnimationEditorWindow::CaptureCurrentPose(PoseLibraryResource* poseLib)
//{
//    if (!m_modelComp || !poseLib) return;
//
//    SavedPose newPose;
//    char buf[64];
//    sprintf_s(buf, "Pose_%02d", (int)poseLib->poseLibrary.size());
//    newPose.name = buf;
//
//    // 選択されたモデルが持つ全ノードの数を取得してコンテナを拡張
//    const auto& currentNodes = m_modelComp->GetNodes();
//    newPose.nodePoses.resize(currentNodes.size());
//
//    //  画面上で弄ったオブジェクトの「現在の姿勢」をそのままキャプチャする儀式
//    for (size_t i = 0; i < currentNodes.size(); ++i)
//    {
//        newPose.nodePoses[i].position = currentNodes[i].position;
//        newPose.nodePoses[i].rotation = currentNodes[i].rotation;
//        newPose.nodePoses[i].scale = currentNodes[i].scale;
//    }
//
//    poseLib->poseLibrary.push_back(newPose);
//}
//
//
//void AnimationEditorWindow::DrawLeftPanel(PoseLibraryResource* poseLib)
//{
//    ImGui::Text("Pose Library");
//    if (ImGui::Button("Capture Current Pose"))
//    {
//        CaptureCurrentPose(poseLib);
//    }
//    ImGui::Separator();
//
//    ImGui::BeginChild("PoseListChild", ImVec2(0, 0), true);
//    for (int i = 0; i < (int)poseLib->poseLibrary.size(); ++i)
//    {
//        bool isSelected = (m_selectedPoseIdx == i);
//        if (ImGui::Selectable(poseLib->poseLibrary[i].name.c_str(), isSelected))
//        {
//            m_selectedPoseIdx = i;
//        }
//    }
//    ImGui::EndChild();
//}
//
//void AnimationEditorWindow::DrawRightPanel(PoseLibraryResource* poseLib)
//{
//    ImGui::Text("Custom Animations");
//    if (ImGui::Button("Create New Custom Anim"))
//    {
//        CustomAnimation newAnim;
//        char buf[64];
//        sprintf_s(buf, "CustomAnim_%02d", (int)poseLib->customAnims.size());
//        newAnim.name = buf;
//        newAnim.duration = 1.0f;
//        poseLib->customAnims.push_back(newAnim);
//    }
//    ImGui::Separator();
//
//    ImGui::BeginChild("AnimListChild", ImVec2(0, 150), true);
//    for (int i = 0; i < (int)poseLib->customAnims.size(); ++i)
//    {
//        bool isSelected = (m_selectedAnimIdx == i);
//        if (ImGui::Selectable(poseLib->customAnims[i].name.c_str(), isSelected))
//        {
//            m_selectedAnimIdx = i;
//        }
//    }
//    ImGui::EndChild();
//
//    ImGui::Separator();
//    ImGui::Text("Timeline / Keyframes");
//
//    if (m_selectedAnimIdx < 0 || m_selectedAnimIdx >= (int)poseLib->customAnims.size())
//    {
//        ImGui::Text("Custom Animation を一覧から選択してください。");
//        return;
//    }
//
//    auto& targetAnim = poseLib->customAnims[m_selectedAnimIdx];
//
//    ImGui::InputText("Anim Name", &targetAnim.name);
//    ImGui::DragFloat("Duration", &targetAnim.duration, 0.1f, 0.1f, 60.0f, "%.2f s");
//
//    if (ImGui::Button("Add Keyframe from Selected Pose"))
//    {
//        if (m_selectedPoseIdx >= 0 && m_selectedPoseIdx < (int)poseLib->poseLibrary.size())
//        {
//            PoseKeyframe kf;
//            kf.time = 0.0f;
//            kf.poseIndex = m_selectedPoseIdx;
//            kf.sourceAnimIdx = 0;
//            kf.sourceTime = 0.0f;
//            targetAnim.keyframes.push_back(kf);
//        }
//    }
//
//    ImGui::BeginChild("TimelineChild", ImVec2(0, 0), true);
//    {
//        int kfToErase = -1;
//        for (int k = 0; k < (int)targetAnim.keyframes.size(); ++k)
//        {
//            auto& kf = targetAnim.keyframes[k];
//            ImGui::PushID(k);
//
//            std::string label = "Unk";
//            if (kf.poseIndex >= 0 && kf.poseIndex < (int)poseLib->poseLibrary.size())
//            {
//                label = poseLib->poseLibrary[kf.poseIndex].name;
//            }
//
//            ImGui::Text("[%s]", label.c_str());
//            ImGui::SameLine();
//            ImGui::SetNextItemWidth(100.0f);
//            ImGui::DragFloat("Time", &kf.time, 0.01f, 0.0f, targetAnim.duration, "%.2f s");
//            ImGui::SameLine();
//            if (ImGui::Button("X")) kfToErase = k;
//            ImGui::PopID();
//        }
//
//        if (kfToErase != -1)
//        {
//            targetAnim.keyframes.erase(targetAnim.keyframes.begin() + kfToErase);
//        }
//
//        std::sort(targetAnim.keyframes.begin(), targetAnim.keyframes.end(), [](const PoseKeyframe& a, const PoseKeyframe& b) {
//            return a.time < b.time;
//            });
//    }
//    ImGui::EndChild();
//}
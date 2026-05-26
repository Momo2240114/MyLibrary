//#pragma once
//#include "../Window/EditorWindow.h"
//#include "imgui_node_editor.h"
//#include <vector>
//#include <string>
//#include <memory>
//
//// 前方宣言：コントローラーの存在を窓の世界に伝える
//class GameObject;
//class GameObjectManager;
//class GltfModelCommponennt;
//class AnimationControllerComponent;
//
//namespace ed = ax::NodeEditor;
//
///// @brief ピンの種類（天より注ぐ入力か、地へ放つ出力か）
//enum class PinKind {
//    Input,
//    Output
//};
//
///// @brief ノードの脈動を繋ぐ「端子」の定義
//struct BasePin {
//    ed::PinId id;
//    std::string name;
//    PinKind kind;
//    ed::NodeId nodeId; // 自身が所属する主（ノード）の識別子
//};
//
///// @brief 画面に浮かぶ精神の断片「ノード」の基底
//struct BaseNode {
//    ed::NodeId id;
//    std::string name;
//    std::vector<BasePin> inputs;
//    std::vector<BasePin> outputs;
//    ImVec2 position;
//
//    virtual ~BaseNode() = default;
//};
//
///// @brief 二つの端子を繋ぐ「因果の糸（リンク）」
//struct BaseLink {
//    ed::LinkId id;
//    ed::PinId startPinId; // 出力元
//    ed::PinId endPinId;   // 入力先
//};
//
///// @brief 独立してノードの海を描画するためのエディタウィンドウ
//#pragma once
//#include "../Window/EditorWindow.h"
//#include "imgui_node_editor.h"
//#include <vector>
//#include <string>
//#include <memory>
//
//// 前方宣言：コントローラーたちの存在を窓の世界に伝える
//class GameObject;
//class GameObjectManager;
//class GltfModelCommponennt;
//class AnimationControllerComponent;
//
//namespace ed = ax::NodeEditor;
//
///// @brief 独立してノードの海を描画するためのエディタウィンドウ
//class AnimNodeEditorWindow : public EditorWindow
//{
//public:
//    // 創世の儀式：コンストラクタでオブジェクトマネージャーを記憶する
//    AnimNodeEditorWindow(const std::string& title, GameObjectManager* objManager);
//    virtual ~AnimNodeEditorWindow();
//
//    void Update(float deltaTime) override;
//    void RenderScene(ID3D11DeviceContext* context) override;
//    void DrawGUI() override;
//
//    // 編集対象のコントローラーを切り替えるための秘術
//    void SetTargetController(AnimationControllerComponent* controller) { m_targetController = controller; }
//
//protected:
//    // 内部の役割分割
//    void DrawCanvas();
//    void DrawInspector();
//    void HandleCreation();
//    void HandleDeletion();
//
//    // 識別子を紡ぎ出す泉（コントローラーが持つ m_NextID の力を借りる）
//    uintptr_t GetNextId();
//
//protected:
//    ed::EditorContext* m_nodeEditorContext = nullptr;
//    ed::Config         m_nodeEditorConfig;
//
//    // ーーー 契約の絆 ーーー
//    AnimationControllerComponent* m_targetController = nullptr;
//    GameObjectManager* m_pObjManager = nullptr;
//
//    // 現在ノードエディタ上で選択されているノードのIDを記憶する聖なる碑文
//    ed::NodeId m_selectedNodeId = 0;
//    // ★追加：選択されたリンク(遷移)のIDを記憶する変数
//    ed::LinkId m_selectedLinkId = 0;
//    bool m_needRestorePositions;
//};
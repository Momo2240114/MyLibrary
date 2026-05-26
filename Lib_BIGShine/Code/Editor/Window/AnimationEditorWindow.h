//#pragma once
//
//#include "EditorWindow.h" // ベースクラスのヘッダー（環境に合わせてパスを調整してください）
//#include <memory>
//#include <string>
//#include <vector>
//
//// 前方宣言の儀。ヘッダーの結合度を下げ、ビルド速度を護る防壁
//class GameObject;
//class GameObjectManager;
//class GltfModelCommponennt;
//class AnimationControllerComponent;
//struct PoseLibraryResource;
//struct SavedPose;
//struct ID3D11DeviceContext;
//
//class AnimationEditorWindow : public EditorWindow
//{
//private:
//    // マネージャーへのポインタを記憶する
//    GameObjectManager* m_pObjManager = nullptr;
//
//    // 現在エディタがフォーカスしているオブジェクトと、その心臓部たるコンポーネント群
//    std::shared_ptr<GameObject>   m_targetObject = nullptr;
//    GltfModelCommponennt* m_modelComp = nullptr;
//    AnimationControllerComponent* m_animController = nullptr;
//
//    float m_playbackSpeed = 1.0f;
//    int   m_selectedPoseIdx = -1;
//    int   m_selectedAnimIdx = -1;
//
//public:
//    // コンストラクタはモデルパスではなく、GameObjectManagerのポインタを受け取る
//    AnimationEditorWindow(const std::string& title, GameObjectManager* objManager);
//    virtual ~AnimationEditorWindow() override = default;
//
//    void Update(float deltaTime) override;
//    void RenderScene(ID3D11DeviceContext* context) override;
//    void DrawGUI() override;
//
//private:
//    void DrawLeftPanel(PoseLibraryResource* poseLib);
//    void DrawRightPanel(PoseLibraryResource* poseLib);
//    void CaptureCurrentPose(PoseLibraryResource* poseLib);
//};
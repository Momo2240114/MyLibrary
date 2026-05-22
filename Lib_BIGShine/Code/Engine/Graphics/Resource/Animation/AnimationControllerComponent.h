#pragma once
#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "ModelAnimator.h"
#include "Engine/GamePlay/Serialize/ActionRegistry.h"


#include <unordered_map>
#include <variant>
#include <memory>

// Blackboardが持てる型の定義
using BlackBoardValue = std::variant<float, int, bool, DirectX::SimpleMath::Vector2>;
// 前方宣言
namespace ax { namespace NodeEditor { struct EditorContext; } }


class AnimationControllerComponent : public Component
{
public:
    const std::string& GetName() const override { static std::string name = "AnimationControllerComponent"; return name; }
    ~AnimationControllerComponent();
    void Start() override;
    void Update(float deltaTime) override;

    void Gui() override;
    void ShowSequencerEditor();
    // エディタ用表示関数
    void OnGuiNodeEditor();
  

    // 特定のステートへ遷移させる（直接呼び出し用）
    void ChangeState(const std::string& stateName, float duration = 0.2f);

    // セーブ・ロードの実装
    void Serialize(nlohmann::json& j) const override;
    void Deserialize(const nlohmann::json& j) override;

    // ステートを手動で追加する
    void AddState(const std::string& stateName, int animIndex);
    // ステートを削除する
    void RemoveState(const std::string& stateName);

    // 外部からトリガーを引く
    void SetTrigger(const std::string& name)
    {
        blackboard[name] = true;
    }
    // 内部でトリガーをリセットする
    void ResetTrigger(const std::string& name)
    {
        if (blackboard.count(name)) {
            blackboard[name] = false;
        }
    }

    AnimGraphNode* AFindNodeByPin(ed::PinId pinId);
private:
    ModelAnimator animator;
    CurveComponent* curvecomp;
    GltfModelCommponennt* modelComp = nullptr;

    bool showSequencerWindow = false; // ウィンドウの表示状態を管理
    bool m_ShowNodeEditor = false; // ウィンドウの表示状態を管理

    // ノードエディタで管理されるステート群
    std::unordered_map<std::string, std::unique_ptr<AnimState>> states;
    const AnimState* currentState = nullptr;
    AnimState* editingState = nullptr;// 編集中のステートを指す

    std::string m_assetFilePath; // 保存するパス

    std::unordered_map<std::string, BlackBoardValue> blackboard;
    // グラフデータ
    std::vector<AnimGraphNode> graphNodes;
    std::vector<AnimTransition> graphTransitions;
    // エディタのインスタンス状態を保持するコンテキスト
    ed::EditorContext* nodeEditorContext = nullptr;
    // エディタの初期設定用
    ed::Config nodeEditorConfig;

    // 内部処理用
    void EvaluateGraphTransitions();
    bool EvaluateCondition(const AnimCondition& cond);
    void LoadAsset(const std::string& path);
    void SaveAsset(const std::string& path);

    // ActionRegistryへの登録名リスト（削除用）
    std::vector<std::string> registeredFullNames;
    // ... 他のメンバ ...
    uintptr_t m_NextID = 1; // 0は無効値(Invalid)として扱われることが多いため1から開始
    // ID発行用ヘルパー
    uintptr_t GetNextID() { return m_NextID++; }
public:
    // 外部からのパラメータ入力
    void SetParam(const std::string& name, BlackBoardValue value) { blackboard[name] = value; }
};
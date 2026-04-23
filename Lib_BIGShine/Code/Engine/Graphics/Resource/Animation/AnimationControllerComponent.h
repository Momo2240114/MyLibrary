#pragma once
#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "ModelAnimator.h"
#include "Engine/GamePlay/Serialize/ActionRegistry.h"

#include <unordered_map>
#include <memory>

class AnimationControllerComponent : public Component
{
public:
    const std::string& GetName() const override { static std::string name = "AnimationControllerComponent"; return name; }

    void Start() override;
    void Update(float deltaTime) override;

    void Gui() override;
    void ShowSequencerEditor();

    // 特定のステートへ遷移させる（直接呼び出し用）
    void ChangeState(const std::string& stateName, float duration = 0.2f);

    // ノードエディタ向けのパラメータ管理（拡張用）
    void SetParam(const std::string& name, float value) { blackboard[name] = value; }

    // セーブ・ロードの実装
    void Serialize(nlohmann::json& j) const override;
    void Deserialize(const nlohmann::json& j) override;

    // ステートを手動で追加する
    void AddState(const std::string& stateName, int animIndex);
    // ステートを削除する
    void RemoveState(const std::string& stateName);
private:
    ModelAnimator animator;
    GltfModelCommponennt* modelComp = nullptr;

    bool showSequencerWindow = false; // ウィンドウの表示状態を管理

    // ノードエディタで管理されるステート群
    std::unordered_map<std::string, std::unique_ptr<AnimState>> states;
    const AnimState* currentState = nullptr;

    // 変数管理（遷移条件用）
    std::unordered_map<std::string, float> blackboard;

    // ActionRegistryへの登録名リスト（削除用）
    std::vector<std::string> registeredFullNames;
};
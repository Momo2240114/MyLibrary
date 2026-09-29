#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"

#include <vector>

// 演出Sequenceから、事前配置されたBossなどの参加状態を切り替えるComponent。
class GameModeObjectToggleComponent : public Component
{
public:
    const std::string& GetName() const override
    {
        static std::string name_ = "GameModeObjectToggleComponent";
        return name_;
    }

    void Start() override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

private:
    // SequenceやEditorから呼べる表示切り替えActionを登録する。
    void RegisterActions();
    // 保存済みの有効状態を使って、対象Objectの参加状態を切り替える。
    void SetObjectEnabled(bool enabled_);
    // 非表示から復帰できるよう、開始時点のComponent有効状態を保存する。
    void CaptureEnabledStates();
    // 通常ComponentとColliderComponentの有効状態を反映する。
    void ApplyEnabledStates(bool enabled_);

private:
    // Scene開始時にこのObjectを参加状態にするかどうか。
    bool startEnabled = true;
    // 通常Componentを表示切り替え対象に含めるかどうか。
    bool includeNormalComponents = true;
    // ColliderComponentを表示切り替え対象に含めるかどうか。
    bool includeColliderComponents = true;
    // Object名付きGlobal Actionを登録し、Sequenceから直接呼べるようにする。
    bool registeredGlobalActions = true;
    bool capturedStates = false;
    std::vector<bool> normalComponentEnabledStates;
    std::vector<bool> colliderComponentEnabledStates;
};

#pragma once
#include "Engine/GamePlay/Core/Component/ComponentBase.h"

// SceneのUI参照とActionをJSONから復元し、メニューの選択と決定を管理する。
class MenuNavigationComponent : public Component
{
public:
    // JSON復元で使用するComponent名を返す。
    const std::string& GetName() const override;
    // 入力待ち時間と選択状態を初期化する。
    void Start() override;
    // KeyConfigとマウス操作から選択を更新し、読み込み済みUIへ反映する。
    void Update(float deltaTime_) override;
    // 入力時間と項目の表示状態を編集する。
    void Gui() override;
    // UI参照、入力Actionと決定Actionを保存する。
    void Serialize(nlohmann::json& json_) const override;
    // Scene JSONのメニュー設定を復元する。
    void Deserialize(const nlohmann::json& json_) override;

    // 設定Windowなどを操作している間はメニュー入力を受け付けない。
    void SetInputSuppressed(bool suppressed_)
    {
        inputSuppressed = suppressed_;
    }

private:
    // 文字や装飾の構成はSceneに置き、Componentは参照と操作状態だけを保持する。
    struct MenuItem
    {
        std::string action;
        std::string labelObject;
        std::string highlightObject;
        std::string markerObject;
        std::string glowObject;
        bool visible = true;
        bool enabled = true;
        float selection = 0.0f;
    };

    // UIとActionが利用できる項目だけを選択可能にする。
    bool IsSelectable(int index_) const;
    // 選択を変更し、設定された移動音を再生する。
    void SelectItem(int index_);
    // 無効な項目を飛ばして前後の候補へ移動する。
    void MoveSelection(int direction_);
    // 選択Actionの実行を共通フェードへ予約する。
    void ConfirmSelection();
    // 保存値を維持し、Curveによる選択の拡縮と実行時の色を反映する。
    void UpdateVisuals();
    // 存在する効果音だけを共通Audio管理から再生する。
    void PlaySound(const std::string& path_) const;
    std::vector<MenuItem> items;
    std::string fadeObject;
    std::string panelObject;
    std::string keyboardGuideObject;
    std::string playStationGuideObject;
    std::string developerGuideObject;
    std::string developerAction;
    std::string previousInputAction;
    std::string nextInputAction;
    std::string confirmInputAction;
    std::string developerInputAction;
    std::string moveSound;
    std::string confirmSound;
    float inputDelay = 1.5f;
    float repeatDelay = 0.35f;
    float repeatInterval = 0.14f;
    float elapsedTime = 0.0f;
    float selectionTime = 0.0f;
    float repeatTime = 0.0f;
    int selectedIndex = -1;
    int heldDirection = 0;
    bool confirmHeld = true;
    bool inputSuppressed = false;
    bool transitioning = false;
};

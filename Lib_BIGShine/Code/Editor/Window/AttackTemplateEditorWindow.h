#pragma once

#include "EditorWindow.h"

#include <string>

// 攻撃テンプレートを一覧から選択し、専用Window内で編集するためのEditorWindow。
class AttackTemplateEditorWindow : public EditorWindow
{
public:
    explicit AttackTemplateEditorWindow(GameObjectManager* manager_);

    // Data/AttackTemplate配下の攻撃テンプレートを管理するGUIを描画する。
    void Draw() override;

private:
    // 選択中テンプレートが空の場合、読み込み済み一覧から編集対象を補完する。
    void SelectFallbackTemplate();

    std::string selectedTemplateId;
    std::string newTemplateIdBuffer = "NewAttackTemplate";
    std::string duplicateTemplateIdBuffer;
};

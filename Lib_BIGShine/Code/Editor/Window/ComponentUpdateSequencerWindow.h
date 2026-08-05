#pragma once

#include "EditorWindow.h"

#include <memory>
#include <vector>

class Component;
class GameObject;

class ComponentUpdateSequencerWindow : public EditorWindow
{
public:
    explicit ComponentUpdateSequencerWindow(GameObjectManager* manager_);
    void Draw() override;

private:
    enum class COMPONENT_UPDATE_PHASE
    {
        // 通常UpdateとLateUpdateは別々の実行順を持つため、編集対象を分ける。
        UPDATE,
        LATE_UPDATE
    };

    struct ComponentRow
    {
        // 表示用にComponent/Colliderの種別と元の登録順をまとめる。
        std::shared_ptr<Component> component;
        bool isCollider = false;
        int order = 0;
    };

    void DrawPhaseTabs();
    void DrawComponentTable(const std::shared_ptr<GameObject>& object_);
    void DrawComponentRow(ComponentRow& row_);
    std::vector<ComponentRow> BuildRows(const std::shared_ptr<GameObject>& object_) const;
    void MoveComponentPriority(Component& component_, int delta_);

    COMPONENT_UPDATE_PHASE selectedPhase = COMPONENT_UPDATE_PHASE::UPDATE;
    // 表にColliderや無効Componentを含めるかどうかの表示オプション。
    bool showColliders = true;
    bool showDisabledComponents = true;
};

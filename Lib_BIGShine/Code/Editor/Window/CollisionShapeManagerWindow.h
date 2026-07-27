#pragma once

#include "EditorWindow.h"

#include <memory>
#include <string>
#include <vector>

class ColliderComponent;
class GameObject;

class CollisionShapeManagerWindow : public EditorWindow
{
public:
    explicit CollisionShapeManagerWindow(GameObjectManager* manager_);

    void Draw() override;

private:
    void DrawGlobalControls();
    void DrawTargetControls();
    void DrawColliderList();
    void DrawObjectPicker();
    std::vector<std::shared_ptr<GameObject>> BuildListedObjects() const;
    void CollectHierarchyObjects(
        const std::shared_ptr<GameObject>& rootObject_,
        std::vector<std::shared_ptr<GameObject>>& objects_) const;
    void SetTargetObjectName(const std::string& objectName_);

    char targetObjectName[128] = {};
    bool includeChildren = true;
    bool listTargetOnly = true;
};

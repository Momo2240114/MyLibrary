#include "PrecompiledHeader.h"

#include "CollisionShapeManagerWindow.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/Physics/ColliderComponent.h"

#include <algorithm>

CollisionShapeManagerWindow::CollisionShapeManagerWindow(
    GameObjectManager* manager_)
    : EditorWindow("Collision Shape Manager", manager_)
{
    isVisible = false;
    defaultWindowSize = ImVec2(520.0f, 640.0f);
}

void CollisionShapeManagerWindow::Draw()
{
    // 全体表示、対象オブジェクト指定、Collider一覧を上から順に並べる管理用ウィンドウ。
    if (!BeginEditorWindow(nullptr, ImGuiWindowFlags_NoCollapse))
    {
        EndEditorWindow();
        return;
    }

    if (!gameObjectManager)
    {
        ImGui::TextUnformatted("GameObjectManager is not available.");
        EndEditorWindow();
        return;
    }

    DrawGlobalControls();
    ImGui::Separator();
    DrawTargetControls();
    ImGui::Separator();
    DrawColliderList();

    EndEditorWindow();
}

void CollisionShapeManagerWindow::DrawGlobalControls()
{
    // ShapeRenderer全体の有効/無効と、全Colliderの表示状態を一括で切り替える。
    bool renderEnabled_ = gameObjectManager->IsShapeRenderEnabled();
    if (ImGui::Checkbox("Render Debug Shapes", &renderEnabled_))
    {
        gameObjectManager->SetShapeRenderEnabled(renderEnabled_);
    }

    const size_t colliderCount_ = gameObjectManager->CountAllCollisionShapes();
    ImGui::Text("All Collision Shapes: %zu", colliderCount_);

    if (ImGui::Button("Show All Shapes"))
    {
        gameObjectManager->SetAllCollisionShapeRenderEnabled(true);
    }
    ImGui::SameLine();
    if (ImGui::Button("Hide All Shapes"))
    {
        gameObjectManager->SetAllCollisionShapeRenderEnabled(false);
    }
}

void CollisionShapeManagerWindow::DrawTargetControls()
{
    // 個別対象は選択中オブジェクト、手入力、一覧選択のどれからでも指定できるようにする。
    ImGui::TextUnformatted("Target Object");

    if (const std::shared_ptr<GameObject> selectedObject_ =
        gameObjectManager->GetSelectedObject())
    {
        if (ImGui::Button("Use Selected Object"))
        {
            SetTargetObjectName(selectedObject_->GetName());
        }
        ImGui::SameLine();
        ImGui::Text("%s", selectedObject_->GetName().c_str());
    }
    else
    {
        ImGui::TextDisabled("No object selected.");
    }

    ImGui::InputText("Object Name", targetObjectName, sizeof(targetObjectName));
    DrawObjectPicker();
    ImGui::Checkbox("Include Children", &includeChildren);

    const size_t targetColliderCount_ =
        gameObjectManager->CountCollisionShapesInHierarchy(
            targetObjectName,
            includeChildren);
    // Include Childrenが有効なら子階層のColliderも含めて数え、表示切り替えにも同じ条件を使う。
    ImGui::Text("Target Collision Shapes: %zu", targetColliderCount_);

    const bool hasTarget_ = targetObjectName[0] != '\0';
    ImGui::BeginDisabled(!hasTarget_);
    if (ImGui::Button("Show Target Shapes"))
    {
        gameObjectManager->SetCollisionShapeRenderEnabledInHierarchy(
            targetObjectName,
            true,
            includeChildren);
    }
    ImGui::SameLine();
    if (ImGui::Button("Hide Target Shapes"))
    {
        gameObjectManager->SetCollisionShapeRenderEnabledInHierarchy(
            targetObjectName,
            false,
            includeChildren);
    }
    ImGui::EndDisabled();

    ImGui::Checkbox("List Target Only", &listTargetOnly);
}

void CollisionShapeManagerWindow::DrawObjectPicker()
{
    // Scene内GameObject名をコンボで選び、文字列指定の入力ミスを減らす。
    const char* preview_ = targetObjectName[0] != '\0'
        ? targetObjectName
        : "(none)";
    if (!ImGui::BeginCombo("Pick Object", preview_)) return;

    for (const std::shared_ptr<GameObject>& object_ :
        gameObjectManager->GetObjects())
    {
        if (!object_) continue;
        const std::string& objectName_ = object_->GetName();
        const bool selected_ = objectName_ == targetObjectName;
        if (ImGui::Selectable(objectName_.c_str(), selected_))
        {
            SetTargetObjectName(objectName_);
        }
        if (selected_) ImGui::SetItemDefaultFocus();
    }

    ImGui::EndCombo();
}

void CollisionShapeManagerWindow::DrawColliderList()
{
    // 表示対象のGameObjectを作ってから、各Colliderのデバッグ描画フラグを表形式で編集する。
    const std::vector<std::shared_ptr<GameObject>> objects_ = BuildListedObjects();
    if (objects_.empty())
    {
        ImGui::TextDisabled("No objects to list.");
        return;
    }

    if (!ImGui::BeginTable(
        "CollisionShapeManagerTable",
        5,
        ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_Resizable |
        ImGuiTableFlags_ScrollY,
        ImVec2(0.0f, 0.0f)))
    {
        return;
    }

    ImGui::TableSetupColumn("Object");
    ImGui::TableSetupColumn("Collider");
    ImGui::TableSetupColumn("Shape");
    ImGui::TableSetupColumn("Ground");
    ImGui::TableSetupColumn("Role");
    ImGui::TableHeadersRow();

    for (const std::shared_ptr<GameObject>& object_ : objects_)
    {
        if (!object_) continue;
        int colliderIndex_ = 0;
        for (const std::shared_ptr<ColliderComponent>& collider_ :
            object_->GetComponents<ColliderComponent>())
        {
            if (!collider_) continue;

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(object_->GetName().c_str());

            ImGui::TableNextColumn();
            const std::string colliderLabel_ = collider_->GetColliderName().empty()
                ? collider_->GetName()
                : collider_->GetColliderName();
            ImGui::TextUnformatted(colliderLabel_.c_str());

            ImGui::TableNextColumn();
            const std::string shapeId_ =
                "##Shape_" + object_->GetName() + "_" + colliderLabel_ +
                "_" + std::to_string(colliderIndex_);
            // チェックボックスは表示名を出さず、ColliderごとのshapeRenderEnabledだけを直接操作する。
            ImGui::Checkbox(shapeId_.c_str(), &collider_->shapeRenderEnabled);

            ImGui::TableNextColumn();
            ImGui::TextUnformatted(collider_->CanBeCharacterGround() ? "Yes" : "No");

            ImGui::TableNextColumn();
            ImGui::Text("%d", static_cast<int>(collider_->GetCollisionRole()));

            ++colliderIndex_;
        }
    }

    ImGui::EndTable();
}

std::vector<std::shared_ptr<GameObject>>
CollisionShapeManagerWindow::BuildListedObjects() const
{
    // Target Onlyなら対象階層だけ、そうでなければManagerが持つ全GameObjectを一覧対象にする。
    std::vector<std::shared_ptr<GameObject>> objects_;
    if (!gameObjectManager) return objects_;

    if (listTargetOnly && targetObjectName[0] != '\0')
    {
        const std::shared_ptr<GameObject> rootObject_ =
            gameObjectManager->Find(targetObjectName);
        CollectHierarchyObjects(rootObject_, objects_);
        return objects_;
    }

    objects_ = gameObjectManager->GetObjects();
    return objects_;
}

void CollisionShapeManagerWindow::CollectHierarchyObjects(
    const std::shared_ptr<GameObject>& rootObject_,
    std::vector<std::shared_ptr<GameObject>>& objects_) const
{
    if (!rootObject_) return;

    // includeChildrenが有効な時はTransform階層を再帰的にたどり、子Colliderも表に出す。
    objects_.push_back(rootObject_);
    if (!includeChildren || !rootObject_->transform) return;

    for (const std::shared_ptr<Transform>& childTransform_ :
        rootObject_->transform->children)
    {
        if (!childTransform_) continue;
        CollectHierarchyObjects(childTransform_->gameObject.lock(), objects_);
    }
}

void CollisionShapeManagerWindow::SetTargetObjectName(
    const std::string& objectName_)
{
    // ImGui::InputText用の固定長バッファへ安全にコピーする。
    strncpy_s(
        targetObjectName,
        sizeof(targetObjectName),
        objectName_.c_str(),
        _TRUNCATE);
}

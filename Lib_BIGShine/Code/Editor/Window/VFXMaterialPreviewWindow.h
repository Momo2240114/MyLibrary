#pragma once

#include "EditorWindow.h"
#include "Editor/Workspace/EditorRenderTarget.h"
#include "Engine/GamePlay/Camera/CameraComponent.h"
#include "Engine/Graphics/VFX/MeshVFXComponent.h"
#include "Engine/Graphics/VFX/MeshVFXRenderSystem.h"

#include <memory>
#include <string>

class GameObject;

// VFXMaterialを専用RenderTarget上の球体へ貼り付けて確認するEditorWindow。
class VFXMaterialPreviewWindow : public EditorWindow
{
public:
    explicit VFXMaterialPreviewWindow(GameObjectManager* manager_);
    void Draw() override;

    bool ShouldPauseScene() const override
    {
        return false;
    }

private:
    // Window内で完結するプレビュー用GameObjectとMeshVFX描画環境を準備する。
    void EnsurePreviewObject();
    // 現在選択しているMaterialを球体へ貼るMeshVFXアセットを生成する。
    MeshVFXAsset CreatePreviewAsset() const;
    void DrawToolbar();
    void DrawPreview(const ImVec2& previewSize_);
    void RenderPreview(const ImVec2& previewSize_);
    void ApplyPreviewMaterial();
    // プレビュー用固定カメラのView/Projectionを更新する。
    void SetupPreviewCamera(int width_, int height_);

private:
    EditorRenderTarget previewTarget;
    // プレビュー専用の描画先、MeshVFX描画システム、カメラをWindow内で完結させる。
    MeshVFXRenderSystem meshVFXRenderSystem;
    CameraComponent previewCamera;
    std::shared_ptr<GameObject> previewObject;
    std::shared_ptr<MeshVFXComponent> previewMeshVFX;
    // 選択中VFXMaterialのパス。Apply時にプレビュー用MeshVFXへ差し込む。
    std::string materialPath;
    float previewRotation = 0.0f;
    bool isInitialized = false;
};

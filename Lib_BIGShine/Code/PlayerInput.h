#pragma once

#include "ComponentBase.h"
#include "KeyConfig.h"


#include <DirectXMath.h>

// 前方宣言
class CameraComponent;
class Movement;

class PlayerInput : public Component
{
public:
    const std::string& GetName() const override { static std::string name = "PlayerInput"; return name; }

    // --- コンポーネント用パラメータ ---
    float moveSpeed = 5.0f;

    // カメラを弱参照で保持（シーン上のメインカメラを割り当てる）
    std::weak_ptr<GameObject> mainCamera;

    void Start() override;
    void Update(float deltaTime) override;
    void Gui() override;
public:
    // フラグを立てる（イベントの Update 中に呼ばれる）
    void RecordInput(const std::string& actionName);

    // フラグをリセット（イベントの Start 直前に呼ぶ）
    void ClearInput(const std::string& actionName);

    // 入力があったか確認（イベントの End で呼ぶ）
    bool HasInput(const std::string& actionName);
private:
    // カメラの向きに基づいたワールド移動ベクトルを計算する
    Vector3 CalculateCameraRelativeInput();

    void RegisterComboAction(const std::string& actionName, int nextAnimIndex);
    // アクション名ごとの「入力済みフラグ」
    std::map<std::string, bool> m_inputFlags;
};
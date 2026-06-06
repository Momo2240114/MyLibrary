#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/GamePlay/Input/KeyConfig.h"


#include <DirectXMath.h>
#include <map>
#include <string>

// 前方宣言
class CameraComponent;
class Movement;

enum class PLAYER_ACTION
{
    MOVE_FORWARD,
    MOVE_BACKWARD,
    MOVE_LEFT,
    MOVE_RIGHT,
    ATTACK
};

class PlayerInput : public Component
{
public:
    const std::string& GetName() const override { static std::string name = "PlayerInput"; return name; }

    // --- コンポーネント用パラメータ ---
    float moveSpeed = 5.0f;

    // カメラを弱参照で保持（シーン上のメインカメラを割り当てる）
    std::weak_ptr<GameObject> mainCamera;

    void Start() override;
    void Update(float deltaTime_) override;
    void Gui() override;
public:
    void BindAction(PLAYER_ACTION action_, const InputCombo& combo_);
    bool ActionPressed(PLAYER_ACTION action_) const;
    bool ActionPressedIn(PLAYER_ACTION action_) const;
    bool ActionPressedOut(PLAYER_ACTION action_) const;
    const std::string& GetActionName(PLAYER_ACTION action_) const;

    // フラグを立てる（イベントの Update 中に呼ばれる）
    void RecordInput(const std::string& actionName_);
    void RecordInput(PLAYER_ACTION action_);

    // フラグをリセット（イベントの Start 直前に呼ぶ）
    void ClearInput(const std::string& actionName_);
    void ClearInput(PLAYER_ACTION action_);

    // 入力があったか確認（イベントの End で呼ぶ）
    bool HasInput(const std::string& actionName_);
    bool HasInput(PLAYER_ACTION action_);
private:
    void SetupInputMap();
    void BindDefaultActions();

    // カメラの向きに基づいたワールド移動ベクトルを計算する
    Vector3 CalculateCameraRelativeInput();

    void RegisterComboAction(PLAYER_ACTION action_, int nextAnimIndex_);
    // アクション名ごとの「入力済みフラグ」
    std::map<std::string, bool> m_inputFlags;
    std::map<PLAYER_ACTION, std::string> inputMap;
};

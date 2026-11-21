#pragma once
#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include <functional>
#include <DirectXMath.h>

class SpriteRenderer;

class UIButton : public Component {
public:
    using ClickEvent = std::function<void()>;

    void Start() override;
    void Gui() override;
    void Update(float deltaTime) override;

    // クリックされた時に実行したい関数をセットする
    void SetOnClick(ClickEvent callback)
    {
        onClickCallback = callback;
    }

    const std::string& GetName() const override
    {
        static std::string name = "UIButton";
        return name;
    }

    void Serialize(nlohmann::json& j) const override;
    void Deserialize(const nlohmann::json& j) override;

private:
    bool IsMouseOver(); // マウスがボタンの上にあるか判定

    // これらを保存・読み込みの対象にする
    std::string selectedCategory;//eventの分類
    std::string selectedActionName;//eventの名前


    std::weak_ptr<SpriteRenderer> spriteRenderer;
    ClickEvent onClickCallback;
    bool isPressed = false;   // 押しっぱなし状態の管理
    bool isHovered = false;   // マウスが乗っているかの管理
};
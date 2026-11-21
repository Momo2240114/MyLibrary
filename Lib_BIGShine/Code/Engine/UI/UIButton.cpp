#include "PrecompiledHeader.h"

#include "UIButton.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/Graphics/Resource/Sprite.h" // SpriteRendererの定義
#include "Engine/GamePlay/Input/MouseInput.h" 
#include "Engine/GamePlay/Serialize/ActionRegistry.h"


void UIButton::Start() {
    // 同じGameObjectからSpriteRendererを探して保持しておく
    if (auto obj = GetGameObject()) {
        spriteRenderer = obj->GetComponent<SpriteRenderer>();
    }
}

void UIButton::Update(float deltaTime)
{
    auto sprite = spriteRenderer.lock();
    if (!sprite) {
        spriteRenderer = GetGameObject()->GetComponent<SpriteRenderer>();
        sprite = spriteRenderer.lock();
    }

    // 1. 名前は持っているが、まだ関数がセットされていない（または名前が変わった）場合の紐付け
    if (!selectedActionName.empty() && !onClickCallback) {
        // Registryから関数を探してセットする
        onClickCallback = ActionRegistry::Instance().GetAction(selectedActionName);
    }

    if (!sprite || !sprite->IsVisible())
    {
        return;
    }

    bool currentlyHovered = IsMouseOver();

    auto& mouse = MouseInput::Instance();

    // ホバー状態の変化で色を変える演出（SpriteRendererの機能を利用）
    if (currentlyHovered) 
    {
        if (!isPressed)
        {
            sprite->SetColor({ 1.2f, 1.2f, 1.2f, 1.0f });
        }

        // 1. 押された瞬間
        if (mouse.LeftDown()) {
            isPressed = true;
            sprite->SetColor({ 0.6f, 0.6f, 0.6f, 1.0f });
        }


        // 2. 離された瞬間 ＆ かつ自分が押されていたなら「クリック」成立
        if (mouse.LeftReleased() && isPressed)
        {
            if (onClickCallback) {
                onClickCallback(); // ここで実行！
            }
            isPressed = false;
        }
    }
    else
    {
        // 範囲外で離されたらキャンセル扱いにする
        if (mouse.LeftReleased()) {
            isPressed = false;
            sprite->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f }); // 通常
        }
        else {
            sprite->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f }); // 通常
        }
    }
}

void UIButton::Gui()
{
    auto& reg = ActionRegistry::Instance();
    auto categories = reg.GetCategories();

    // 1. カテゴリ選択
    if (ImGui::BeginCombo("Category", selectedCategory.empty() ? "Select Category" : selectedCategory.c_str())) {
        for (const auto& cat : categories) {
            if (ImGui::Selectable(cat.c_str(), selectedCategory == cat)) {
                selectedCategory = cat;
                selectedActionName = ""; // カテゴリが変わったらリセット
            }
        }
        ImGui::EndCombo();
    }

    // 2. アクション選択
    // カテゴリが未選択なら実行不可の見た目にする
    bool canSelectAction = !selectedCategory.empty();
    if (!canSelectAction) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
    }

    if (ImGui::BeginCombo("Action", selectedActionName.empty() ? "Select Action" : selectedActionName.c_str())) {
        if (canSelectAction) {
            auto names = reg.GetActionNames(selectedCategory);
            for (auto& name : names) {
                if (ImGui::Selectable(name.c_str(), selectedActionName == name)) {
                    selectedActionName = name;
                    onClickCallback = reg.GetAction(selectedActionName);
                }
            }
        }
        ImGui::EndCombo();
    }

    if (!canSelectAction)
    {
        ImGui::PopStyleColor();
    }

    // 3. テスト実行ボタン
    bool isValid = reg.HasAction(selectedActionName);

    if (!isValid) {
        // 無効な場合はグレーアウト
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
    }

    if (ImGui::Button("Test Run", ImVec2(-1, 0))) {
        if (isValid) {
            reg.Call(selectedActionName);
        }
    }

    if (!isValid) {
        ImGui::PopStyleColor(4);
    }
}

bool UIButton::IsMouseOver() {
    auto sprite = spriteRenderer.lock();
    if (!sprite)
    {
        return false;
    }

    // マウスのスクリーン座標
    DirectX::XMFLOAT2 mousePos = MouseInput::Instance().GetScreenPos();

    // Spriteの基準位置（Transformの座標）
    DirectX::XMFLOAT3 pos = sprite->GetPosition();
    // 描画サイズ（localSize * WorldScale）
    DirectX::XMFLOAT2 size = sprite->GetSize();

    // --- ピボットによる判定範囲のオフセット計算 ---
    float left = pos.x;
    float top = pos.y;

    switch (sprite->GetPivot())
    {
    case PosOfsset::TOP_L:
        // Transform座標が左上なので、left/topはそのままでOK
        break;

    case PosOfsset::MID:
        // Transform座標が中央なので、サイズ半分だけ左上へズらす
        left -= size.x * 0.5f;
        top -= size.y * 0.5f;
        break;

    case PosOfsset::UNDER_M:
        // Transform座標が中央下なので、左右は半分、上下はサイズ分上にズらす
        left -= size.x * 0.5f;
        top -= size.y;
        break;
    }

    // 右端と下端を算出
    float right = left + size.x;
    float bottom = top + size.y;

    // AABB判定
    return (mousePos.x >= left && mousePos.x <= right &&
        mousePos.y >= top && mousePos.y <= bottom);
}


void UIButton::Serialize(nlohmann::json& j) const
{
    // 選択されているアクションの名前を保存
    j["SelectedActionName"] = selectedActionName;
}

void UIButton::Deserialize(const nlohmann::json& j)
{
    if (j.contains("SelectedActionName")) {
        selectedActionName = j["SelectedActionName"].get<std::string>();

        auto func = ActionRegistry::Instance().GetAction(selectedActionName);
        if (func) {
            SetOnClick(func);
        }
    }
}
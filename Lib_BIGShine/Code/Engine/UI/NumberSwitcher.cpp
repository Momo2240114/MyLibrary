#include "PrecompiledHeader.h"

#include "NumberSwitcher.h"
#include "Engine/Graphics/Resource/Sprite.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include <algorithm>

void NumberSwitcher::Start()
{
    // 同じGameObject上のSpriteRendererを取得し、現在の数字を初期反映する。
    auto gameObject_ = GetGameObject();
    if (!gameObject_)
    {
        return;
    }

    spriteRenderer = gameObject_->GetComponent<SpriteRenderer>();
    SetDigit(currentDigit);
}

void NumberSwitcher::SetDigit(int num_)
{
    // Grid数が0以下だとIndex計算が壊れるため、最低1に補正する。
    if (maxColumns < 1)
    {
        maxColumns = 1;
    }
    if (maxRows < 1)
    {
        maxRows = 1;
    }

    const int maxIndex_ = (maxColumns * maxRows) - 1;
    // Texture内に存在するGrid範囲へ丸める。
    currentDigit = std::clamp(num_, 0, maxIndex_);

    auto sr_ = spriteRenderer.lock();
    if (!sr_)
    {
        return;
    }

    // SpriteRenderer側にGrid Indexを渡し、対応する数字Cellを表示する。
    sr_->SetUVGridIndex(currentDigit, maxColumns, digitSize);
}

void NumberSwitcher::Gui() 
{
    if (ImGui::InputInt("Current Digit", &currentDigit)) {
        // Editorで値を変えたら即座にUVへ反映する。
        SetDigit(currentDigit);
    }

    ImGui::Separator();
    ImGui::Text("Settings");

    if (ImGui::InputInt("Max Columns", &maxColumns)) {
        if (maxColumns < 1)
        {
            maxColumns = 1;
        }
        SetDigit(currentDigit);
    }

    if (ImGui::InputInt("Max Rows", &maxRows)) {
        if (maxRows < 1)
        {
            maxRows = 1;
        }
        SetDigit(currentDigit);
    }

    if (ImGui::DragFloat2("Digit Size", &digitSize.x, 1.0f)) {
        SetDigit(currentDigit);
    }
}

void NumberSwitcher::Serialize(nlohmann::json& j) const {
    // 数字TextureのGrid設定と現在表示Indexを保存する。
    j["CurrentDigit"] = currentDigit;
    j["MaxColumns"] = maxColumns;
    j["MaxRows"] = maxRows;
    j["DigitSize"] = { digitSize.x, digitSize.y };
}

void NumberSwitcher::Deserialize(const nlohmann::json& j)
{
    // 古いSceneで一部Keyが無い場合は現在値を維持する。
    if (j.contains("CurrentDigit"))
    {
        currentDigit = j["CurrentDigit"];
    }
    if (j.contains("MaxColumns"))
    {
        maxColumns = j["MaxColumns"];
    }
    if (j.contains("MaxRows"))
    {
        maxRows = j["MaxRows"];
    }
    if (j.contains("DigitSize")) {
        digitSize.x = j["DigitSize"][0];
        digitSize.y = j["DigitSize"][1];
    }

    // 読み込み後の設定で表示を更新する。
    SetDigit(currentDigit);
}

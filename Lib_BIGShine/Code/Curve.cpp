#include "Curve.h"

float curvecomp::Evaluate(float t)
{
    if (!data || data->keys.empty()) return 0.0f;
    auto& keys = data->keys;

    if (t <= keys.front().time) return keys.front().value;
    if (t >= keys.back().time) return keys.back().value;

    for (size_t i = 0; i < keys.size() - 1; ++i)
    {
        if (t >= keys[i].time && t <= keys[i + 1].time)
        {
            float dt = keys[i + 1].time - keys[i].time;
            float rate = (t - keys[i].time) / dt;
            return keys[i].value * (1.0f - rate) + keys[i + 1].value * rate;
        }
    }
    return 0.0f;
}

void curvecomp::ShowEditorUI()
{
    if (!data) return;

    ImGui::Text("Curve: %s", data->name.c_str());

    // キー編集・削除
    for (size_t i = 0; i < data->keys.size(); ++i)
    {
        CurveKey& k = data->keys[i];
        ImGui::PushID((int)i);  // ボタンIDの重複回避

        ImGui::DragFloat("Time", &k.time, 0.01f);
        ImGui::DragFloat("Value", &k.value, 0.01f);

        if (ImGui::Button("Delete"))
        {
            data->keys.erase(data->keys.begin() + i);
            ImGui::PopID();
            --i; // インデックス調整
            continue;
        }

        ImGui::Separator();
        ImGui::PopID();
    }

    // キー追加
    if (ImGui::Button("Add Key"))
    {
        CurveKey k;
        if (!data->keys.empty())
            k.time = data->keys.back().time + 0.5f;
        data->keys.push_back(k);
    }
}
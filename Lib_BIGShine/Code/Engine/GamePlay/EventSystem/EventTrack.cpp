#include "PrecompiledHeader.h"

#include "EventTrack.h"
#include "Engine/GamePlay/Serialize/ActionRegistry.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"

#include "imgui.h"

#include <algorithm>
#include <cmath>

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"



EventTrack::EventTrack(const std::string& name, EventType defaultType):
	trackName(name), defaultEventType(defaultType)
{
}

void EventTrack::Update(float currentTime, std::shared_ptr<GameObject> owner)
{
    // 再生ヘッドが巻き戻った、あるいはLoopした場合は、
    // 範囲外へ出た扱いにしてEnd処理を取りこぼさないようにする。
    static float lastTime = 0;
    bool looped = (currentTime < lastTime);
    lastTime = currentTime;

    for (auto& ev : events)
    {
        bool inRange = (currentTime >= ev.startTime && currentTime <= ev.endTime);

        // A. 範囲内に入った最初のFrameでStart Actionを実行する。
        if (inRange && !ev.started) {
            ev.started = true;
            ev.ended = false;
            if (!ev.actionStart.empty())
            {
                Execute(ev.actionStart, owner);
            }
            return; // このFrameはStartだけにして、Updateは次Frameから始める。
        }

        // B. Event範囲内にいる間はUpdate Actionを毎Frame実行する。
        if (inRange) {
            if (!ev.actionUpdate.empty())
            {
                Execute(ev.actionUpdate, owner);
            }
            return;
        }

        // C. 範囲から出た、またはLoopで通過したEventのEnd Actionを1回だけ実行する。
        if ((!inRange && ev.started && !ev.ended) || (looped && ev.started && !ev.ended)) {
            ev.ended = true;
            // startedを戻しておくことで、次回再生時にStartから入り直せる。
            ev.started = false;

            if (!ev.actionEnd.empty()) {
                printf("[Event] Executing End: %s\n", ev.label.c_str());
                Execute(ev.actionEnd, owner);
            }
        }
    }
}

void EventTrack::Execute(const std::string& name, std::shared_ptr<GameObject> owner)
{
    // まずOwner固有のAction名を優先して探す。
    // BuildActionName版と素の名前の両方を許可し、登録側の形式違いを吸収する。
    if (owner && owner->localRegistry.HasAction(owner->BuildActionName(name))) {
        owner->localRegistry.Call(owner->BuildActionName(name));
    }
    else if (owner && owner->localRegistry.HasAction(name)) {
        owner->localRegistry.Call(name);
    }
    // Localに無い場合は、Scene全体で共有されるGlobalRegistryへフォールバックする。
    else if (ActionRegistry::Instance().HasAction(name)) {
        ActionRegistry::Instance().Call(name);
    }
}


// ----------------------------------------------------------
// イベント操作
// ----------------------------------------------------------

void EventTrack::AddEvent(EventType type, const std::string& label, float start, float end, unsigned int color)
{
    // Timeline上に新しいEventを追加する。Action名は後からEditorで設定する。
    events.push_back({ type, start, end, color, label });
}

void EventTrack::DeleteEvent(int index)
{
    if (index >= 0 && index < (int)events.size())
    {
        events.erase(events.begin() + index);
    }
}

void EventTrack::DuplicateEvent(int index)
{
    if (index >= 0 && index < (int)events.size())
    {
        // 現在の設定をそのまま複製し、細かい調整はEditor側のDrag操作で行う。
        events.push_back(events[index]);
    }
}

void EventTrack::BeginEdit(int index)
{
    editingEntry = index;
}

void EventTrack::EndEdit()
{
    editingEntry = -1;
    editingPart = 0;
}

void EventTrack::DoubleClick(int index)
{

}

void EventTrack::Copy()
{
    // 編集中EventをClipboardへ保存する。
    if (editingEntry >= 0 && editingEntry < (int)events.size())
    {
        clipboard = events[editingEntry];
    }
}

void EventTrack::Paste()
{
    // 編集中EventへClipboard内容を上書きする。
    if (editingEntry >= 0 && editingEntry < (int)events.size())
    {
        events[editingEntry] = clipboard;
    }
}


// ----------------------------------------------------------
// ImGui Timeline 描画 (汎用イベント)
// ----------------------------------------------------------
bool EventTrack::DrawSequencer(
    float* currentTime,
    bool* expanded,
    int* selectedEntry,
    float* firstVisibleTime,
    float lengthSeconds,
    std::shared_ptr<GameObject> owner
)
{
    static float pixelsPerSecond = 100.0f;
    const float rowHeight = 24.0f;
    const float labelWidth = 120.0f;
    auto& globalReg = ActionRegistry::Instance();

    // --- 1. カテゴリリストの準備 (Local/Global) ---
    struct CategoryEntry
    {
        std::string name;
        bool isLocal;
    };

    std::vector<CategoryEntry> allCategories;
    if (owner) {
        // OwnerのLocalActionを候補に入れ、EventがObject固有の処理を呼べるようにする。
        for (const auto& cat : owner->localRegistry.GetCategories())
        {
            allCategories.push_back({ cat, true });
        }
    }
    // GlobalActionも候補に入れ、Camera/VFXなど共有ActionをTrackから呼べるようにする。
    for (const auto& cat : globalReg.GetCategories())
    {
        allCategories.push_back({ cat, false });
    }

    // Timelineの横方向Scaleを調整する。
    ImGui::SliderFloat("Zoom", &pixelsPerSecond, 20.0f, 1000.0f, "%.0f px/s");

    float itemAreaHeight = (std::max)(160.0f, (float)events.size() * rowHeight);
    ImGui::BeginChild((trackName + "Seq").c_str(), ImVec2(0, itemAreaHeight + 40), true, ImGuiWindowFlags_HorizontalScrollbar);

    float contentWidth = labelWidth + (lengthSeconds * pixelsPerSecond);
    ImGui::Dummy(ImVec2(contentWidth, 0));
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 origin = ImGui::GetCursorScreenPos();

    // Eventバーを乗せるTimeline背景。
    draw_list->AddRectFilled(origin, origin + ImVec2((std::max)(contentWidth, ImGui::GetWindowWidth()), itemAreaHeight), IM_COL32(30, 30, 30, 255));

    if (*expanded)
    {
        for (int i = 0; i < (int)events.size(); i++)
        {
            auto& ev = events[i];
            ImGui::PushID(i);
            float yTop = origin.y + rowHeight * i;

            // --- タイムライン上の描画とドラッグ操作 (既存機能) ---
            float x0 = origin.x + labelWidth + ev.startTime * pixelsPerSecond;
            float x1 = origin.x + labelWidth + ev.endTime * pixelsPerSecond;
            float width = x1 - x0;

            draw_list->AddRectFilled({ x0, yTop + 2 }, { x1, yTop + rowHeight - 2 }, ev.color, 4.0f);
            if (*selectedEntry == i) {
                draw_list->AddRect({ x0, yTop + 2 }, { x1, yTop + rowHeight - 2 }, IM_COL32_WHITE, 4.0f, 0, 2.0f);
            }

            // バー上にはpayloadを表示し、未設定Eventを見つけやすくする。
            draw_list->AddText({ x0 + 4, yTop + 4 }, IM_COL32_WHITE, ev.payload.empty() ? "(No Action)" : ev.payload.c_str());

            // バー全体をDragすると、Eventの開始/終了を同じ幅のまま移動する。
            ImGui::SetCursorScreenPos({ x0, yTop });
            ImGui::InvisibleButton("move_event", { width, rowHeight });

            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                float deltaX = ImGui::GetIO().MouseDelta.x;
                float deltaTime = deltaX / pixelsPerSecond;
                ev.startTime += deltaTime;
                ev.endTime += deltaTime;

                // Timeline外へはみ出した分だけ押し戻し、Event長は変えない。
                if (ev.startTime < 0) {
                    float d = -ev.startTime;
                    ev.startTime += d;
                    ev.endTime += d;
                }
                if (ev.endTime > lengthSeconds) {
                    float d = ev.endTime - lengthSeconds;
                    ev.startTime -= d;
                    ev.endTime -= d;
                }
            }

            // 行全体を押せるようにして、細いバー以外からでも選択できるようにする。
            ImGui::SetCursorScreenPos({ origin.x, yTop });
            if (ImGui::InvisibleButton("row_select", { labelWidth + (lengthSeconds * pixelsPerSecond), rowHeight })) {
                *selectedEntry = i;
            }

            // 選択中Eventは左Label領域で名前と呼び出すActionを編集する。
            if (*selectedEntry == i) {
                ImGui::SetCursorScreenPos({ origin.x, yTop });

                // std::stringをImGuiで編集するため、一時Bufferへ移してから戻す。
                char buf[128];
                strncpy_s(buf, ev.label.c_str(), sizeof(buf));

                ImGui::SetNextItemWidth(labelWidth - 5); // 少し余白を作る
                if (ImGui::InputText("##edit_labelName", buf, sizeof(buf))) {
                    ev.label = buf; // 入力があるたびに即座に反映
                }

                ImGui::SetNextItemWidth(labelWidth);
                if (ImGui::BeginCombo("##ev_act_combo", ev.payload.empty() ? "Select..." : ev.payload.c_str(), ImGuiComboFlags_NoArrowButton)) {
                    for (const auto& catEntry : allCategories) {
                        if (ImGui::Selectable(((catEntry.isLocal ? "[L] " : "[G] ") + catEntry.name).c_str())) {
                            // カテゴリ選択後は、そのカテゴリの先頭Actionを仮設定して素早く確認できるようにする。
                            auto names = catEntry.isLocal ? owner->localRegistry.GetActionNames(catEntry.name) : globalReg.GetActionNames(catEntry.name);
                            if (!names.empty())
                            {
                                ev.payload = names[0];
                            }
                            ev.label = ev.payload; // ラベルも更新
                        }
                    }
                    ImGui::EndCombo();
                }
            }
            else {
                draw_list->AddText({ origin.x + 4, yTop + 4 }, IM_COL32_WHITE, ev.label.c_str());
            }

            // 開始端/終了端Dragは今後の拡張用として、Sequencer側の構造に合わせてここへ置く想定。

            ImGui::PopID();
        }
    }

    // 現在の再生位置を赤線で表示する。
    float cx = origin.x + labelWidth + (*currentTime) * pixelsPerSecond;
    draw_list->AddLine({ cx, origin.y }, { cx, origin.y + itemAreaHeight }, IM_COL32(255, 0, 0, 255), 2.0f);

    ImGui::EndChild();
    return true;
}

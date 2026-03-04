#include "EventTrack.h"
#include "ActionRegistry.h"
#include "GameObject.h"

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
    // ★重要：再生ヘッドが巻き戻った、あるいはループした判定
    static float lastTime = 0;
    bool looped = (currentTime < lastTime);
    lastTime = currentTime;

    for (auto& ev : events)
    {
        bool inRange = (currentTime >= ev.startTime && currentTime <= ev.endTime);

        // --- A. 範囲内に入った（Start） ---
        if (inRange && !ev.started) {
            ev.started = true;
            ev.ended = false;
            if (!ev.actionStart.empty()) Execute(ev.actionStart, owner);
            return; // ★このフレームは Start だけして抜ける（Update を 1フレーム遅らせる）
        }

        // --- B. 継続中（Update） ---
        if (inRange) {
            if (!ev.actionUpdate.empty()) Execute(ev.actionUpdate, owner);
            return;
        }

        // --- C. 範囲から出た、またはループした（End） ---
        // 「既に開始しているが、今は範囲外」かつ「まだ終了処理をしていない」場合
        if ((!inRange && ev.started && !ev.ended) || (looped && ev.started && !ev.ended)) {
            ev.ended = true;
            // started を false に戻すことで次回の再生に備える
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
    // 1. まず自分の LocalRegistry を探す
    if (owner && owner->localRegistry.HasAction(name)) {
        owner->localRegistry.Call(name);
    }
    // 2. なければ GlobalRegistry を探す
    else if (ActionRegistry::Instance().HasAction(name)) {
        ActionRegistry::Instance().Call(name);
    }
}


// ----------------------------------------------------------
// イベント操作
// ----------------------------------------------------------

void EventTrack::AddEvent(EventType type, const std::string& label, float start, float end, unsigned int color)
{
    events.push_back({ type, start, end, color, label });
}

void EventTrack::DeleteEvent(int index)
{
    if (index >= 0 && index < (int)events.size())
        events.erase(events.begin() + index);
}

void EventTrack::DuplicateEvent(int index)
{
    if (index >= 0 && index < (int)events.size())
        events.push_back(events[index]);
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
    // TODO: イベント詳細設定ウィンドウを開くなど
}

void EventTrack::Copy()
{
    if (editingEntry >= 0 && editingEntry < (int)events.size())
        clipboard = events[editingEntry];
}

void EventTrack::Paste()
{
    if (editingEntry >= 0 && editingEntry < (int)events.size())
        events[editingEntry] = clipboard;
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
    struct CategoryEntry { std::string name; bool isLocal; };
    std::vector<CategoryEntry> allCategories;
    if (owner) {
        for (const auto& cat : owner->localRegistry.GetCategories()) allCategories.push_back({ cat, true });
    }
    for (const auto& cat : globalReg.GetCategories()) allCategories.push_back({ cat, false });

    // ズームコントロール
    ImGui::SliderFloat("Zoom", &pixelsPerSecond, 20.0f, 1000.0f, "%.0f px/s");

    float itemAreaHeight = (std::max)(160.0f, (float)events.size() * rowHeight);
    ImGui::BeginChild((trackName + "Seq").c_str(), ImVec2(0, itemAreaHeight + 40), true, ImGuiWindowFlags_HorizontalScrollbar);

    float contentWidth = labelWidth + (lengthSeconds * pixelsPerSecond);
    ImGui::Dummy(ImVec2(contentWidth, 0));
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 origin = ImGui::GetCursorScreenPos();

    // 背景描画
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

            // ラベル（アクション名）を表示
            draw_list->AddText({ x0 + 4, yTop + 4 }, IM_COL32_WHITE, ev.payload.empty() ? "(No Action)" : ev.payload.c_str());

            // --- 3. ドラッグ移動ロジックの追加 ---
            ImGui::SetCursorScreenPos({ x0, yTop });
            // バー全体を掴むためのボタン
            ImGui::InvisibleButton("move_event", { width, rowHeight });

            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                float deltaX = ImGui::GetIO().MouseDelta.x;
                float deltaTime = deltaX / pixelsPerSecond;
                ev.startTime += deltaTime;
                ev.endTime += deltaTime;

                // 範囲外チェック
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

            // 選択判定
            ImGui::SetCursorScreenPos({ origin.x, yTop });
            if (ImGui::InvisibleButton("row_select", { labelWidth + (lengthSeconds * pixelsPerSecond), rowHeight })) {
                *selectedEntry = i;
            }

            // --- 選択されているイベントの詳細設定 (アクション選択) ---
            if (*selectedEntry == i) {
                // ラベルエリアにアクション選択 GUI を表示（または別の場所に表示）
                ImGui::SetCursorScreenPos({ origin.x, yTop });

                // 名前入力用のバッファ（std::string を直接編集）
                char buf[128];
                strncpy_s(buf, ev.label.c_str(), sizeof(buf));

                ImGui::SetNextItemWidth(labelWidth - 5); // 少し余白を作る
                if (ImGui::InputText("##edit_labelName", buf, sizeof(buf))) {
                    ev.label = buf; // 入力があるたびに即座に反映
                }

                // ペイロードを一時的にカテゴリとアクションに分けるのは手間なので、
                // 今回は「カテゴリ選択」→「その中のアクション選択」の 2 Step GUI を用意します。

                ImGui::SetNextItemWidth(labelWidth);
                if (ImGui::BeginCombo("##ev_act_combo", ev.payload.empty() ? "Select..." : ev.payload.c_str(), ImGuiComboFlags_NoArrowButton)) {
                    for (const auto& catEntry : allCategories) {
                        if (ImGui::Selectable(((catEntry.isLocal ? "[L] " : "[G] ") + catEntry.name).c_str())) {
                            // カテゴリを選択したら、そのカテゴリの最初のアクションを仮設定
                            auto names = catEntry.isLocal ? owner->localRegistry.GetActionNames(catEntry.name) : globalReg.GetActionNames(catEntry.name);
                            if (!names.empty()) ev.payload = names[0];
                            ev.label = ev.payload; // ラベルも更新
                        }
                    }
                    ImGui::EndCombo();
                }
            }
            else {
                draw_list->AddText({ origin.x + 4, yTop + 4 }, IM_COL32_WHITE, ev.label.c_str());
            }

            // (開始・終了ドラッグの InvisibleButton はここに維持...)

            ImGui::PopID();
        }
    }

    // 現在時間ライン
    float cx = origin.x + labelWidth + (*currentTime) * pixelsPerSecond;
    draw_list->AddLine({ cx, origin.y }, { cx, origin.y + itemAreaHeight }, IM_COL32(255, 0, 0, 255), 2.0f);

    ImGui::EndChild();
    return true;
}
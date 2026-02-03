#pragma once

#include "imgui.h"
#include <vector>
#include <string>
#include <memory>

struct AnimationEvent
{
	float startTime;
	float endTime;
	unsigned int color;
	std::string label;
};

class AnimationEventTrack
{
public:
	AnimationEventTrack();
	// イベント操作
	void AddEvent(const std::string& label, float start, float end, unsigned int color);
	void DeleteEvent(int index);
	void DuplicateEvent(int index);
	void BeginEdit(int index);
	void EndEdit();
	void DoubleClick(int index);
	void Copy();
	void Paste();

	// ImGui 描画
	bool DrawSequencer(
		float* currentTime,             // 現在の秒数
		bool* expanded,
		int* selectedEntry,
		float* firstVisibleTime,        // スクロール位置（秒）
		float animationLengthSeconds
	);

	// データアクセス
	int GetItemCount() const { return static_cast<int>(events.size()); }
	const AnimationEvent& GetEvent(int index) const { return events[index]; }
	AnimationEvent& GetEvent(int index) { return events[index]; }

private:
	std::vector<AnimationEvent> events;
	float itemHeight = 20;
	// 編集状態
	int editingEntry;
	int editingPart;
	// コピー用
	AnimationEvent clipboard;
	// 内部ユーティリティ
	int GetCustomHeight(int index) const { return 0; } // 拡張用
};

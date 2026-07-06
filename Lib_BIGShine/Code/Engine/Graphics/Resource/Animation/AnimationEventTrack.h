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
	void AddEvent(const std::string& label_, float start_, float end_, unsigned int color_);
	void DeleteEvent(int index_);
	void DuplicateEvent(int index_);
	void BeginEdit(int index_);
	void EndEdit();
	void DoubleClick(int index_);
	void Copy();
	void Paste();

	// ImGui 描画
	bool DrawSequencer(
		float* currentTime_,             // 現在の秒数
		bool* expanded_,
		int* selectedEntry_,
		float* firstVisibleTime_,        // スクロール位置（秒）
		float animationLengthSeconds_
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

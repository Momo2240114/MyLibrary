#pragma once

#include "imgui.h"
#include <vector>
#include <string>
#include <memory>

struct AnimationEvent
{
	// アニメーション秒数上の開始・終了範囲。
	float startTime;
	float endTime;

	// Sequencer 上で表示する色。
	unsigned int color;

	// Event 名やメモ表示に使うラベル。
	std::string label;
};

class AnimationEventTrack
{
public:
	AnimationEventTrack();
	// イベント操作
	// 指定範囲にイベントバーを追加する。
	void AddEvent(const std::string& label_, float start_, float end_, unsigned int color_);
	void DeleteEvent(int index_);
	void DuplicateEvent(int index_);

	// Drag/Copy/Paste の対象になるイベントを設定する。
	void BeginEdit(int index_);
	void EndEdit();

	// 詳細編集を追加する時の入口。現状は予約だけしている。
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
	// EditorWindow 側がイベント数と各イベントを参照・編集するためのアクセサ。
	int GetItemCount() const
	{
		return static_cast<int>(events.size());
	}

	const AnimationEvent& GetEvent(int index) const
	{
		return events[index];
	}

	AnimationEvent& GetEvent(int index)
	{
		return events[index];
	}

private:
	// Sequencer に表示するイベント一覧。
	std::vector<AnimationEvent> events;
	float itemHeight = 20;
	// 編集状態
	// editingEntry は現在操作中のイベント番号、editingPart は左右ハンドルなどの部位番号。
	int editingEntry;
	int editingPart;
	// コピー用
	// Copy/Paste で一時保持するイベント。
	AnimationEvent clipboard;

	// 内部ユーティリティ
	int GetCustomHeight(int index) const
	{
		return 0;
	} // 拡張用
};

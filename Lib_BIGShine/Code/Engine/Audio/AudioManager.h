#pragma once



#include <string>
#include <set>

#include "Audio.h"

class AudioManager
{
public:
	static AudioManager& Instance() {
		static AudioManager ins;
		return ins;
	}

	void LoadFile();

	void AddBGM(std::unique_ptr<AudioSource>);
	void AddSE(std::unique_ptr<AudioSource>);

	void PlayBGM(size_t index) const;
	void SetVolumeBGM(size_t index, float Volume) const;
	void SetVolumeSE(size_t index, float Volume) const;

	void PlaySE(size_t index) const;
	void StopBGM();
	void StopSE();
	void StopSE(int index);
	void RemoveBGM(size_t index);

	void ClearAll();

	void AddAudioFromFile(const std::string& path, bool isBGM);
	void DrawGUI(); // GUI表示用
private:
	~AudioManager() {};
	AudioManager() {};
	AudioManager(const AudioManager* i) {};
	AudioManager operator= (const AudioManager* i) {};

	std::vector<std::unique_ptr<AudioSource>> audio_BGMs; // AudioSourceのBGM管理用ベクター
	std::vector<std::unique_ptr<AudioSource>> audio_SEs;  // AudioSourceのSE管理用ベクター

	// 重複チェック用のパス保存
	std::set<std::string> loaded_paths;
};

// BGMインデックス
enum BGM_Number {
	BGM_TITLE,       // Title BGM
	BGM_TUTORIAL,    // Tutorial BGM
	BGM_GAME,    // GAME BGM
};

// SEインデックス
enum SE_Number {
	SE_TITLESELECT,    // Title tap sound
	SE_TTILEMOVE,    // Creature/Select sound
	SE_HEARTBEAT,
	SE_GAMEOVER,
	SE_GAMECLEAR,
	SE_DISCOVERY,
};
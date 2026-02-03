#include "AudioManager.h"


#include "ImGui.h"

#include <filesystem>

void AudioManager::LoadFile()
{
	Audio::Instance().Initialize();

	std::vector<std::string> fileNames_BGM = {
		//BGMファイルを書くところ
	};


	if (!fileNames_BGM.empty()) {
		for (const auto& fileName : fileNames_BGM) {
			for (const auto& fileName : fileNames_BGM) {
				AudioSource* raw = Audio::Instance().LoadAudioSource(fileName.c_str());
				if (!raw) continue;
				AddBGM(std::unique_ptr<AudioSource>(raw));
			}

		}
	}

	std::vector<std::string> fileNames_SE = {
		//SEファイルを書くところ
	};

	if (!fileNames_SE.empty())
	{
		for (const auto& fileName : fileNames_SE) {
			AudioSource* raw = Audio::Instance().LoadAudioSource(fileName.c_str());
			if (!raw) continue;
			AddSE(std::unique_ptr<AudioSource>(raw));
		}
	}
}

void AudioManager::AddBGM(std::unique_ptr<AudioSource> source) {
	audio_BGMs.push_back(std::move(source));
}

void AudioManager::AddSE(std::unique_ptr<AudioSource> source) {
	audio_SEs.push_back(std::move(source));
}

void AudioManager::PlayBGM(size_t index) const
{
	if (index >= audio_BGMs.size()) {
		return;
	}
	audio_BGMs[index]->Play(true);
}

void AudioManager::SetVolumeBGM(size_t index, float Volume) const
{
	if (index >= audio_BGMs.size()) {
		return;
	}
	audio_BGMs[index]->SetVolume(Volume);
}

void AudioManager::SetVolumeSE(size_t index, float Volume) const
{
	if (index >= audio_SEs.size()) {
		return;
	}
	audio_SEs[index]->SetVolume(Volume);
}

void AudioManager::PlaySE(size_t index) const
{
	if (index >= audio_SEs.size()) {
		return;
	}
	audio_SEs[index]->Play(false);
}

void AudioManager::StopBGM()
{
	for (auto& audioBGM : audio_BGMs) {
		if (audioBGM) {
			audioBGM->Stop(); // AudioSource の Stop メソッドを呼び出す
		}
	}
}

void AudioManager::StopSE()
{
	for (auto& audioSE : audio_SEs) {
		if (audioSE) {
			audioSE->Stop(); // AudioSource の Stop メソッドを呼び出す
		}
	}
}

void AudioManager::StopSE(int index)
{
	audio_SEs[index]->Stop();
}

void AudioManager::RemoveBGM(size_t index)
{
	if (index < audio_BGMs.size()) {
		audio_BGMs.erase(audio_BGMs.begin() + index);
	}
}

void AudioManager::DrawGUI() 
{
	namespace fs = std::filesystem;

	ImGui::Begin("Audio Loader");

	auto drawFiles = [&](const std::string& directory, bool isBGM) {
		if (!fs::exists(directory)) {
			ImGui::Text("Directory not found: %s", directory.c_str());
			return;
		}

		for (const auto& entry : fs::directory_iterator(directory)) {
			// フォルダではなくファイルのみ対象にする
			if (!entry.is_regular_file()) continue;

			std::string path = entry.path().string();
			std::string fileName = entry.path().filename().string();

			// 重複チェック
			bool isLoaded = (loaded_paths.find(path) != loaded_paths.end());

			if (isLoaded) {
				// すでにロードされている場合は、ボタンではなく「無効化されたテキスト」を表示
				// これにより、間違えて二重にロードすることが物理的に不可能になります
				ImGui::TextDisabled("[Loaded] %s", fileName.c_str());
			}
			else {
				// 未ロードの場合のみボタンを表示
				if (ImGui::Button(fileName.c_str())) {
					AddAudioFromFile(path, isBGM);
				}
			}
		}
		};

	if (ImGui::CollapsingHeader("BGM Selection", ImGuiTreeNodeFlags_DefaultOpen)) {
		drawFiles("Data/Audio/BGM", true);
	}

	if (ImGui::CollapsingHeader("SE Selection", ImGuiTreeNodeFlags_DefaultOpen)) {
		drawFiles("Data/Audio/SE", false);
	}

	ImGui::End();
}

void AudioManager::ClearAll()
{
	// すべてのオーディオを停止
	StopBGM();

	audio_BGMs.clear();
	audio_SEs.clear();

}

void AudioManager::AddAudioFromFile(const std::string& path, bool isBGM)
{
	// すでにロードされているかチェック
	if (loaded_paths.find(path) != loaded_paths.end()) {
		return; // 既にある場合は何もしない
	}

	AudioSource* raw = Audio::Instance().LoadAudioSource(path.c_str());
	if (!raw) return;

	if (isBGM) {
		audio_BGMs.push_back(std::unique_ptr<AudioSource>(raw));
	}
	else {
		audio_SEs.push_back(std::unique_ptr<AudioSource>(raw));
	}

	// パスを記録
	loaded_paths.insert(path);
}


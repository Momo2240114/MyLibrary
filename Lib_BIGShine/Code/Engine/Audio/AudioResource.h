#pragma once

#include <vector>
#include <Windows.h>
#include <mmreg.h>

// PCM形式のWAVファイルを直接解析し、波形データとWAVEFORMATEXを保持するリソースクラス。
class AudioResource
{
public:
	// 指定WAVファイルからRIFF、fmt、dataチャンクを読み込む。
	AudioResource(const char* filename);
	~AudioResource();

	// データ取得
	UINT8* GetAudioData()
	{
		return data.data();
	}

	// データサイズ取得
	UINT32 GetAudioBytes() const
	{
		return static_cast<UINT32>(data.size());
	}

	// WAVEフォーマット取得
	const WAVEFORMATEX& GetWaveFormat() const
	{
		return wfx;
	}

private:
	// RIFFヘッダ
	struct Riff
	{
		UINT32				tag;			// RIFF形式の識別子 'RIFF'
		UINT32				size;			// これ以降のファイルサイズ(ファイルサイズ - 8)
		UINT32				type;			// RIFFの種類を表す識別子 'WAVE'
	};

	// チャンク
	struct Chunk
	{
		UINT32				tag;			// チャンク形式の識別子 'fmt ' 'data'
		UINT32				size;			// データサイズ('fmt 'リニアPCMならば16 'data'波形データサイズ)
	};

	// fmt チャンク
	struct Fmt
	{
		UINT16				fmtId;			// フォーマットID(リニアPCMならば1)
		UINT16				channel;		// チャンネル数(モノラル:1 ステレオ:2)
		UINT32				sampleRate;		// サンプリングレート(44.1kHzなら44100)
		UINT32				transRate;		// データ速度(Byte/sec) [ 44.1kHz 16bit ステレオ 44100×2×2 ]
		UINT16				blockSize;		// ブロックサイズ(Byte/sample×チャンネル数)
		UINT16				quantumBits;	// サンプルあたりのビット数(bit/sample)
	};

	// 読み込んだRIFFヘッダー情報。
	Riff					riff;
	// fmtチャンクから取得したPCMフォーマット情報。
	Fmt						fmt;
	// dataチャンクから読み込んだPCM波形データ。
	std::vector<UINT8>		data;
	// 再生APIへ渡せる形式へ変換したWAVEフォーマット情報。
	WAVEFORMATEX			wfx;

};

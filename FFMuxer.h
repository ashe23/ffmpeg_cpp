#pragma once
#include "FFheader.h"
#include <string>

class FFMuxer {
public:
	void Start();
	~FFMuxer();
private:
	AVStream* AudioStream;
	AVStream* VideoStream;
	AVCodec* AudioCodec;
	AVCodec* VideoCodec;
	AVFrame* AudioFrame=nullptr;
	AVFrame* VideoFrame=nullptr;
	AVCodecContext* AudioCodecContext;
	AVCodecContext* VideoCodecContext;
	AVOutputFormat* OutputFormat;
	AVFormatContext* FormatContext;
	AVDictionary* Dictionary = nullptr;
	SwrContext* AudioResamplerContext = nullptr;
	SwsContext* VideoScaleContext = nullptr;
private:
	const char* OutputFile = "Test.mp4";
	static const int FPS = 30;
	static const int WIDTH = 1280;
	static const int HEIGHT = 720;
	int ErrorCode = 0;
private:
	void PrintError(int error_code);
	void AllocateOutputContext();
	void AddStream();
	void SetAudioAndVideoCodecParams();
	void SetDictionary();
	void OpenCodecs();
};
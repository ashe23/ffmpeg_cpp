#pragma once

// include ffmpeg headers
extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavutil/common.h"
#include "libavutil/frame.h"
#include "libavutil/samplefmt.h"
}

// include c++ default headers
#include <iostream>
#include <string>
#include <fstream>

class FFAudioEncoder
{
public:
	FFAudioEncoder() = default;
	FFAudioEncoder(std::string in_file, std::string out_file) : InputFileName(in_file), OutputFileName(out_file) {}
	~FFAudioEncoder();
	void StartEncode();
private:
	AVCodec * Codec;
	AVCodecContext* CodecContext;
	AVFrame* Frame;
	AVPacket* Packet;
	std::string InputFileName;
	std::string OutputFileName;
	FILE *OutputFile; // todo:ashe23 change to c++ later
private:
	void EncodeFrame(bool flush);
	void OpenOutputFile();
	void AllocPacketAndFrame();
	void Abort(const std::string ErrorMsg);
	void Cleanup();
private:
	// CodecContext user defined settings
	int BitRate = 64000;
	int SampleRate = 44100;
};
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
#include <string>

class FFAudioEncoder
{
public:
	FFAudioEncoder() = default;
	FFAudioEncoder(std::string in_file) : InputFile(in_file) {}
	~FFAudioEncoder();
	void StartEncode();
private:
	AVCodec * Codec;
	AVCodecContext* CodecContext;
	AVFrame* Frame;
	AVPacket* Packet;
	std::string InputFile;

private:
	void Abort(const std::string ErrorMsg);
	void Cleanup();
private:
	// CodecContext user defined settings
	int BitRate = 64000;
	int SampleRate = 44100;
};
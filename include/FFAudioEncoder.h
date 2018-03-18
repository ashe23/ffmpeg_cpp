#pragma once

// include ffmpeg headers
extern "C"
{
#include "libavcodec/avcodec.h"

#include "libavformat/avformat.h"
#include "libavformat/avio.h"

#include "libavutil/common.h"
#include "libavutil/frame.h"
#include "libavutil/samplefmt.h"
#include "libavutil/avassert.h"
#include "libavutil/opt.h"
#include "libavutil/timestamp.h"
#include "libavutil/mathematics.h"
#include "libavutil/audio_fifo.h"

#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
}

// include c++ default headers
#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <iterator>
#include <algorithm>

class FFAudioEncoder
{
public:
	FFAudioEncoder() = default;
	FFAudioEncoder(std::string in_file, std::string out_file) : InputFileName(in_file), OutputFileName(out_file) {}
	~FFAudioEncoder();
	void StartEncode();
private:
	AVCodec * Codec;
	AVCodecContext* OutputCodecContext;
	AVOutputFormat* OutputFormat;
	AVFormatContext* OutputFormatContext;
	AVIOContext* OuputIOContext;
	AVFrame* Frame;
	AVPacket* Packet;
	AVDictionary* Dictionary;
	AVStream* Stream;
	AVAudioFifo* AudioFifo;

	std::string InputFileName;
	std::string OutputFileName;
	FILE *OutputFile; // todo:ashe23 change to c++ later
private:
	void EncodeFrame(bool flush);
	void OpenOutputFile();
	void InitFifo();
	void WriteOutputFileHeader();
	void Loop();
	uint8_t* ReadSamplesFromInputFile();
	void AllocPacketAndFrame();
	void Abort(const std::string ErrorMsg);
	void Cleanup();
	void PrintError(int ErrorCode);

	// method for FIFO
	void AddSamplesToFifo(uint8_t **data);
private:
	// CodecContext user defined settings
	int BitRate = 64000;
	int SampleRate = 44100;
	int FrameNum = 1000;
};
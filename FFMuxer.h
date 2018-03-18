#pragma once
#include "FFheader.h"
#include <string>

enum class FrameType {
	Audio = 0,
	Video
};


struct OutputStream {
	/* pts of the next frame that will be generated */
	int64_t next_pts;
	int samples_count;

	AVFrame *frame;
	AVFrame *tmp_frame;

	float t, tincr, tincr2;
};

class FFMuxer {
public:
	FFMuxer();
	~FFMuxer();
	void Start();
private:
	AVStream* AudioStream;
	AVStream* VideoStream;
	AVCodec* AudioCodec;
	AVCodec* VideoCodec;
	AVFrame* AudioFrame = nullptr;
	AVFrame* VideoFrame = nullptr;
	AVCodecContext* AudioCodecContext;
	AVCodecContext* VideoCodecContext;
	AVOutputFormat* OutputFormat;
	AVFormatContext* FormatContext;
	AVDictionary* Dictionary = nullptr;
	SwrContext* AudioResamplerContext = nullptr;
	SwsContext* VideoScaleContext = nullptr;
	OutputStream AudioSt = { 0 };
	OutputStream VideoSt = { 0 };
private:
	const char* OutputFile = "assets/audio/Test.mp4";
	static const int FPS = 30;
	static const int WIDTH = 1280;
	static const int HEIGHT = 720;
	static const int STREAM_DURATION = 10;
	int ErrorCode = 0;
private:
	void PrintError(int error_code);
	void AllocateOutputContext();
	void AddStream();
	void SetAudioAndVideoCodecParams();
	void SetDictionary();
	void OpenCodecs();
	void OpenOutputFile();
	void Loop();
	void FillYUVImage(AVFrame *pict, int frame_index);


	int WriteFrame(const AVRational *time_base, AVStream *st, AVPacket *pkt);
	AVFrame* alloc_audio_frame(enum AVSampleFormat sample_fmt, uint64_t channel_layout, int sample_rate, int nb_samples);
};
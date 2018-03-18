#include "FFMuxer.h"

void FFMuxer::Start()
{
	// todo make initialization mechanism
	OutputStream audio_st = { 0 };
	OutputStream video_st = { 0 };

	av_register_all();
	AllocateOutputContext();
	SetDictionary();
	AddStream();
	OpenCodecs();


	av_dump_format(FormatContext, 0, OutputFile, 1);

	OpenOutputFile();

	ErrorCode = avformat_write_header(FormatContext, &Dictionary);
	if (ErrorCode < 0) {
		fprintf(stderr, "Error occurred when opening output file\n");
		PrintError(ErrorCode);
	}

	Loop();

	av_write_trailer(FormatContext);

	if (!(OutputFormat->flags & AVFMT_NOFILE))
	{
		/* Close the output file. */
		avio_closep(&FormatContext->pb);
	}

	avformat_free_context(FormatContext);

;}

FFMuxer::FFMuxer()
{
}

FFMuxer::~FFMuxer()
{
	if (Dictionary)
	{
		av_dict_free(&Dictionary);
	}
}

void FFMuxer::PrintError(int error_code)
{
	char error_buf[256];
	av_make_error_string(error_buf, sizeof(error_buf), error_code);	
	printf("Error Code: %d\nError Description: %s", error_code, error_buf);
	_sleep(5000);
	exit(1);
}

void FFMuxer::AllocateOutputContext()
{
	avformat_alloc_output_context2(&FormatContext, nullptr, nullptr, OutputFile);
	if (!FormatContext) {
		printf("Could not deduce output format from file extension: using MPEG.\n");
		avformat_alloc_output_context2(&FormatContext, NULL, "mpeg", OutputFile);
	}
	if (!FormatContext)
	{
		printf("Cant allocate output context!\n");
		exit(1);
	}

	OutputFormat = FormatContext->oformat; 
}

void FFMuxer::AddStream()
{
	// audio codec initializing
	AudioCodec = avcodec_find_encoder(AV_CODEC_ID_MP3);
	if (!AudioCodec) {
		fprintf(stderr, "Could not find encoder for MP3\n");
		exit(1);
	}
	AudioStream = avformat_new_stream(FormatContext, AudioCodec);
	if (!AudioStream)
	{
		fprintf(stderr, "Could not allocate stream for audio\n");
		exit(1);
	}
	AudioCodecContext = avcodec_alloc_context3(AudioCodec);
	if (!AudioCodecContext) {
		fprintf(stderr, "Could not alloc an encoding context for audio\n");
		exit(1);
	}

	// video codec initializing
	VideoCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
	if (!VideoCodec) {
		fprintf(stderr, "Could not find encoder for h264\n");
		exit(1);
	}
	VideoStream = avformat_new_stream(FormatContext, VideoCodec);
	if (!VideoStream)
	{
		fprintf(stderr, "Could not allocate stream for video\n");
		exit(1);
	}
	VideoCodecContext = avcodec_alloc_context3(VideoCodec);
	if (!VideoCodecContext) {
		fprintf(stderr, "Could not alloc an encoding context for video\n");
		exit(1);
	}


	SetAudioAndVideoCodecParams();
}

void FFMuxer::SetAudioAndVideoCodecParams()
{
	// audio
	const AVRational AudioRational = { 1, AudioCodecContext->sample_rate};

	AudioCodecContext->sample_fmt = AudioCodec->sample_fmts ? AudioCodec->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
	AudioCodecContext->bit_rate = 64000;
	AudioCodecContext->sample_rate = 44100;
	AudioCodecContext->channels = av_get_channel_layout_nb_channels(AudioCodecContext->channel_layout);
	AudioCodecContext->channel_layout = AV_CH_LAYOUT_STEREO;// todo change order
	AudioStream->time_base = AudioRational;

	// video
	const AVRational VideoRational = { FPS, 1 };

	VideoCodecContext->codec_tag = 0;
	VideoCodecContext->codec_id = AV_CODEC_ID_H264;
	VideoCodecContext->codec_type = AVMEDIA_TYPE_VIDEO;
	VideoCodecContext->width = WIDTH;
	VideoCodecContext->height = HEIGHT;
	VideoCodecContext->gop_size = 12;
	VideoCodecContext->pix_fmt = AV_PIX_FMT_YUV420P;
	VideoCodecContext->framerate = VideoRational;
	VideoCodecContext->time_base = av_inv_q(VideoRational);

	if (FormatContext->oformat->flags & AVFMT_GLOBALHEADER)
	{
		VideoCodecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	}
}

void FFMuxer::SetDictionary()
{
	av_dict_set(&Dictionary, "profile", "high", 0);
	av_dict_set(&Dictionary, "preset", "superfast", 0);
	av_dict_set(&Dictionary, "tune", "zerolatency", 0);
}

void FFMuxer::OpenCodecs()
{
	// Audio
	ErrorCode = avcodec_open2(AudioCodecContext, AudioCodec, nullptr);
	if (ErrorCode < 0)
	{
		printf("Could not open audio codec!");
		PrintError(ErrorCode);
	}

	AudioFrame = av_frame_alloc();
	AudioFrame->format = AudioCodecContext->sample_fmt;
	AudioFrame->channel_layout = AudioCodecContext->channel_layout;
	AudioFrame->sample_rate = AudioCodecContext->sample_rate;
	AudioFrame->nb_samples = AudioCodecContext->frame_size;

	ErrorCode = av_frame_get_buffer(AudioFrame, 0);
	if (ErrorCode < 0)
	{
		printf("Error allocating an audio buffer!");
		PrintError(ErrorCode);
	}

	AudioSt.frame = AudioFrame;
	AudioSt.tmp_frame = AudioFrame;
	AudioSt.t = 0;
	AudioSt.tincr = 2 * M_PI * 110.0 / AudioCodecContext->sample_rate;
	/* increment frequency by 110 Hz per second */
	AudioSt.tincr2 = 2 * M_PI * 110.0 / AudioCodecContext->sample_rate / AudioCodecContext->sample_rate;

	// copy the stream parameters to the muxer
	ErrorCode = avcodec_parameters_from_context(AudioStream->codecpar, AudioCodecContext);
	if (ErrorCode < 0)
	{
		printf("Could not copy the stream parameters!");
		PrintError(ErrorCode);
	}

	// create resampler context
	AudioResamplerContext = swr_alloc();
	if (!AudioResamplerContext)
	{
		printf("Could not allocate resampler context\n");
		exit(1);
	}

	// set resampler options
	av_opt_set_int(AudioResamplerContext, "in_channel_count", AudioCodecContext->channels, 0);
	av_opt_set_int(AudioResamplerContext, "in_sample_rate", AudioCodecContext->sample_rate, 0);
	av_opt_set_sample_fmt(AudioResamplerContext, "in_sample_fmt", AV_SAMPLE_FMT_S16, 0);
	av_opt_set_int(AudioResamplerContext, "out_channel_count", AudioCodecContext->channels, 0);
	av_opt_set_int(AudioResamplerContext, "out_sample_rate", AudioCodecContext->sample_rate, 0);
	av_opt_set_sample_fmt(AudioResamplerContext, "out_sample_fmt", AudioCodecContext->sample_fmt, 0);

	// initialize the resampling context
	if ((ErrorCode = swr_init(AudioResamplerContext)) < 0)
	{
		printf("Failed to initialize the resampling context\n");
		PrintError(ErrorCode);
		exit(1);
	}



	// Video
	ErrorCode = avcodec_open2(VideoCodecContext, VideoCodec, &Dictionary);
	if (ErrorCode < 0)
	{
		printf("Could not open video codec!");
		PrintError(ErrorCode);
	}

	VideoFrame = av_frame_alloc();
	VideoFrame->width = WIDTH;
	VideoFrame->height = HEIGHT;
	VideoFrame->format = VideoCodecContext->pix_fmt;

	ErrorCode = av_frame_get_buffer(VideoFrame, 32);
	if (ErrorCode < 0)
	{
		PrintError(ErrorCode);
	}


	VideoSt.frame = VideoFrame;
	VideoSt.tmp_frame = VideoFrame;

	ErrorCode = avcodec_parameters_from_context(VideoStream->codecpar, VideoCodecContext);
	if (ErrorCode < 0)
	{
		printf("Could not copy the stream parameters\n");
		PrintError(ErrorCode);
	}
}

void FFMuxer::OpenOutputFile()
{
	if (!(OutputFormat->flags & AVFMT_NOFILE))
	{
		ErrorCode = avio_open(&FormatContext->pb, OutputFile, AVIO_FLAG_WRITE);
		if (ErrorCode < 0) {
			fprintf(stderr, "Could not open '%s'\n", OutputFile);
			PrintError(ErrorCode);
		}
	}
}

void FFMuxer::Loop()
{	
}

void FFMuxer::FillYUVImage(AVFrame* pict, int frame_index)
{
	int x, y, i;

	i = frame_index;

	/* Y */
	for (y = 0; y < HEIGHT; y++)
		for (x = 0; x < WIDTH; x++)
			pict->data[0][y * pict->linesize[0] + x] = x + y + i * 3;

	/* Cb and Cr */
	for (y = 0; y < HEIGHT / 2; y++) {
		for (x = 0; x < WIDTH / 2; x++) {
			pict->data[1][y * pict->linesize[1] + x] = 128 + y + i * 2;
			pict->data[2][y * pict->linesize[2] + x] = 64 + x + i * 5;
		}
	}
}


int FFMuxer::WriteFrame(const AVRational * time_base, AVStream * st, AVPacket * pkt)
{
	/* rescale output packet timestamp values from codec to stream timebase */
	av_packet_rescale_ts(pkt, *time_base, st->time_base);
	pkt->stream_index = st->index;

	/* Write the compressed frame to the media file. */
	return av_interleaved_write_frame(FormatContext, pkt);
}

AVFrame * FFMuxer::alloc_audio_frame(AVSampleFormat sample_fmt, uint64_t channel_layout, int sample_rate, int nb_samples)
{
	AVFrame *frame = av_frame_alloc();
	int ret;

	if (!frame)
	{
		fprintf(stderr, "Error allocating an audio frame\n");
		exit(1);
	}

	frame->format = sample_fmt;
	frame->channel_layout = channel_layout;
	frame->sample_rate = sample_rate;
	frame->nb_samples = nb_samples;

	if (nb_samples) {
		ret = av_frame_get_buffer(frame, 0);
		if (ret < 0) {
			fprintf(stderr, "Error allocating an audio buffer\n");
			exit(1);
		}
	}

	return frame;
}

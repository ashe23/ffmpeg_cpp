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
	system("pause");
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
	// for starting we would generate audio and video, later we will change it to fill from buffer
	bool encode_video = true;
	bool encode_audio = true;
	do
	{
		encode_video = WriteVideoFrame();

	} while (encode_video || encode_audio);
	//while (encode_video || encode_audio)
	//{
	//	/* select the stream to encode */
	//	if (encode_video &&
	//		(!encode_audio || av_compare_ts(video_st.next_pts, video_st.enc->time_base,
	//			audio_st.next_pts, audio_st.enc->time_base) <= 0)) 
	//	{
	//		encode_video = !write_video_frame(oc, &video_st);
	//	}
	//	else 
	//	{
	//		encode_audio = !write_audio_frame(oc, &audio_st);
	//	}
	//}
}

bool FFMuxer::WriteFrame(FrameType Type)
{
	if (Type == FrameType::Audio)
	{
		AVPacket Packet = { 0 };
		av_init_packet(&Packet);

		// Audio
		int ErrorCode = avcodec_send_frame(AudioCodecContext, AudioFrame);
		if (ErrorCode < 0)
		{
			PrintError(ErrorCode);
		}

		ErrorCode = avcodec_receive_packet(AudioCodecContext, &Packet);
		if (ErrorCode < 0)
		{
			PrintError(ErrorCode);
		}

		av_interleaved_write_frame(FormatContext, &Packet);
		av_packet_unref(&Packet);
		return true;
	}
	else if (Type == FrameType::Video)
	{
		AVPacket Packet = { 0 };
		av_init_packet(&Packet);

		// Audio
		int ErrorCode = avcodec_send_frame(VideoCodecContext, VideoFrame);
		if (ErrorCode < 0)
		{
			PrintError(ErrorCode);
		}

		ErrorCode = avcodec_receive_packet(VideoCodecContext, &Packet);
		if (ErrorCode < 0)
		{
			PrintError(ErrorCode);
		}

		av_interleaved_write_frame(FormatContext, &Packet);
		av_packet_unref(&Packet);
		return true;
	}
	return false;
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

AVFrame * FFMuxer::GetVideoFrame()
{
	//AVCodecContext *c = ost->enc;

	/* check if we want to generate more frames */
	const AVRational r = { 1, 1 };
	if (av_compare_ts(VideoSt.next_pts, VideoCodecContext->time_base, STREAM_DURATION, r) >= 0)
	{
		return nullptr;
	}

	/* when we pass a frame to the encoder, it may keep a reference to it
	* internally; make sure we do not overwrite it here */
	ErrorCode = av_frame_make_writable(VideoSt.frame);
	if (ErrorCode < 0)
	{
		PrintError(ErrorCode);
	}

	if (VideoCodecContext->pix_fmt != AV_PIX_FMT_YUV420P) {
		/* as we only generate a YUV420P picture, we must convert it
		* to the codec pixel format if needed */
		if (!VideoScaleContext) {
			VideoScaleContext = sws_getContext(
				VideoCodecContext->width,
				VideoCodecContext->height,
				AV_PIX_FMT_YUV420P,
				VideoCodecContext->width, 
				VideoCodecContext->height,
				VideoCodecContext->pix_fmt,
				SWS_BICUBIC, nullptr, nullptr, nullptr
			);
			if (!VideoScaleContext) {
				fprintf(stderr, "Could not initialize the conversion context\n");
				exit(1);
			}
		}
		FillYUVImage(VideoSt.tmp_frame, VideoSt.next_pts);
		sws_scale(
			VideoScaleContext,
			(const uint8_t * const *)VideoSt.tmp_frame->data,
			VideoSt.tmp_frame->linesize,
			0, 
			VideoCodecContext->height,
			VideoSt.frame->data,
			VideoSt.frame->linesize
		);
	}
	else {
		FillYUVImage(VideoSt.frame, VideoSt.next_pts);
	}

	VideoSt.frame->pts = VideoSt.next_pts++;

	return VideoSt.frame;
}

void FFMuxer::GenerateRandomAudio()
{
	
}

bool FFMuxer::WriteVideoFrame()
{

	VideoFrame = GetVideoFrame();
	return WriteFrame(FrameType::Video);	
}

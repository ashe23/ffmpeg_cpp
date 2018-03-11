#include "..\FFAudioEncoder.h"

int flush_encoder(AVFormatContext *fmt_ctx, unsigned int stream_index) {
	int ret;
	int got_frame;
	AVPacket enc_pkt;
	if (!(fmt_ctx->streams[stream_index]->codec->codec->capabilities &
		CODEC_CAP_DELAY))
		return 0;
	while (1) {
		enc_pkt.data = NULL;
		enc_pkt.size = 0;
		av_init_packet(&enc_pkt);
		ret = avcodec_encode_audio2(fmt_ctx->streams[stream_index]->codec, &enc_pkt,
			NULL, &got_frame);
		av_frame_free(NULL);
		if (ret < 0)
			break;
		if (!got_frame) {
			ret = 0;
			break;
		}
		printf("Flush Encoder: Succeed to encode 1 frame!\tsize:%5d\n", enc_pkt.size);
		/* mux encoded frame */
		ret = av_write_frame(fmt_ctx, &enc_pkt);
		if (ret < 0)
			break;
	}
	return ret;
}

FFAudioEncoder::~FFAudioEncoder()
{
	//Cleanup();
	printf("Resources cleaned successfully.");
}

void FFAudioEncoder::StartEncode()
{
	// checking if given input file exists and valid
	std::ifstream input_file{ InputFileName.c_str() };
	if (!input_file.good())
	{
		// todo:ashe23 weird printing with fprintf
		std::cerr << "Cant Open file " << InputFileName << std::endl;
		Abort("");
	}
	input_file.close(); // todo:ashe23 optimize

	// initializing codecs and all ffmpeg stuff
	avcodec_register_all();
	av_register_all();
	
	uint8_t * FrameBuffer = nullptr;
	FILE *in_file;
	in_file = fopen(InputFileName.c_str(), "rb");

	avformat_alloc_output_context2(&OutputFormatContext, NULL, NULL, OutputFileName.c_str());
	OutputFormat = OutputFormatContext->oformat;
	/*OutputFormatContext = avformat_alloc_context();
	OutputFormat = av_guess_format(nullptr, OutputFileName.c_str(), nullptr);
	OutputFormatContext->oformat = OutputFormat;*/

	// open output url
	if (avio_open(&OutputFormatContext->pb, OutputFileName.c_str(), AVIO_FLAG_READ_WRITE) < 0)
	{
		Abort("Cant open file");
	}

	Stream = avformat_new_stream(OutputFormatContext, 0);
	if (!Stream)
	{
		Abort("Cant create stream");
	}

	OutputCodecContext = Stream->codec;
	OutputCodecContext->codec_id = OutputFormat->audio_codec;
	OutputCodecContext->codec_type = AVMEDIA_TYPE_AUDIO;
	OutputCodecContext->sample_fmt = AV_SAMPLE_FMT_S16P;
	OutputCodecContext->sample_rate = 48000;
	OutputCodecContext->channel_layout = AV_CH_LAYOUT_STEREO;
	OutputCodecContext->channels = av_get_channel_layout_nb_channels(OutputCodecContext->channel_layout);
	OutputCodecContext->bit_rate = 64000;

	av_dump_format(OutputFormatContext, 0, OutputFileName.c_str(), 1);

	Codec = avcodec_find_encoder(OutputCodecContext->codec_id);
	if (!Codec)
	{
		Abort("No codec");
	}

	/*if (OutputFormatContext->oformat->flags & AVFMT_GLOBALHEADER) {
		OutputCodecContext->flags |= CODEC_FLAG_GLOBAL_HEADER;
	}*/

	if (avcodec_open2(OutputCodecContext, Codec, nullptr) < 0)
	{
		Abort("Cant open codec");
	}

	Frame = av_frame_alloc();
	Frame->nb_samples = OutputCodecContext->frame_size;
	Frame->format = OutputCodecContext->sample_fmt;

	int size = av_samples_get_buffer_size(nullptr, OutputCodecContext->channels, OutputCodecContext->frame_size, OutputCodecContext->sample_fmt, 1);
	FrameBuffer = (uint8_t *)av_malloc(size);
	avcodec_fill_audio_frame(Frame, OutputCodecContext->channels, OutputCodecContext->sample_fmt, (const uint8_t*)FrameBuffer, size, 1);

	//Write Header
	avformat_write_header(OutputFormatContext, nullptr);

	Packet = av_packet_alloc();

	int got_frame;
	int ret;
	for (int i = 0; i< FrameNum; i++) {
		//Read PCM
		if (fread(FrameBuffer, 1, size, in_file) <= 0) {
			Abort("Failed to read raw data");
		}
		else if (feof(in_file)) {
			break;
		}
		Frame->data[0] = FrameBuffer;  //PCM Data

		Frame->pts = i* 10;
		got_frame = 0;
		//Encode
		ret = avcodec_encode_audio2(OutputCodecContext, Packet, Frame, &got_frame);
		if (ret < 0) {
			Abort("Failed to encode");
		}
		if (got_frame == 1) {
			printf("Succeed to encode 1 frame! \tsize:%5d\n", Packet->size);
			Packet->stream_index = Stream->index;
			ret = av_write_frame(OutputFormatContext, Packet);
			av_free_packet(Packet);
		}
	}

	//Flush Encoder
	ret = flush_encoder(OutputFormatContext, 0);
	if (ret < 0) {
		Abort("Flushing encoder failed");
	}

	//Write Trailer
	av_write_trailer(OutputFormatContext);

	//Clean
	if (Stream) {
		avcodec_close(Stream->codec);
		av_free(Frame);
		av_free(FrameBuffer);
	}
	avio_close(OutputFormatContext->pb);
	avformat_free_context(OutputFormatContext);

	fclose(in_file);

	//// find mp3 encoder
	//Codec = avcodec_find_encoder(AV_CODEC_ID_MP3);
	//if (!Codec)
	//{
	//	Abort("Codec not found!");
	//}

	//// allocating space for codec
	//CodecContext = avcodec_alloc_context3(Codec);
	//if (!CodecContext)
	//{
	//	Abort("Could not allocate audio codec context");
	//}

	//// setting audio codec parameters
	//CodecContext->bit_rate = BitRate;
	//CodecContext->sample_fmt = CodecContext->codec->sample_fmts[0];
	//CodecContext->sample_rate = SampleRate;
	//CodecContext->channel_layout = AV_CH_LAYOUT_STEREO;
	//CodecContext->channels = av_get_channel_layout_nb_channels(CodecContext->channel_layout);

	//// trying to open codec
	//if (avcodec_open2(CodecContext, Codec, nullptr) < 0)
	//{
	//	Abort("Could not open codec");
	//}

	//OpenOutputFile();
	//AllocPacketAndFrame();

	//printf("Successfully encoded!\n");
}

//void FFAudioEncoder::EncodeFrame(bool flush)
//{
//	if (flush)
//	{
//		Frame = nullptr;
//	}
//
//	int status_code;
//
//	// sending frame for encoding
//	status_code = avcodec_send_frame(OutputCodecContext, Frame);
//	if (status_code < 0)
//	{
//		Abort("Error sending the frame to the encoder");
//	}
//
//	while (status_code >= 0)
//	{
//		status_code = avcodec_receive_packet(OutputCodecContext, Packet);
//		if (status_code == AVERROR(EAGAIN) || status_code == AVERROR_EOF)
//		{
//			return;
//		}
//		else if (status_code < 0)
//		{
//			Abort("Error encoding audio frame");
//		}
//
//		// writing data to file
//		fwrite(Packet->data, 1, Packet->size, OutputFile);
//
//		// free packet
//		av_packet_unref(Packet);
//	}
//}

void FFAudioEncoder::OpenOutputFile()
{
	/*OutputFile = fopen(OutputFileName.c_str(), "wb");
	if (!OutputFile)
	{
		Abort("Could not create file");
	}*/
	int error;
	error = avio_open(&OuputIOContext, OutputFileName.c_str(), AVIO_FLAG_WRITE);
	if (error < 0)
	{
		PrintError(error);
	}

	OutputFormatContext = avformat_alloc_context();
	if (!OutputFormatContext)
	{
		Abort("Could not allocate output format context");
	}

	OutputFormatContext->pb = OuputIOContext;

	// guess format of output file
	auto out_format = av_guess_format(nullptr, OutputFileName.c_str(), nullptr);
	if (!out_format)
	{
		Abort("Could not find output file format");
	}
	OutputFormatContext->oformat = out_format;

	// find aac codec
	Codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
	if (!Codec)
	{
		Abort("Could not find AAC Encoder");
	}

	// create a new audio stream in output file
	Stream = avformat_new_stream(OutputFormatContext, nullptr);
	if (!Stream)
	{
		Abort("Could not create new stream");
	}

	OutputCodecContext = avcodec_alloc_context3(Codec);
	if (!OutputCodecContext)
	{
		Abort("Could not allocate an encoding context");
	}
	
	// set encoder params
	OutputCodecContext->channels = 2;
	OutputCodecContext->channel_layout = av_get_default_channel_layout(OutputCodecContext->channels);
	OutputCodecContext->sample_rate = SampleRate;
	OutputCodecContext->sample_fmt = Codec->sample_fmts[0];
	OutputCodecContext->bit_rate = BitRate;

	// Allow the use of the experimental AAC encoder
	OutputCodecContext->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

	Stream->time_base.num = 1;
	Stream->time_base.den = OutputCodecContext->sample_rate;

	if (OutputFormatContext->oformat->flags & AVFMT_GLOBALHEADER)
	{
		OutputCodecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	}

	// open the encoder
	error = avcodec_open2(OutputCodecContext, Codec, nullptr);
	if (error < 0)
	{
		PrintError(error);
		Abort("Could not open output codec");
	}

	error = avcodec_parameters_from_context(Stream->codecpar, OutputCodecContext);
	if (error < 0)
	{
		PrintError(error);
		Abort("Could not init stream parameters");
	}	
}

void FFAudioEncoder::InitFifo()
{
	AudioFifo = av_audio_fifo_alloc(OutputCodecContext->sample_fmt, OutputCodecContext->channels, 1);
	if (!AudioFifo)
	{
		Abort("Could not allocate FIFO");
	}
}

void FFAudioEncoder::WriteOutputFileHeader()
{
	int error;
	error = avformat_write_header(OutputFormatContext, nullptr);
	if (error < 0)
	{
		PrintError(error);
		Abort("Could not write output file header");
	}
}

void FFAudioEncoder::Loop()
{
	const int OUTPUT_FRAME_SIZE = OutputCodecContext->frame_size;
	std::ifstream in_file{ InputFileName, std::ifstream::binary };
	// create buffer for holding data
	std::vector<uint8_t> Buffer(OUTPUT_FRAME_SIZE, 0);
	int offset = 0;

	// determine file size
	in_file.seekg(0, in_file.end);
	const int INPUT_FILE_SIZE = in_file.tellg();
	in_file.seekg(0, in_file.beg);

	//read data
	in_file.read(reinterpret_cast<char*>(Buffer.data()), OUTPUT_FRAME_SIZE);

	// ========================
	


	// ========================


	//d = (uint8_t*)Buffer.data();
	//AddSamplesToFifo(d);

	// nullify vector
	std::fill(Buffer.begin(), Buffer.end(), 0);


	//while (1)
	//{
	//	int finished = 0;
	//	
	//	// filling queue until we have 1 complete frame
	//	while (av_audio_fifo_size(AudioFifo) < OUTPUT_FRAME_SIZE)
	//	{
	//		
	//		//auto data = ReadSamplesFromInputFile();
	//		//AddSamplesToFifo(data);
	//	}

	//	// encode and write frame
	//	while (av_audio_fifo_size(AudioFifo) >= OUTPUT_FRAME_SIZE || (finished && av_audio_fifo_size(AudioFifo) > 0))
	//	{

	//	}

	//	if (finished)
	//	{
	//		break;
	//	}
	//}

	// todo:ashe23 write trailer
}

uint8_t* FFAudioEncoder::ReadSamplesFromInputFile()
{
	std::ifstream in_file{ InputFileName, std::ios::binary };
	


	return nullptr;
}

void FFAudioEncoder::AllocPacketAndFrame()
{
	Packet = av_packet_alloc();
	if (!Packet)
	{
		Abort("Could not allocate the packet");
	}

	Frame = av_frame_alloc();
	if (!Frame)
	{
		Abort("Could not allocate the frame");
	}

	// frame settings
	Frame->nb_samples = OutputCodecContext->frame_size;
	Frame->format = OutputCodecContext->sample_fmt;
	Frame->channel_layout = OutputCodecContext->channel_layout;

	if (av_frame_get_buffer(Frame, 0) < 0)
	{
		Abort("Could not allocate audio data buffers");
	}
}

// Stopping program execution and printing given error message
void FFAudioEncoder::Abort(const std::string ErrorMsg)
{
	if (ErrorMsg.size() > 0)
	{
		fprintf(stderr, "%s\n", ErrorMsg);
	}
	system("pause");
	exit(1);
}

// Realising Resources
void FFAudioEncoder::Cleanup()
{
	if (Frame)
	{
		av_frame_free(&Frame);
	}
	if (Packet)
	{
		av_packet_free(&Packet);
	}
	if (OutputCodecContext)
	{
		avcodec_free_context(&OutputCodecContext);
	}
	if (OutputFile)
	{
		fclose(OutputFile);
	}
}

void FFAudioEncoder::PrintError(int ErrorCode)
{
	char error_buf[256];
	av_make_error_string(error_buf, sizeof(error_buf), ErrorCode);
	fprintf(stderr, "%c\n", *error_buf);
}

void FFAudioEncoder::AddSamplesToFifo(uint8_t **data)
{
	int error;
	const int FRAME_SIZE = OutputCodecContext->frame_size;

	// allocate space
	error = av_audio_fifo_realloc(AudioFifo, av_audio_fifo_size(AudioFifo) + FRAME_SIZE);
	if (error < 0)
	{
		Abort("Could not reallocate FIFO");
	}

	// write data
	if (av_audio_fifo_write(AudioFifo, (void **)data, FRAME_SIZE) < FRAME_SIZE)
	{
		Abort("Could not write data to FIFO");
	}

}


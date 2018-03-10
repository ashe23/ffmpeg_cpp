#include "..\FFAudioEncoder.h"

FFAudioEncoder::~FFAudioEncoder()
{
	Cleanup();
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

	// initializing codecs and all ffmpeg stuff
	avcodec_register_all();

	// find mp3 encoder
	Codec = avcodec_find_encoder(AV_CODEC_ID_MP3);
	if (!Codec)
	{
		Abort("Codec not found!");
	}

	// allocating space for codec
	CodecContext = avcodec_alloc_context3(Codec);
	if (!CodecContext)
	{
		Abort("Could not allocate audio codec context");
	}

	// setting audio codec parameters
	CodecContext->bit_rate = BitRate;
	CodecContext->sample_fmt = CodecContext->codec->sample_fmts[0];
	CodecContext->sample_rate = SampleRate;
	CodecContext->channel_layout = AV_CH_LAYOUT_STEREO;
	CodecContext->channels = av_get_channel_layout_nb_channels(CodecContext->channel_layout);

	// trying to open codec
	if (avcodec_open2(CodecContext, Codec, nullptr) < 0)
	{
		Abort("Could not open codec");
	}

	OpenOutputFile();
	AllocPacketAndFrame();

	// ===============================================
	// here we starting to read input file and fill it to packet and encode save
	const int BUF_SIZE = CodecContext->frame_size;
	unsigned long offset = 0;
	unsigned long InputFileLength = 0;
	uint8_t *samples = nullptr;

	// creating buffer for holding data
	// getting file length
	input_file.seekg(0, input_file.end);
	InputFileLength = input_file.tellg();
	input_file.seekg(0, input_file.beg);

	printf("File length: %d\n", InputFileLength);

	while (offset <= InputFileLength) {
		// read new data to bufffer
		uint8_t * buffer = new uint8_t[BUF_SIZE];
		input_file.read(reinterpret_cast<char*>(buffer), BUF_SIZE);

		// Encoding data
		for (int i = 0; i < BUF_SIZE; ++i)
		{
			if (av_frame_make_writable(Frame) < 0)
			{
				Abort("Cant make writable frame");
			}

			samples = Frame->data[0];

			samples[2 * i] = buffer[i];
			samples[2 * i + 1] = buffer[i];

			EncodeFrame(false);

		}
		// change offset
		offset += BUF_SIZE;
		input_file.seekg(offset, input_file.cur);

		// clearing buffer
		delete [] buffer;
	}
	

	// ===============================================
	// flush encoder
	EncodeFrame(true);

	printf("Successfully encoded!\n");
}

void FFAudioEncoder::EncodeFrame(bool flush)
{
	if (flush)
	{
		Frame = nullptr;
	}

	int status_code;

	// sending frame for encoding
	status_code = avcodec_send_frame(CodecContext, Frame);
	if (status_code < 0)
	{
		Abort("Error sending the frame to the encoder");
	}

	while (status_code >= 0)
	{
		status_code = avcodec_receive_packet(CodecContext, Packet);
		if (status_code == AVERROR(EAGAIN) || status_code == AVERROR_EOF)
		{
			return;
		}
		else if (status_code < 0)
		{
			Abort("Error encoding audio frame");
		}

		// writing data to file
		fwrite(Packet->data, 1, Packet->size, OutputFile);

		// free packet
		av_packet_unref(Packet);
	}
}

void FFAudioEncoder::OpenOutputFile()
{
	OutputFile = fopen(OutputFileName.c_str(), "wb");
	if (!OutputFile)
	{
		Abort("Could not create file");
	}
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
	Frame->nb_samples = CodecContext->frame_size;
	Frame->format = CodecContext->sample_fmt;
	Frame->channel_layout = CodecContext->channel_layout;

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
	av_frame_free(&Frame);
	av_packet_free(&Packet);
	avcodec_free_context(&CodecContext);
	fclose(OutputFile);
}

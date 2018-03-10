#include "..\FFAudioEncoder.h"

FFAudioEncoder::~FFAudioEncoder()
{
	Cleanup();
	printf("Resources cleaned successfully.");
}

void FFAudioEncoder::StartEncode()
{
	// initializing codecs and all ffmpeg stuff
	avcodec_register_all();

	// find mp2 encoder
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
}

// Stopping program execution and printing given error message
void FFAudioEncoder::Abort(const std::string ErrorMsg)
{
	fprintf(stderr, "%s\n", ErrorMsg);
	system("pause");
	exit(1);
}

// Realising Resources
void FFAudioEncoder::Cleanup()
{
	avcodec_free_context(&CodecContext);
}

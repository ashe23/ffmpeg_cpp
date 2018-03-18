#pragma once

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
#include "libavutil/channel_layout.h"

#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
}
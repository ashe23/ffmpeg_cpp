#include "FFAudioEncoder.h"

int main()
{
	
	// getting pcm data and converting it to wav or mp3
	FFAudioEncoder AudioEncoder{"assets/audio/sample_s16.pcm", "assets/audio/output.mp3"};
	AudioEncoder.StartEncode();


	system("pause");
	return 0;
}
#include "FFAudioEncoder.h"

int main()
{
	
	// getting pcm data and converting it to wav or mp3
	FFAudioEncoder AudioEncoder{"assets/audio/input.pcm"};
	AudioEncoder.StartEncode();


	system("pause");
	return 0;
}
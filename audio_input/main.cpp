#include <iostream>
#include "audio_input.h"

using namespace std;
using namespace sound;

int main()
{
	// Default constructor launches default audio input device in 16bit, 44100kHz mode
	audio_input dev;

	cout << "Press ENTER to record sound..." << endl;
	cin.get();
	// Start recording
	dev.record();
	cout << "Recording..." << endl;
	cout << "Press ENTER to stop." << endl;
	cin.get();
	// Export recorded data to wav file
	dev.export_wav();
	cout << "Recorded sound is stored in tmp.wav file." << endl;

	// Destructor takes care of releasing the resources
	return 0;
}
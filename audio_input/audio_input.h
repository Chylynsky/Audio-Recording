#pragma once
#pragma comment (lib, "winmm")

#include "Windows.h"
#include <exception>
#include <stdexcept>
#include <fstream>
#include <vector>
#include <condition_variable>
#include <algorithm>
#include <mutex>
#include <thread>
#include <string>

namespace sound
{
	// Get a number of available audio-input devices.
	unsigned short audio_input_devices_count();
	// Get the name of a given audio-input device.
	std::string audio_input_device_name(const unsigned short device_id);

	class audio_input
	{
		using sample = char;
		using buffer = std::vector<sample>;

		enum class device_state {idle, recording};

		class wav_file
		{
			std::ofstream file;
			const audio_input* current_device;
			
			const unsigned short channels;
			const unsigned short pcm_resolution;
			const unsigned long sample_rate;
			const unsigned long data_transfer_rate;

			// Write bit words of specific sizes
			template <typename T>
			void write(T value, unsigned size = sizeof(T))
			{
				while (size)
				{
					file.put(static_cast<char> (value & 0xFF));
					value >>= 8;
					size--;
				}
			}

		public:

			wav_file(const audio_input* dev);
			wav_file(const audio_input* dev, std::string& file_name);
			~wav_file();

			void export_file();
		};

		const static unsigned int buffer_size;

		std::thread record_thread;
		std::mutex record_mutex;
		std::condition_variable buffer_control;

		buffer buffer1;
		buffer buffer2;
		buffer audio_block;

		HWAVEIN device_handle;
		WAVEFORMATEX format_params;
		WAVEHDR buffer1_params;
		WAVEHDR buffer2_params;
		WAVEINCAPS device_info;
		volatile device_state current_state;

		// Callback function called whenever buffers are full.
		static void _stdcall update_buffers(HWAVEIN hwi, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2);
		// Convert WinApi errors into exceptions.
		void handle_error(MMRESULT& err);
		// Handle recording audio data, copy it into "buffer audio_block". Runs on seperate std::thread, created in "void record()".
		void _record();
		
	public:

		// Opens audio-input device capable of 16-bit, 44.1kHz transmission on 1 channel. Throws if no such device is available.
		explicit audio_input(const unsigned int device_id = WAVE_MAPPER, const unsigned long sample_rate = 44100, const unsigned short bit_depth = 16, const unsigned short channels = 1);
		~audio_input();

		// Start recording audio input and save it into "buffer audio_block". Recording takes place on a seperate std::thread.
		void record();
		// Stop reading input data and clear "buffer audio_block".
		void reset();
		// Clear recorded data, doesn't stop recording
		void clear();
		// Stop recording, input data can be read safely after calling this function.
		void stop();
		// Get audio-input device's current sample rate.
		unsigned long get_sample_rate();
		// Get audio-input device's current bit_depth.
		unsigned short get_bit_depth();
		//
		std::wstring get_device_name();
		//
		buffer::iterator first_byte_iterator();
		//
		buffer::iterator last_byte_iterator();
		//
		sample* first_byte_ptr();
		//
		const unsigned int bytes_recorded();
		// Export "buffer audio_block" into tmp.wav file. Stops recording.
		void export_wav();
		// Export "buffer audio_block" into file_name.wav file. Stops recording. File extension .wav will be added in the process.
		void export_wav(std::string file_name);
	};

	// Exceptions

	struct bad_format : public std::runtime_error
	{
		bad_format(const char* msg);
		bad_format(const std::string& msg);
	};

	struct bad_device_id : public std::runtime_error
	{
		bad_device_id(const char* msg);
		bad_device_id(const std::string& msg);
	};
}
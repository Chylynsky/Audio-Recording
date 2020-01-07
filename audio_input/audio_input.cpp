#include "audio_input.h"

// Definitions for sound namespace methods

unsigned short sound::audio_input_devices_count()
{
	return waveInGetNumDevs();
}

std::string sound::audio_input_device_name(const unsigned short device_id)
{
	WAVEINCAPS tempAudioParams;

	// Get audio parameters
	MMRESULT err = waveInGetDevCaps(device_id, &tempAudioParams, sizeof(WAVEINCAPS));

	if (err == MMSYSERR_BADDEVICEID)
		throw bad_device_id("Specified device identifier is out of range.");
	else if (err == MMSYSERR_NODRIVER)
		throw std::runtime_error("No device driver is present.");
	else if (err == MMSYSERR_NOMEM)
		throw std::runtime_error("Unable to allocate or lock memory.");

	return tempAudioParams.szPname;
}

// Definitions for audio_input class methods
// Public:

sound::audio_input::audio_input(const unsigned int device_id, const unsigned long sample_rate, const unsigned short bit_depth, const unsigned short channels)
	: format_params{ WAVE_FORMAT_PCM, channels, sample_rate, static_cast<unsigned short>(sample_rate * channels * bit_depth / 8), static_cast<unsigned short>(channels * bit_depth / 8), bit_depth, 0 },
	buffer1{ buffer(buffer_size) }, buffer2{ buffer(buffer_size) }, buffer1_params{ buffer1.data(), buffer_size * sizeof(sample) }, buffer2_params{ buffer2.data(), buffer_size * sizeof(sample) },
	current_state{ device_state::idle }
{
	MMRESULT err = waveInGetDevCaps(WAVE_MAPPER, &device_info, sizeof(WAVEINCAPS));

	try { handle_error(err); }
	catch (std::runtime_error & e) { throw e; }

	// Open audio device
	err = waveInOpen(&device_handle, device_id, &format_params, (DWORD_PTR)update_buffers, (DWORD_PTR)this, CALLBACK_FUNCTION);
	
	try { handle_error(err); }
	catch (bad_format& e) { throw e; }
	catch (bad_device_id& e) { throw e; }
	catch (std::runtime_error& e) { throw e; }

	// Prepare the buffer for an audio input
	err = waveInPrepareHeader(device_handle, &buffer1_params, sizeof(WAVEHDR));

	try { handle_error(err); }
	catch (std::runtime_error& e) { throw e; }

	err = waveInPrepareHeader(device_handle, &buffer2_params, sizeof(WAVEHDR));

	try { handle_error(err); }
	catch (std::runtime_error & e) { throw e; }

	err = waveInAddBuffer(device_handle, &buffer1_params, sizeof(WAVEHDR));

	try { handle_error(err); }
	catch (std::runtime_error & e) { throw e; }

	err = waveInAddBuffer(device_handle, &buffer2_params, sizeof(WAVEHDR));

	try { handle_error(err); }
	catch (std::runtime_error & e) { throw e; }
}

sound::audio_input::~audio_input()
{
	current_state = device_state::idle;

	// Stop receiving audio data, mark buffers as done
	waveInReset(device_handle);

	// Wait for the record_thread to terminate
	if (record_thread.joinable())
		record_thread.join();

	// Clean up headers
	// Buffer 1:
	waveInUnprepareHeader(device_handle, &buffer1_params, sizeof(WAVEHDR));

	// Buffer 2:
	waveInUnprepareHeader(device_handle, &buffer2_params, sizeof(WAVEHDR));

	// Close audio device
	waveInClose(device_handle);
}

void sound::audio_input::record()
{
	if (current_state == device_state::idle)
	{
		// Start receiving audio data
		MMRESULT err = waveInStart(device_handle);

		try { handle_error(err); }
		catch (std::runtime_error & e) { throw e; }

		current_state = device_state::recording;

		// Create a seperate thread for recording
		record_thread = std::thread(&audio_input::_record, this);
	}
	else return;
}

void sound::audio_input::reset()
{
	// Stop receiving audio data, mark buffers as done
	MMRESULT err = waveInReset(device_handle);

	try { handle_error(err); }
	catch (std::runtime_error & e) { throw e; }

	if (current_state == device_state::recording)
	{
		current_state = device_state::idle;

		if (record_thread.joinable())
			record_thread.join();
	}

	audio_block.clear();
}

void sound::audio_input::clear()
{
	audio_block.clear();
}

void sound::audio_input::stop()
{
	// Stop receiving audio data
	MMRESULT err = waveInReset(device_handle);

	try { handle_error(err); }
	catch (std::runtime_error & e) { throw e; }

	if (current_state == device_state::recording)
	{
		current_state = device_state::idle;

		if (record_thread.joinable())
			record_thread.join();
	}
	else return;
}

unsigned long sound::audio_input::get_sample_rate()
{
	return format_params.nSamplesPerSec;
}

unsigned short sound::audio_input::get_bit_depth()
{
	return format_params.wBitsPerSample;
}

std::wstring sound::audio_input::get_device_name()
{
	return reinterpret_cast<wchar_t*>(device_info.szPname);
}

std::vector<char>::iterator sound::audio_input::first_byte_iterator()
{
	return audio_block.begin();
}

std::vector<char>::iterator sound::audio_input::last_byte_iterator()
{
	return audio_block.end();
}

char* sound::audio_input::first_byte_ptr()
{
	return audio_block.data();
}

const unsigned int sound::audio_input::bytes_recorded()
{
	return audio_block.size();
}

void sound::audio_input::export_wav()
{
	stop();

	try 
	{
		wav_file file(this);
		file.export_file();
	}
	catch (std::runtime_error& e)
	{
		throw e;
	}
}

void sound::audio_input::export_wav(std::string file_name)
{
	stop();

	try
	{
		wav_file file(this, file_name);
		file.export_file();
	}
	catch (std::runtime_error& e)
	{
		throw e;
	}
}

// Private:

const unsigned int sound::audio_input::buffer_size{ 4096 };

void _stdcall sound::audio_input::update_buffers(HWAVEIN hwi, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
	audio_input* current_device = reinterpret_cast<audio_input*>(dwInstance);

	if (uMsg == WIM_DATA)
		current_device->buffer_control.notify_one();
}

void sound::audio_input::handle_error(MMRESULT& err)
{
	if (err == MMSYSERR_INVALHANDLE)
		throw std::runtime_error("Specified device handle is invalid.");
	else if (err == MMSYSERR_NODRIVER)
		throw std::runtime_error("No device driver is present.");
	else if (err == MMSYSERR_NOMEM)
		throw std::runtime_error("Unable to allocate or lock memory.");
	else if (err == WAVERR_UNPREPARED)
		throw std::runtime_error("The buffer pointed to by the pwh parameter hasn't been prepared.");
	else if (err == MMSYSERR_ALLOCATED)
		throw std::runtime_error("Specified resource is already allocated.");
	else if (err == MMSYSERR_BADDEVICEID)
		throw bad_device_id("Specified device identifier is out of range.");
	else if (err == WAVERR_BADFORMAT)
		throw bad_format("Attempted to open with an unsupported waveform-audio format.");
	else return;
}

void sound::audio_input::_record()
{	
	while (current_state == device_state::recording)
	{
		std::unique_lock<std::mutex> record_lock(record_mutex);

		buffer_control.wait(record_lock, [&]{ return ((buffer1_params.dwFlags & 0x1) == WHDR_DONE); });
		audio_block.insert(audio_block.end(), buffer1.begin(), buffer1.begin() + buffer1_params.dwBytesRecorded);
		waveInAddBuffer(device_handle, &buffer1_params, sizeof(WAVEHDR));

		buffer_control.wait(record_lock, [&]{ return ((buffer2_params.dwFlags & 0x1) == WHDR_DONE); });
		audio_block.insert(audio_block.end(), buffer2.begin(), buffer2.begin() + buffer2_params.dwBytesRecorded);
		waveInAddBuffer(device_handle, &buffer2_params, sizeof(WAVEHDR));
	}
}

// Definitions for audio_input::wav_file class methods

sound::audio_input::wav_file::wav_file(const audio_input* dev) : current_device{ dev }, channels{ dev->format_params.nChannels }, 
pcm_resolution{ dev->format_params.wBitsPerSample }, sample_rate{ dev->format_params.nSamplesPerSec }, data_transfer_rate{ dev->format_params.nAvgBytesPerSec }
{
	file.open("tmp.wav", std::ios::binary);

	if (!file.good())
		throw std::runtime_error("Could not open tmp.wav.");
}

sound::audio_input::wav_file::wav_file(const audio_input* dev, std::string& file_name) : current_device{ dev }, channels{ dev->format_params.nChannels }, 
pcm_resolution{ dev->format_params.wBitsPerSample }, sample_rate{ dev->format_params.nSamplesPerSec }, data_transfer_rate{ dev->format_params.nAvgBytesPerSec }
{
	std::string extension = ".wav";

	if (!std::equal(file_name.end() - extension.size(), file_name.end(), extension.begin(), extension.end()))
		file_name += extension;

	file.open(file_name, std::ios::binary);

	if (!file.good())
		throw std::runtime_error("Could not open " + file_name + ".");
}

sound::audio_input::wav_file::~wav_file()
{
	file.close();
}

void sound::audio_input::audio_input::wav_file::export_file()
{
	// Write the file headers
	file << "RIFF----WAVEfmt ";
	write(16, 4);
	write(1, 2);
	write(channels, 2);
	write(sample_rate, 4);
	write(data_transfer_rate, 4);  // (Sample Rate * BitsPerSample * Channels) / 8
	write(channels * pcm_resolution / 8, 2);  // size of two integer samples, one for each channel, in bytes
	write(pcm_resolution, 2);

	// Write the data chunk header
	unsigned long data_chunk_pos = static_cast<unsigned long>(file.tellp());
	file << "data----";  // (chunk size to be filled in later)

	for (unsigned long i = 0; i < current_device->audio_block.size(); i++)
		write(current_device->audio_block[i], 1);

	// (We'll need the final file size to fix the chunk sizes above)
	unsigned long file_length = static_cast<unsigned long>(file.tellp());

	// Fix the data chunk header to contain the data size
	file.seekp(data_chunk_pos + static_cast<unsigned long>(4));
	write(file_length - data_chunk_pos + static_cast<unsigned long>(8));

	// Fix the file header to contain the proper RIFF chunk size, which is (file size - 8) bytes
	file.seekp(4);
	write(file_length - static_cast<unsigned long>(8), 4);
}

// Definitions for exceptions

sound::bad_format::bad_format(const char* msg) : std::runtime_error(msg)
{

}

sound::bad_format::bad_format(const std::string& msg) : std::runtime_error(msg)
{

}

sound::bad_device_id::bad_device_id(const char* msg) : std::runtime_error(msg)
{

}

sound::bad_device_id::bad_device_id(const std::string& msg) : std::runtime_error(msg)
{

}
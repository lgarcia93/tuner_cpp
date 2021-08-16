#pragma once

#include <audioclient.h>
#include <audiopolicy.h>
#include <mmdeviceapi.h>
#include <combaseapi.h>
#include <vector>
#include <iostream>
#include <future>

#define REFTIMES_PER_SEC  10000000
#define REFTIMES_PER_MILLISEC  10000

class AudioManager
{
private:
	IMMDeviceEnumerator* _deviceEnumerator = nullptr;
	IMMDevice* _defaultInputDevice = nullptr;
	IMMDevice* _defaultOutputDevice = nullptr;
	IAudioClient* _audioInputClientInterface = nullptr;
	IAudioClient* _audioOutputClientInterface = nullptr;

	std::mutex currentFrequencyMutex;

	double currentFrequency = 0;

	void EnumAudioEndpoints(const EDataFlow& dataFlow, IMMDeviceCollection*& p_deviceCollection);
	std::vector<IMMDevice*> GetAllDevicesByType(const EDataFlow& dataFlow);

public:
	AudioManager() {
		HRESULT result = CoInitialize(NULL);

		result = CoCreateInstance(
			__uuidof(MMDeviceEnumerator),
			NULL,
			CLSCTX_ALL,
			__uuidof(IMMDeviceEnumerator),
			(void**)&_deviceEnumerator
		);
	};

	void ActivateDefaultAudioInput();
	void ActivateDefaultAudioOutput();
	const std::string GetDeviceFriendlyName(IMMDevice* device);

	std::vector<IMMDevice*> GetAllInputDevices();
	std::vector<IMMDevice*> GetAllOutputDevices();

	void GenerateWhiteNoise();
	void StartListeningAudio();
	void SetCurrentFrequency(double newFrequency);
	double GetCurrentFrequency();
};


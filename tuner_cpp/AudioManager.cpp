#pragma warning(disable: 4211)
#pragma warning(disable: 4996)
#pragma warning(disable: 4189)
#pragma warning(disable: 4018)



#include "AudioManager.h"
#include <Functiondiscoverykeys_devpkey.h>
#include <codecvt>
#include <locale>
#include <memory>
#include <cmath>
#include <vector>
#include "vendor/fftw3/include/fftw3.h"
#include <bit>
#include <unordered_map>

#define REAL 0
#define IMAG 1

#define ANALYZE_BUFFER_SIZE 16384

REFERENCE_TIME hnsRequestedDuration = REFTIMES_PER_SEC;
#define TWO_PI (3.14159265359 * 2)

double* window = new double[ANALYZE_BUFFER_SIZE];

void buildHanWindow()
{
	for (int i = 0; i < ANALYZE_BUFFER_SIZE; ++i)
		window[i] = .5 * (1 - cos(TWO_PI * i / (ANALYZE_BUFFER_SIZE - 1.0)));
}
void applyWindow(std::vector<double>& data)
{
	for (int i = 0; i < ANALYZE_BUFFER_SIZE; ++i)
		data[i] *= (double)window[i];
}

void fft(fftw_complex* in, fftw_complex* out)
{
	fftw_plan plan = fftw_plan_dft_1d(ANALYZE_BUFFER_SIZE, in, out, FFTW_FORWARD, FFTW_ESTIMATE);

	fftw_execute(plan);

	fftw_destroy_plan(plan);
	fftw_cleanup();
}

static const  IID  IID_IAudioClient = {
	//MIDL_INTERFACE("1CB9AD4C-DBFA-4c32-B178-C2F568A703B2")
	0x1cb9ad4c, 0xdbfa, 0x4c32, {0xb1, 0x78, 0xc2, 0xf5, 0x68, 0xa7, 0x03, 0xb2}

};

static const IID IID_IAudioCaptureClient = {
	//MIDL_INTERFACE("C8ADBD64-E71E-48a0-A4DE-185C395CD317")
	0xc8adbd64, 0xe71e, 0x48a0, {0xa4, 0xde, 0x18, 0x5c, 0x39, 0x5c, 0xd3, 0x17}
};

static const IID   IID_IAudioRenderClient = {
	//MIDL_INTERFACE("F294ACFC-3146-4483-A7BF-ADDCA7C260E2")
	0xf294acfc, 0x3146, 0x4483, {0xa7, 0xbf, 0xad, 0xdc, 0xa7, 0xc2, 0x60, 0xe2}
};

std::string convert_to_string(const std::wstring& wstr)
{
	using convert_typeX = std::codecvt_utf16<wchar_t>;
	std::wstring_convert<convert_typeX, wchar_t> converterX;

	return converterX.to_bytes(wstr);
}

void AudioManager::ActivateDefaultAudioInput()
{
	if (_defaultInputDevice == NULL) {
		_deviceEnumerator->GetDefaultAudioEndpoint(EDataFlow::eCapture, ERole::eConsole, &_defaultInputDevice);
	}

	std::cout << "Choosen Input: " << GetDeviceFriendlyName(_defaultInputDevice) << std::endl;

	_defaultInputDevice->Activate(
		IID_IAudioClient,
		CLSCTX_ALL,
		NULL,
		(void**)&_audioInputClientInterface
	);

}

void AudioManager::ActivateDefaultAudioOutput()
{
	if (_defaultOutputDevice == NULL) {
		_deviceEnumerator->GetDefaultAudioEndpoint(EDataFlow::eRender, ERole::eConsole, &_defaultOutputDevice);

		//_deviceEnumerator->Release();
	}

	std::cout << "Choosen Output: " << GetDeviceFriendlyName(_defaultOutputDevice) << std::endl;

	_defaultOutputDevice->Activate(
		IID_IAudioClient,
		CLSCTX_ALL,
		NULL,
		(void**)&_audioOutputClientInterface
	);
}

const std::string AudioManager::GetDeviceFriendlyName(IMMDevice* device)
{
	IPropertyStore* property_store = nullptr;

	HRESULT result = device->OpenPropertyStore(STGM_READ, &property_store);

	PROPVARIANT varName;

	PropVariantInit(&varName);

	result = property_store->GetValue(
		PKEY_Device_FriendlyName,
		&varName
	);

	const std::string deviceName = convert_to_string(varName.pwszVal);

	return deviceName;
}


void AudioManager::EnumAudioEndpoints(const EDataFlow& dataFlow, IMMDeviceCollection*& p_deviceCollection)
{
	HRESULT result = _deviceEnumerator->EnumAudioEndpoints(
		dataFlow,
		DEVICE_STATE_ACTIVE,
		&p_deviceCollection
	);
}

std::vector<IMMDevice*> AudioManager::GetAllDevicesByType(const EDataFlow& dataFlow)
{
	IMMDeviceCollection* p_deviceCollection = NULL;
	std::vector<IMMDevice*> devices;

	EnumAudioEndpoints(dataFlow, p_deviceCollection);

	unsigned int deviceCount;

	HRESULT result = p_deviceCollection->GetCount(&deviceCount);

	for (unsigned int i = 0; i < deviceCount; i++) {

		IMMDevice* device;

		p_deviceCollection->Item(i, &device);

		devices.push_back(device);
	}

	return devices;
}


std::vector<IMMDevice*> AudioManager::GetAllInputDevices()
{
	return GetAllDevicesByType(EDataFlow::eCapture);
}

std::vector<IMMDevice*> AudioManager::GetAllOutputDevices()
{
	return GetAllDevicesByType(EDataFlow::eRender);
}

void AudioManager::StartListeningAudio()
{
	ActivateDefaultAudioInput();

	DWORD flags = 0;

	REFERENCE_TIME requestedSoundBufferDuration = REFTIMES_PER_SEC * 2;
	DWORD initStreamFlags = (AUDCLNT_STREAMFLAGS_RATEADJUST
		| AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM
		| AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY);

	WAVEFORMATEX mixFormat = {};
	mixFormat.wFormatTag = WAVE_FORMAT_PCM;
	mixFormat.nChannels = 1;
	mixFormat.nSamplesPerSec = 44100;
	mixFormat.wBitsPerSample = 16;
	mixFormat.nBlockAlign = (mixFormat.nChannels * mixFormat.wBitsPerSample) / 8;
	mixFormat.nAvgBytesPerSec = mixFormat.nSamplesPerSec * mixFormat.nBlockAlign;

	HRESULT result = _audioInputClientInterface->Initialize(
		AUDCLNT_SHAREMODE_SHARED,
		initStreamFlags,
		requestedSoundBufferDuration,
		0,
		&mixFormat,
		NULL
	);

	IAudioCaptureClient* p_audioCaptureClient;

	_audioInputClientInterface->GetService(
		IID_IAudioCaptureClient,
		(void**)&p_audioCaptureClient
	);

	_audioInputClientInterface->Start();

	UINT32 bufferSize;
	HRESULT hr = _audioInputClientInterface->GetBufferSize(&bufferSize);

	std::vector<double> analyzeBuffer;

	int totalSamples = 0;

	//input
	fftw_complex input_fft[ANALYZE_BUFFER_SIZE];

	//output
	fftw_complex output_fft[ANALYZE_BUFFER_SIZE];

	buildHanWindow();

	while (true) {
		UINT32 bufferPadding;

		hr = _audioInputClientInterface->GetCurrentPadding(&bufferPadding);

		UINT32 frameCount = bufferSize - bufferPadding;

		int16_t* buffer;

		hr = p_audioCaptureClient->GetBuffer(
			(BYTE**)(&buffer),
			&frameCount,
			&flags,
			NULL,
			NULL
		);

		for (int i = 0; i < frameCount; i++) {
			if (totalSamples < ANALYZE_BUFFER_SIZE) {
				analyzeBuffer.push_back(double(buffer[i] / 32.767));
				totalSamples++;
			}
		}

		if (totalSamples >= ANALYZE_BUFFER_SIZE)
		{
			totalSamples = 0;

			for (int i = 0; i < ANALYZE_BUFFER_SIZE; ++i)
			{
				input_fft[i][REAL] = analyzeBuffer[i];
				input_fft[i][IMAG] = 0;
			}

			fft(input_fft, output_fft);

			for (int i = 0; i < ANALYZE_BUFFER_SIZE; ++i)
				analyzeBuffer[i] *= window[i];

			double predominantFreq = 0;
			double predominantCnt = 0;
			double highestMag = 0;

			std::unordered_map<double, int> freqs;

			struct FrequencyInfo {
				double Frequency;
				double Magnitude;
			};

			for (int i = 0; i < ANALYZE_BUFFER_SIZE / 2; i++)
			{
				double mag = (output_fft[i][REAL] * output_fft[i][REAL]) + (output_fft[i][IMAG] * output_fft[i][IMAG]);
				double freq = i * mixFormat.nSamplesPerSec / ANALYZE_BUFFER_SIZE / 2;

				if (mag > highestMag)
				{

					SetCurrentFrequency(freq);

					highestMag = mag;
					predominantFreq = freq;

					freqs[freq] = freqs[freq]++;

					if (freqs[freq] > predominantCnt) {
						predominantCnt = freqs[freq];
						predominantFreq = freq;
					}
				}
			}

			analyzeBuffer.clear();
		}

		hr = p_audioCaptureClient->ReleaseBuffer(frameCount);
	}



	_audioInputClientInterface->Stop();
	_audioInputClientInterface->Release();
	p_audioCaptureClient->Release();
}

void AudioManager::GenerateWhiteNoise()
{
	ActivateDefaultAudioOutput();

	DWORD flags = 0;

	REFERENCE_TIME requestedSoundBufferDuration = REFTIMES_PER_SEC * 2;
	DWORD initStreamFlags = (AUDCLNT_STREAMFLAGS_RATEADJUST
		| AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM
		| AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY);

	WAVEFORMATEX mixFormat = {};
	mixFormat.wFormatTag = WAVE_FORMAT_PCM;
	mixFormat.nChannels = 2;
	mixFormat.nSamplesPerSec = 44100;
	mixFormat.wBitsPerSample = 16;
	mixFormat.nBlockAlign = (mixFormat.nChannels * mixFormat.wBitsPerSample) / 8;
	mixFormat.nAvgBytesPerSec = mixFormat.nSamplesPerSec * mixFormat.nBlockAlign;

	HRESULT result = _audioOutputClientInterface->Initialize(
		AUDCLNT_SHAREMODE_SHARED,
		initStreamFlags,
		requestedSoundBufferDuration,
		0,
		&mixFormat,
		NULL
	);

	IAudioRenderClient* p_audioRenderClient;

	_audioOutputClientInterface->GetService(
		IID_IAudioRenderClient,
		(LPVOID*)&p_audioRenderClient
	);

	_audioOutputClientInterface->Start();

	UINT32 bufferSize;
	HRESULT hr = _audioOutputClientInterface->GetBufferSize(&bufferSize);

	double playbackTime = 0.0;

	const float TONE_HZ = 440;

	const int16_t TONE_VOLUME = 5000;

	while (true)
	{
		UINT32 bufferPadding;

		hr = _audioOutputClientInterface->GetCurrentPadding(&bufferPadding);

		UINT32 frameCount = bufferSize - bufferPadding;

		int16_t* buffer;
		hr = p_audioRenderClient->GetBuffer(frameCount, (BYTE**)(&buffer));

		for (UINT32 frameIndex = 0; frameIndex < frameCount; frameIndex++)
		{
			float amplitude = (float)sin(playbackTime * TWO_PI * TONE_HZ) + (float)sin(playbackTime * TWO_PI * 554.37) + (float)sin(playbackTime * TWO_PI * 659.25);

			//	std::cout << amplitude << std::endl;
			int16_t y = (int16_t)(TONE_VOLUME * amplitude);


			*(buffer++) = y; // left
			*(buffer++) = y; // right

			playbackTime += 1.f / mixFormat.nSamplesPerSec;
		}



		hr = p_audioRenderClient->ReleaseBuffer(frameCount, 0);
	}

	_audioOutputClientInterface->Stop();
	_audioOutputClientInterface->Release();
	p_audioRenderClient->Release();
}

void AudioManager::SetCurrentFrequency(double newFrequency)
{
	std::lock_guard<std::mutex> lock(currentFrequencyMutex);

	currentFrequency = newFrequency;

}

double AudioManager::GetCurrentFrequency()
{
	std::lock_guard<std::mutex> lock(currentFrequencyMutex);
	return currentFrequency;
}
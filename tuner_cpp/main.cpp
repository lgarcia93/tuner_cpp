// CppStudies.cpp : Este arquivo cont�m a fun��o 'main'. A execu��o do programa come�a e termina ali.
//

#include <iostream>
#include <memory>
#include <audioclient.h>
#include <audiopolicy.h>
#include <mmdeviceapi.h>
#include <combaseapi.h>
#include <vector>
#include <Functiondiscoverykeys_devpkey.h>
#include <codecvt>
#include <locale>
#include "AudioManager.h"
#include <future>
#include <thread>
#include <functional>

std::unique_ptr<AudioManager> audioManager = std::make_unique<AudioManager>();

int main()
{
	while (true) {
		Sleep(100);
		std::cout << "Getting freq " << std::endl;

		std::cout << "Freq: " << audioManager->GetCurrentFrequency() << std::endl;
	}

	std::cin.get();
}


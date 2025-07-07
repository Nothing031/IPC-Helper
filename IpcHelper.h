#pragma once
#include <memory>
#include <string>
#include <Windows.h>

class IpcHelper
{
public:
	class Sender;
	class Reader;
};

class IpcHelper::Sender
{
public:
	inline static bool Init(const char* name, int mapSize)
	{
		if (initialized) return true;
		size = mapSize;
		std::string mutexName = std::string(name) + ".mutex";
		std::string eventName = std::string(name) + ".event";
		std::string mapFileName = std::string(name) + ".mapfile";

		hMutex = CreateMutexA(NULL, FALSE, mutexName.c_str());
		hEvent = CreateEventA(NULL, TRUE, FALSE, eventName.c_str());
		HANDLE hMapFile = CreateFileMappingA(
			INVALID_HANDLE_VALUE,
			NULL,
			PAGE_READWRITE,
			0,
			size,
			mapFileName.c_str()
		);
		if (hMutex == INVALID_HANDLE_VALUE || hEvent == INVALID_HANDLE_VALUE || hMapFile == INVALID_HANDLE_VALUE)
			return false;
		
		pBuffer = MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, size);
		if (pBuffer == NULL)
			return false;
		ZeroMemory(pBuffer, size);

		initialized = true;
		return true;
	}

	inline static int Size()
	{
		if (!initialized) [[unlikely]] return NULL;
		return size;
	}

	inline static LPVOID Buffer()
	{
		if (!initialized) [[unlikely]] return NULL;
		return pBuffer;
	}

	inline static void SendData(void* src, int srcSize)
	{
		if (!initialized || !src) [[unlikely]] return;

		WaitForSingleObject(hMutex, INFINITE);
		memcpy(pBuffer, src, srcSize);
		SetEvent(hEvent);
		ReleaseMutex(hMutex);
	}
private:
	static bool initialized;
	static int size;
	static LPVOID pBuffer;
	static HANDLE hEvent;
	static HANDLE hMutex;
	static HANDLE hMapFile;
};

class IpcHelper::Reader
{
public:
	inline static bool Init(const char* name, int mapSize)
	{
		if (initialized) return true;
		size = mapSize;
		std::string mutexName = std::string(name) + ".mutex";
		std::string eventName = std::string(name) + ".event";
		std::string mapFileName = std::string(name) + ".mapfile";

		hMutex = OpenMutexA(SYNCHRONIZE | EVENT_ALL_ACCESS, FALSE, mutexName.c_str());
		hEvent = OpenEventA(SYNCHRONIZE | EVENT_ALL_ACCESS, FALSE, eventName.c_str());

		HANDLE hMapFile = OpenFileMappingA(
			FILE_MAP_ALL_ACCESS,
			FALSE,
			mapFileName.c_str()
		);

		if (hMutex == INVALID_HANDLE_VALUE || hEvent == INVALID_HANDLE_VALUE || hMapFile == INVALID_HANDLE_VALUE)
			return false;

		pBuffer = MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, size);
		if (pBuffer == NULL)
			return false;

		initialized = true;
		return true;
	}

	inline static void QuitWaiting()
	{
		if (!initialized) [[unlikely]] return;

		SetEvent(hEvent);
	}

	inline static void WaitAndReadData(void* dest, int destSize)
	{
		if (!initialized || !dest) [[unlikely]] return;
		
		WaitForSingleObject(hEvent, INFINITE);
		WaitForSingleObject(hMutex, INFINITE);
		memcpy(dest, pBuffer, destSize);
		ResetEvent(hEvent);
		ReleaseMutex(hMutex);
	}

	inline static int Size()
	{
		size;
	}

	inline static LPVOID Buffer()
	{
		pBuffer;
	}

private:
	static bool initialized;
	static int size;
	static LPVOID pBuffer;
	static HANDLE hEvent;
	static HANDLE hMutex;
	static HANDLE hMapFile;
};

bool IpcHelper::Sender::initialized = false;
int IpcHelper::Sender::size = 0;
LPVOID IpcHelper::Sender::pBuffer = NULL;
HANDLE IpcHelper::Sender::hEvent = INVALID_HANDLE_VALUE;
HANDLE IpcHelper::Sender::hMutex = INVALID_HANDLE_VALUE;
HANDLE IpcHelper::Sender::hMapFile = INVALID_HANDLE_VALUE;

bool IpcHelper::Reader::initialized = false;
int IpcHelper::Reader::size = 0;
LPVOID IpcHelper::Reader::pBuffer = NULL;
HANDLE IpcHelper::Reader::hEvent = INVALID_HANDLE_VALUE;
HANDLE IpcHelper::Reader::hMutex = INVALID_HANDLE_VALUE;
HANDLE IpcHelper::Reader::hMapFile = INVALID_HANDLE_VALUE;

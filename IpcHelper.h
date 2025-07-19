#pragma once
#include <memory>
#include <string>
#include <ctime>
#include <Windows.h>

namespace IpcHelper
{
	enum EWaitResult {
		IPC_WAIT_TIMEOUT,
		IPC_WAIT_FAILED,
		IPC_WAIT_SUCCESS,
		IPC_WAIT_QUIT,
		IPC_WAIT_SHUTDOWN,
	};

	class SharedMemoryHelper
	{
	public:
		~SharedMemoryHelper();
		SharedMemoryHelper(const SharedMemoryHelper&) = delete;
		SharedMemoryHelper& operator=(const SharedMemoryHelper&) = delete;

		static bool isExist(std::wstring);
		static bool openSharedMemory(std::wstring str, uint32_t size, std::unique_ptr<SharedMemoryHelper>* helperOut);
		static bool createSharedMemory(std::wstring str, uint32_t size, std::unique_ptr<SharedMemoryHelper>* helperOut);

		// info
		int size();

		// synchronous
		void quitWaiting();
		void lock();
		void unlock();

		// motify
		IpcHelper::EWaitResult waitWriteData(void* dst, uint32_t dstSize, uint32_t timeout = INFINITE);
		IpcHelper::EWaitResult waitReadData(void* src, uint32_t srcSize, uint32_t timeout = INFINITE);

		// data
		void* rawBuffer();
	protected:
		SharedMemoryHelper();

	private:
		void CleanUp();

		bool m_initialized;
		int m_size;
		LPVOID m_pBuffer;
		HANDLE m_hBufferMutex;
		HANDLE m_hBufferEvent;
		HANDLE m_hBufferMapFile;
		HANDLE m_hShutdownEvent;
		HANDLE m_hInstanceQuitWaitingEvent;
	};
}

bool IpcHelper::SharedMemoryHelper::isExist(std::wstring str)
{
	HANDLE mutex = OpenMutexW(SYNCHRONIZE | MUTEX_ALL_ACCESS, NULL, std::wstring(str + L".buffer.mutex").c_str());
	if (mutex)
	{
		CloseHandle(mutex);
		return true;
	}
	else
	{
		return false;
	}
}

bool IpcHelper::SharedMemoryHelper::openSharedMemory(std::wstring str, uint32_t size, std::unique_ptr<SharedMemoryHelper>* helperOut)
{
	std::unique_ptr<SharedMemoryHelper> helper(new SharedMemoryHelper());
	std::wstring mapFileName = std::wstring(str) + L".buffer.mapfile";
	std::wstring bufferMutexName = std::wstring(str) + L".buffer.mutex";
	std::wstring bufferEventName = std::wstring(str) + L".buffer.event.data";
	std::wstring shutdownName = std::wstring(str) + L".buffer.event.shutdown";
	// mapfile
	helper->m_hBufferMapFile = OpenFileMappingW(
		FILE_MAP_ALL_ACCESS,
		FALSE,
		mapFileName.c_str()
	);
	if (helper->m_hBufferMapFile == NULL)
		goto Failed;
	// buffer
	helper->m_pBuffer = MapViewOfFile(helper->m_hBufferMapFile, FILE_MAP_ALL_ACCESS, 0, 0, size);
	if (helper->m_pBuffer == NULL)
		goto Failed;
	// buffer mutex, event
	helper->m_hBufferMutex = OpenMutexW(MUTEX_ALL_ACCESS, FALSE, bufferMutexName.c_str());
	helper->m_hBufferEvent = OpenEventW(EVENT_ALL_ACCESS, FALSE, bufferEventName.c_str());
	// shutdown event
	helper->m_hShutdownEvent = OpenEventW(EVENT_ALL_ACCESS, FALSE, shutdownName.c_str());
	if (helper->m_hBufferMutex == NULL || helper->m_hBufferEvent == NULL || helper->m_hShutdownEvent == NULL)
		goto Failed;

Success:
	helper->m_initialized = true;
	(*helperOut) = std::move(helper);
	return true;

Failed:
	(*helperOut) = nullptr;
	return false;
}

bool IpcHelper::SharedMemoryHelper::createSharedMemory(std::wstring str, uint32_t size, std::unique_ptr<SharedMemoryHelper>* helperOut)
{
	std::unique_ptr<SharedMemoryHelper> helper(new SharedMemoryHelper());
	std::wstring mapFileName = std::wstring(str) + L".buffer.mapfile";
	std::wstring bufferMutexName = std::wstring(str) + L".buffer.mutex";
	std::wstring bufferEventName = std::wstring(str) + L".buffer.event.data";
	std::wstring shutdownName = std::wstring(str) + L".buffer.event.shutdown";

	if (isExist(str))
		goto Failed;

	helper->m_hBufferMapFile = CreateFileMappingW(
		INVALID_HANDLE_VALUE,
		NULL,
		PAGE_READWRITE,
		0,
		size,
		mapFileName.c_str()
	);
	if (helper->m_hBufferMapFile == NULL || GetLastError() == ERROR_ALREADY_EXISTS)
		goto Failed;
	// buffer
	helper->m_pBuffer = MapViewOfFile(helper->m_hBufferMapFile, FILE_MAP_ALL_ACCESS, 0, 0, size);
	if (helper->m_pBuffer == NULL)
		goto Failed;
	// buffer mutex, evvent
	helper->m_hBufferMutex = CreateMutexW(NULL, FALSE, bufferMutexName.c_str());
	if (helper->m_hBufferMutex == NULL || GetLastError() == ERROR_ALREADY_EXISTS)
		goto Failed;
	helper->m_hBufferEvent = CreateEventW(NULL, FALSE, FALSE, bufferEventName.c_str());
	if (helper->m_hBufferEvent == NULL || GetLastError() == ERROR_ALREADY_EXISTS)
		goto Failed;
	// shutdown event
	helper->m_hShutdownEvent = CreateEventW(NULL, FALSE, FALSE, shutdownName.c_str());
	if (helper->m_hShutdownEvent == NULL || GetLastError() == ERROR_ALREADY_EXISTS)
		goto Failed;

Success:
	helper->m_initialized = true;
	(*helperOut) = std::move(helper);
	return true;

Failed:
	(*helperOut) = nullptr;
	return false;
}

void IpcHelper::SharedMemoryHelper::quitWaiting()
{
	SetEvent(m_hInstanceQuitWaitingEvent);
}

void IpcHelper::SharedMemoryHelper::lock()
{
	WaitForSingleObject(m_hBufferMutex, INFINITE);
}

void IpcHelper::SharedMemoryHelper::unlock()
{
	ReleaseMutex(m_hBufferMutex);
}

IpcHelper::EWaitResult IpcHelper::SharedMemoryHelper::waitWriteData(void* src, uint32_t srcSize, uint32_t timeout)
{
	if (!m_initialized) [[unlikely]]
		return EWaitResult::IPC_WAIT_FAILED;

	HANDLE waitHandles[2] = { m_hBufferMutex, m_hInstanceQuitWaitingEvent };
	DWORD dwResult = WaitForMultipleObjects(2, waitHandles, FALSE, timeout);
	switch (dwResult)
	{
	case WAIT_OBJECT_0:		// got mutex
		memcpy(m_pBuffer, src, srcSize);
		SetEvent(m_hBufferEvent);
		ReleaseMutex(m_hBufferMutex);
		return EWaitResult::IPC_WAIT_SUCCESS;
	case WAIT_OBJECT_0 + 1:	// quit waiting
		ResetEvent(m_hInstanceQuitWaitingEvent);
		return EWaitResult::IPC_WAIT_QUIT;
	case WAIT_TIMEOUT:
		return EWaitResult::IPC_WAIT_TIMEOUT;
	default:
		return EWaitResult::IPC_WAIT_FAILED;
	}
	return EWaitResult::IPC_WAIT_FAILED;
}

IpcHelper::EWaitResult IpcHelper::SharedMemoryHelper::waitReadData(void* dst, uint32_t dstSize, uint32_t timeout)
{
	if (!m_initialized) [[unlikely]]
		return EWaitResult::IPC_WAIT_FAILED;

	uint32_t waitBeginTime, waitElapsedTime, nextWaitTime;
	waitBeginTime = clock();
	HANDLE waitHandles[2] = { m_hBufferEvent, m_hInstanceQuitWaitingEvent };
	DWORD dwResult = WaitForMultipleObjects(2, waitHandles, FALSE, timeout);
	waitElapsedTime = clock() - waitBeginTime;
	nextWaitTime = timeout - waitElapsedTime > 0
		? timeout - waitElapsedTime
		: 0;

	EWaitResult waitResult = EWaitResult::IPC_WAIT_FAILED;
	switch (dwResult)
	{
	case WAIT_OBJECT_0:		// data
	{
		ResetEvent(m_hBufferEvent);
		HANDLE waitHandles2[2] = { m_hBufferMutex, m_hInstanceQuitWaitingEvent };
		DWORD dwResult2 = WaitForMultipleObjects(2, waitHandles2, FALSE, timeout);
		switch (dwResult2)
		{
		case WAIT_OBJECT_0: // got event
			memcpy(dst, m_pBuffer, dstSize);
			ReleaseMutex(m_hBufferMutex);
			waitResult = EWaitResult::IPC_WAIT_SUCCESS;
			break;
		case WAIT_OBJECT_0 + 1: // quit waiting
			ResetEvent(m_hInstanceQuitWaitingEvent);
			waitResult = EWaitResult::IPC_WAIT_QUIT;
			break;
		case WAIT_TIMEOUT:
			waitResult = EWaitResult::IPC_WAIT_TIMEOUT;
			break;
		}
		break;
	}
	case WAIT_OBJECT_0 + 1: // quit waiting
		ResetEvent(m_hInstanceQuitWaitingEvent);
		waitResult =  EWaitResult::IPC_WAIT_QUIT;
		break;
	case WAIT_TIMEOUT:
		waitResult = EWaitResult::IPC_WAIT_TIMEOUT;
		break;
	}
	return waitResult;
}

void* IpcHelper::SharedMemoryHelper::rawBuffer()
{
	return m_pBuffer;
}

IpcHelper::SharedMemoryHelper::SharedMemoryHelper()
	: m_initialized(false)
	, m_size(0)
	, m_pBuffer(NULL)
	, m_hBufferEvent(NULL)
	, m_hBufferMutex(NULL)
	, m_hBufferMapFile(NULL)
	, m_hShutdownEvent(NULL)
	, m_hInstanceQuitWaitingEvent(CreateEvent(NULL, TRUE, FALSE, NULL))
{
}

IpcHelper::SharedMemoryHelper::~SharedMemoryHelper()
{
	CleanUp();
}

void IpcHelper::SharedMemoryHelper::CleanUp()
{
	m_initialized = false;
	m_size = 0;
	m_pBuffer = NULL;

	if (m_hBufferMutex)
	{
		CloseHandle(m_hBufferMutex);
		m_hBufferMutex = NULL;
	}
	if (m_hBufferEvent)
	{
		CloseHandle(m_hBufferEvent);
		m_hBufferEvent = NULL;
	}
	if (m_hBufferMapFile)
	{
		CloseHandle(m_hBufferMapFile);
		m_hBufferMapFile = NULL;
	}
	if (m_hShutdownEvent)
	{
		CloseHandle(m_hShutdownEvent);
		m_hShutdownEvent = NULL;
	}
	if (m_hInstanceQuitWaitingEvent)
	{
		CloseHandle(m_hInstanceQuitWaitingEvent);
		m_hInstanceQuitWaitingEvent = NULL;
	}
}

/// simple code that display pressed key data received via shared memory using IpcHelper.h
#include <iostream>
#include <Windows.h>
#include "../IpcHelper.h"

int main()
{
    std::unique_ptr<IpcHelper::SharedMemoryHelper> helper = nullptr;
    if (IpcHelper::SharedMemoryHelper::isExist(L"helper"))
    {
        if (!IpcHelper::SharedMemoryHelper::openSharedMemory(L"helper", sizeof(int), &helper))
        {
            std::cout << "failed to open shared memory" << std::endl;
            return 1;
        }
        else
            std::cout << "ipc opened" << std::endl;
    }
    else
    {
        if (!IpcHelper::SharedMemoryHelper::createSharedMemory(L"helper", sizeof(int), &helper))
        {
            std::cout << "failed to create shared memory" << std::endl;
            return 1;
        }
        else
            std::cout << "ipc created" << std::endl;
    }

    int buf = 0;
    while (true)
    {
        IpcHelper::EWaitResult result = helper->waitReadData(&buf, sizeof(buf));
        if (result == IpcHelper::IPC_WAIT_SUCCESS)
            std::cout << (char)buf << std::endl;
        if (result == IpcHelper::IPC_WAIT_TIMEOUT)
            std::cout << "timeout" << std::endl;
        if (result == IpcHelper::IPC_WAIT_FAILED)
            std::cout << "failed" << std::endl;
    }

    return 0;
}

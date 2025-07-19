/// simple code that hook low level keyboard and send data via shared memory using IpcHelper.h
#include <iostream>
#include <Windows.h>
#include <memory>
#include "../IpcHelper.h"

HHOOK m_hHook;
HANDLE hMutex;
LPVOID pBuf;
std::unique_ptr<IpcHelper::SharedMemoryHelper> helper;

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode >= HC_ACTION)
    {
        KBDLLHOOKSTRUCT* pkbhs = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        // send message
        {
            std::cout << pkbhs->vkCode << std::endl;
            const char* msg = "this is message";
            helper->waitWriteData(&pkbhs->vkCode, sizeof(int));
        }
        if (pkbhs->vkCode == VK_ESCAPE)
        {
            PostQuitMessage(0);
        }
    }

    return CallNextHookEx(m_hHook, nCode, wParam, lParam);
}

int main()
{
    std::wstring ipcName = L"IPCHELPER";
    if (IpcHelper::SharedMemoryHelper::isExist(ipcName))
    {
        if (IpcHelper::SharedMemoryHelper::openSharedMemory(ipcName, sizeof(int), &helper))
            std::cout << "shared memory opened" << std::endl;
        else
        {
            std::cout << "failed to open shared memory" << std::endl;
            return 1;
        }
    }
    else
    {
        if (IpcHelper::SharedMemoryHelper::createSharedMemory(ipcName, sizeof(int), &helper))
            std::cout << "shared memory created" << std::endl;
        else
        {
            std::cout << "failed to create shared memory" << std::endl;
            return 1;
        }
    }

    m_hHook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc, NULL, NULL);
    if (m_hHook == NULL)
    {
        std::cout << "Failed to install hook, Error code : " << GetLastError();
        return 1;
    }

    MSG msg;
    while (GetMessage(&msg, NULL, NULL, NULL))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    UnhookWindowsHookEx(m_hHook);

    std::cout << "exited";
}

#pragma once
#ifndef DLL_H
#define DLL_H
#define WIN32_LEAN_AND_MEAN

#include <napi.h>
#include <windows.h>
#include <TlHelp32.h>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <iostream>
#include <iomanip>

std::vector<BYTE> GetBytes(const std::string &data)
{
  std::vector<BYTE> bytes;
  for (unsigned int i = 0; i < data.length(); i += 2)
  {
    std::string byteString = data.substr(i, 2);
    BYTE byte = (BYTE)strtol(byteString.c_str(), NULL, 16);
    bytes.push_back(byte);
  };

  return bytes;
};

namespace dll
{
  bool inject(DWORD pid, const std::string &data_string)
  {
    HANDLE handle = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (handle == NULL)
    {
      return false;
    };

    std::vector<BYTE> data = GetBytes(data_string);
    LPVOID targetProcessPath = VirtualAllocEx(handle, NULL, data.size(), MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (targetProcessPath == NULL)
    {
      CloseHandle(handle);
      return false;
    };

    if (WriteProcessMemory(handle, targetProcessPath, data.data(), data.size(), NULL) == 0)
    {
      VirtualFreeEx(handle, targetProcessPath, 0, MEM_RELEASE);
      CloseHandle(handle);
      return false;
    };

    HMODULE kernel32 = GetModuleHandle("kernel32.dll");
    if (kernel32 == NULL)
    {
      VirtualFreeEx(handle, targetProcessPath, 0, MEM_RELEASE);
      CloseHandle(handle);
      return false;
    };

    LPTHREAD_START_ROUTINE loadLibraryAddress = (LPTHREAD_START_ROUTINE)GetProcAddress(kernel32, "LoadLibraryA");
    if (loadLibraryAddress == NULL)
    {
      VirtualFreeEx(handle, targetProcessPath, 0, MEM_RELEASE);
      CloseHandle(handle);
      return false;
    };

    HANDLE thread = CreateRemoteThread(handle, NULL, 0, loadLibraryAddress, targetProcessPath, 0, NULL);
    if (thread == NULL)
    {
      VirtualFreeEx(handle, targetProcessPath, 0, MEM_RELEASE);
      CloseHandle(handle);
      return false;
    };

    WaitForSingleObject(thread, INFINITE);
    DWORD moduleHandle;
    GetExitCodeThread(thread, &moduleHandle);

    VirtualFreeEx(handle, targetProcessPath, 0, MEM_RELEASE);
    CloseHandle(thread);
    CloseHandle(handle);

    return moduleHandle > 0;
  };
};

#endif
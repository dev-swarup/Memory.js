#pragma once
#ifndef MEMORY_H
#define MEMORY_H
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <vector>

class memory
{
public:
  memory() {}
  ~memory() {}

  bool read(HANDLE hProcess, DWORD64 address, SIZE_T size, char *dstBuffer)
  {
    return ReadProcessMemory(hProcess, (LPCVOID)address, dstBuffer, size, NULL) != 0;
  }

  bool write(HANDLE hProcess, DWORD64 address, const char *srcBuffer, SIZE_T size)
  {
    return WriteProcessMemory(hProcess, (LPVOID)address, srcBuffer, size, NULL) != 0;
  }
};

#endif
#pragma once
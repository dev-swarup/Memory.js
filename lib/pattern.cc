#include <node.h>
#include <windows.h>
#include <TlHelp32.h>
#include <vector>
#include <string>
#include <sstream>
#include "pattern.h"

pattern::pattern() {}
pattern::~pattern() {}

struct MEMORY_REGION
{
  DWORD_PTR dwBaseAddr;
  DWORD_PTR dwMemorySize;
};

bool pattern::FindPattern(DWORD pid, const std::string &pattern_value, std::vector<DWORD_PTR> &address)
{
  HANDLE handle = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, pid);
  if (handle == NULL)
  {
    return false;
  };

  BYTE *pCurrMemoryData = NULL;
  MEMORY_BASIC_INFORMATION mbi;
  std::vector<MEMORY_REGION> m_vMemoryRegion;
  mbi.RegionSize = 0x1000;

  DWORD_PTR dwAddress = 0x0L;
  std::vector<bool> isWildcard;
  std::vector<BYTE> searchValue;
  ConvertPattern(pattern_value, searchValue, isWildcard);
  DWORD_PTR nSearchSize = searchValue.size();

  while (VirtualQueryEx(handle, (LPCVOID)dwAddress, &mbi, sizeof(mbi)) && (dwAddress < 0x00007fffffffffff) && ((dwAddress + mbi.RegionSize) > dwAddress))
  {
    if ((mbi.State == MEM_COMMIT) && ((mbi.Protect & PAGE_GUARD) == 0) && (mbi.Protect != PAGE_NOACCESS) && ((mbi.AllocationProtect & PAGE_NOCACHE) != PAGE_NOCACHE))
    {
      MEMORY_REGION mData = {0};
      mData.dwBaseAddr = (DWORD_PTR)mbi.BaseAddress;
      mData.dwMemorySize = mbi.RegionSize;
      m_vMemoryRegion.push_back(mData);
    };

    dwAddress = (DWORD_PTR)mbi.BaseAddress + mbi.RegionSize;
  };

  for (auto &mData : m_vMemoryRegion)
  {
    DWORD_PTR dwNumberOfBytesRead = 0;
    pCurrMemoryData = new BYTE[mData.dwMemorySize];
    ZeroMemory(pCurrMemoryData, mData.dwMemorySize);
    if (!ZwReadVirtualMemory(handle, (LPVOID)mData.dwBaseAddr, pCurrMemoryData, mData.dwMemorySize, &dwNumberOfBytesRead) || dwNumberOfBytesRead <= 0)
    {
      delete[] pCurrMemoryData;
      continue;
    };

    DWORD_PTR dwOffset = 0;
    int iOffset = MemFind(pCurrMemoryData, dwNumberOfBytesRead, searchValue, isWildcard);
    while (iOffset != -1)
    {
      dwOffset += iOffset;
      address.push_back(dwOffset + mData.dwBaseAddr);
      dwOffset += nSearchSize;
      iOffset = MemFind(pCurrMemoryData + dwOffset, dwNumberOfBytesRead - dwOffset - nSearchSize, searchValue, isWildcard);
    };

    delete[] pCurrMemoryData;
    pCurrMemoryData = NULL;
  };

  CloseHandle(handle);
  return true;
};

int MemFind(BYTE *buffer, DWORD_PTR dwBufferSize, const std::vector<BYTE> &searchValue, const std::vector<bool> &isWildcard)
{
  if (dwBufferSize < 0)
  {
    return -1;
  };

  DWORD_PTR searchSize = searchValue.size();
  for (DWORD_PTR i = 0; i < dwBufferSize; i++)
  {
    DWORD_PTR j;
    for (j = 0; j < searchSize; j++)
    {
      if (!isWildcard[j] && buffer[i + j] != searchValue[j])
        break;
    };

    if (j == searchSize)
      return i;
  };

  return -1;
};

void ConvertPattern(const std::string &pattern, std::vector<BYTE> &searchValue, std::vector<bool> &isWildcard)
{
  std::string byteStr;
  std::istringstream iss(pattern);
  while (iss >> byteStr)
  {
    if (byteStr == "?" || byteStr == "??")
    {
      searchValue.push_back(0);
      isWildcard.push_back(true);
    }
    else
    {
      searchValue.push_back(static_cast<BYTE>(std::stoul(byteStr, nullptr, 16)));
      isWildcard.push_back(false);
    }
  };
};
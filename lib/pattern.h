#pragma once
#ifndef PATTERN_H
#define PATTERN_H
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <TlHelp32.h>

class pattern
{
public:
  pattern();
  ~pattern();

  bool FindPattern(DWORD pid, const std::string &pattern_value, std::vector<DWORD_PTR> &address);
};

#endif
#pragma once
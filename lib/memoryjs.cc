#include <windows.h>
#include <psapi.h>
#include <napi.h>
#include <string>
#include <thread>
#include "dll.h"
#include "pattern.h"
#include "memoryjs.h"

#pragma comment(lib, "psapi.lib")

memory Memory;
pattern Pattern;

struct Vector3
{
  float x, y, z;
};

struct Vector4
{
  float w, x, y, z;
};

Napi::Value listProcess(const Napi::CallbackInfo &args)
{
  Napi::Env env = args.Env();

  HANDLE hProcessSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
  PROCESSENTRY32 pEntry;

  if (hProcessSnapshot == INVALID_HANDLE_VALUE)
  {
    return Napi::Boolean::New(env, false);
  };

  pEntry.dwSize = sizeof(pEntry);

  if (!Process32First(hProcessSnapshot, &pEntry))
  {
    CloseHandle(hProcessSnapshot);
    return Napi::Boolean::New(env, false);
  };

  std::vector<PROCESSENTRY32> processEntries;

  do
  {
    processEntries.push_back(pEntry);
  } while (Process32Next(hProcessSnapshot, &pEntry));

  CloseHandle(hProcessSnapshot);
  Napi::Array processes = Napi::Array::New(env, processEntries.size());

  for (std::vector<PROCESSENTRY32>::size_type i = 0; i != processEntries.size(); i++)
  {
    Napi::Object process = Napi::Object::New(env);

    process.Set(Napi::String::New(env, "cntThreads"), Napi::Value::From(env, (int)processEntries[i].cntThreads));
    process.Set(Napi::String::New(env, "szExeFile"), Napi::String::New(env, processEntries[i].szExeFile));
    process.Set(Napi::String::New(env, "th32ProcessID"), Napi::Value::From(env, (int)processEntries[i].th32ProcessID));
    process.Set(Napi::String::New(env, "th32ParentProcessID"), Napi::Value::From(env, (int)processEntries[i].th32ParentProcessID));
    process.Set(Napi::String::New(env, "pcPriClassBase"), Napi::Value::From(env, (int)processEntries[i].pcPriClassBase));

    processes.Set(i, process);
  };

  return processes;
};

Napi::Value openHandle(const Napi::CallbackInfo &args)
{
  Napi::Env env = args.Env();

  if (args.Length() != 1 || !args[0].IsNumber())
  {
    return Napi::Boolean::New(env, false);
  };

  DWORD pid = args[0].As<Napi::Number>().Uint32Value();
  HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);

  if (hProcess == NULL)
  {
    return Napi::Boolean::New(env, false);
  };

  return Napi::External<HANDLE>::New(env, hProcess);
};

Napi::Value closeHandle(const Napi::CallbackInfo &args)
{
  Napi::Env env = args.Env();

  if (args.Length() != 1 || !args[0].IsExternal())
  {
    return Napi::Boolean::New(env, false);
  };

  HANDLE hProcess = args[0].As<Napi::External<HANDLE>>().Data();

  if (CloseHandle(hProcess))
  {
    return Napi::Boolean::New(env, true);
  }
  else
  {
    return Napi::Boolean::New(env, false);
  }
};

Napi::Value readMemory(const Napi::CallbackInfo &args)
{
  Napi::Env env = args.Env();

  if (args.Length() != 3 || !args[0].IsNumber() || !args[1].IsNumber() || !args[2].IsNumber())
  {
    return Napi::Boolean::New(env, false);
  };

  HANDLE handle = (HANDLE)args[0].As<Napi::Number>().Int64Value();
  DWORD64 address = args[1].As<Napi::Number>().Int64Value();
  SIZE_T size = args[2].As<Napi::Number>().Int64Value();

  char *data = (char *)malloc(sizeof(char) * size);
  if (data == NULL)
  {
    return Napi::Boolean::New(env, false);
  };

  if (!Memory.read(handle, address, size, data))
  {
    free(data);
    return Napi::Boolean::New(env, false);
  };

  Napi::Buffer<char> value = Napi::Buffer<char>::Copy(env, data, size);
  free(data);

  return value;
};

Napi::Value writeMemory(const Napi::CallbackInfo &args)
{
  Napi::Env env = args.Env();

  if (args.Length() != 3 || !args[0].IsNumber() || !args[1].IsNumber() || !args[2].IsBuffer())
  {
    return Napi::Boolean::New(env, false);
  };

  HANDLE handle = (HANDLE)args[0].As<Napi::Number>().Int64Value();
  DWORD64 address = args[1].As<Napi::Number>().Int64Value();
  SIZE_T length = args[2].As<Napi::Buffer<char>>().Length();
  char *data = args[2].As<Napi::Buffer<char>>().Data();

  if (!Memory.write<char *>(handle, address, data, length))
  {
    return Napi::Boolean::New(env, false);
  };

  return Napi::Boolean::New(env, true);
};

Napi::Value findPattern(const Napi::CallbackInfo &args)
{
  Napi::Env env = args.Env();

  if (args.Length() != 3 || !args[0].IsNumber() || !args[1].IsString() || !args[2].IsFunction())
  {
    return Napi::Boolean::New(env, false);
  };

  DWORD pid = args[0].As<Napi::Number>().Uint32Value();
  std::string pattern(args[1].As<Napi::String>().Utf8Value());
  Napi::Function callback = args[2].As<Napi::Function>();

  std::thread(pid, pattern, callback, env {
    std::vector<DWORD_PTR> addresses;
    bool success = FindPattern(pid, pattern, addresses);

    Napi::HandleScope scope(env);
    if (!success || addresses.empty())
    {
      callback.Call(env.Global(), {Napi::Boolean::New(env, false)});
    }
    else
    {
      Napi::Array result = Napi::Array::New(env, addresses.size());
      for (size_t i = 0; i < addresses.size(); ++i)
      {
        result.Set(i, Napi::Number::New(env, addresses[i]));
      }
      callback.Call(env.Global(), {result});
    } }).detach();

  return env.Undefined();
};

Napi::Value inject(const Napi::CallbackInfo &args)
{
  Napi::Env env = args.Env();

  if (args.Length() != 2 || !args[0].IsNumber() || !args[1].IsString())
  {
    return Napi::Boolean::New(env, false);
  };

  DWORD pid = args[0].As<Napi::Number>().Uint32Value();
  std::string data(args[1].As<Napi::String>().Utf8Value());

  bool success = dll::inject(pid, data);

  return Napi::Boolean::New(env, success);
};

Napi::Object init(Napi::Env env, Napi::Object exports)
{
  exports.Set(Napi::String::New(env, "ListProcess"), Napi::Function::New(env, listProcess));
  exports.Set(Napi::String::New(env, "OpenHandle"), Napi::Function::New(env, openHandle));
  exports.Set(Napi::String::New(env, "CloseHandle"), Napi::Function::New(env, closeHandle));
  exports.Set(Napi::String::New(env, "Read"), Napi::Function::New(env, readMemory));
  exports.Set(Napi::String::New(env, "Write"), Napi::Function::New(env, writeMemory));
  exports.Set(Napi::String::New(env, "FindPattern"), Napi::Function::New(env, findPattern));
  exports.Set(Napi::String::New(env, "InjectCode"), Napi::Function::New(env, inject));
  return exports;
};

NODE_API_MODULE(memoryjs, init);
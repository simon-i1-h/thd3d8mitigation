#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdarg.h>

#define THF_LOG_PREFIX "[thd3d8fix]"

int myvasprintf(char**, const char*, va_list);
int myasprintf(char**, const char*, ...);
int AllocateErrorMessageA(DWORD, char**);
void ThfVLog(const char*, va_list);
void ThfLog(const char*, ...);
void ThfVError(DWORD, const char*, va_list);
void ThfError(DWORD, const char*, ...);

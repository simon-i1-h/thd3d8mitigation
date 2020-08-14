#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

// util.c

#define THF_LOG_PREFIX "[thd3d8fix]"

int myvasprintf(char**, const char*, va_list);
int myasprintf(char**, const char*, ...);
int AllocateErrorMessageA(DWORD, char**);
void ThfVLog(const char*, va_list);
void ThfLog(const char*, ...);
void ThfVError(DWORD, const char*, va_list);
void ThfError(DWORD, const char*, ...);

// hashtable.cpp

struct hashtable;

struct hashtable* hashtable_new(void);
void hashtable_del(struct hashtable* h);
void hashtable_insert(struct hashtable* h, uintptr_t k, void* v);
void hashtable_erase(struct hashtable* h, uintptr_t k);
void* hashtable_get(struct hashtable* h, uintptr_t k);

#ifdef __cplusplus
}
#endif

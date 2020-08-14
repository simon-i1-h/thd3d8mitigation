#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define CINTERFACE
#include <d3d8.h>

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

// thd3d8fix.c

typedef IDirect3D8* (WINAPI* Type_Direct3DCreate8)(UINT);

typedef HRESULT(__stdcall* Type_IDirect3DDevice8_Present)(
	IDirect3DDevice8*,
	CONST RECT*,
	CONST RECT*,
	HWND,
	CONST RGNDATA*
);

typedef HRESULT(__stdcall* Type_IDirect3D8_CreateDevice)(
	IDirect3D8*,
	UINT,
	D3DDEVTYPE,
	HWND,
	DWORD,
	D3DPRESENT_PARAMETERS*,
	IDirect3DDevice8**
);

struct IDirect3D8ExtraData {
	Type_IDirect3D8_CreateDevice VanillaCreateDevice;
};

struct IDirect3DDevice8ExtraData {
	Type_IDirect3DDevice8_Present VanillaPresent;
	D3DPRESENT_PARAMETERS pp;
};

// util.c

#define THF_LOG_PREFIX "[thd3d8fix]"

int myvasprintf(char**, const char*, va_list);
int myasprintf(char**, const char*, ...);
int AllocateErrorMessageA(DWORD, char**);
void ThfVLog(const char*, va_list);
void ThfLog(const char*, ...);
void ThfVError(DWORD, const char*, va_list);
void ThfError(DWORD, const char*, ...);

// extradatatable.cpp

struct IDirect3D8ExtraDataTable;
struct IDirect3DDevice8ExtraDataTable;

struct IDirect3D8ExtraData* AllocateIDirect3D8ExtraData(Type_IDirect3D8_CreateDevice);
struct IDirect3DDevice8ExtraData* AllocateIDirect3DDevice8ExtraData(Type_IDirect3DDevice8_Present, D3DPRESENT_PARAMETERS);

struct IDirect3D8ExtraDataTable* IDirect3D8ExtraDataTableNew(void);
void IDirect3D8ExtraDataTableInsert(struct IDirect3D8ExtraDataTable *, IDirect3D8*, struct IDirect3D8ExtraData*);
void IDirect3D8ExtraDataTableErase(struct IDirect3D8ExtraDataTable *, IDirect3D8*);
struct IDirect3D8ExtraData* IDirect3D8ExtraDataTableGet(struct IDirect3D8ExtraDataTable *, IDirect3D8*);

struct IDirect3DDevice8ExtraDataTable* IDirect3DDevice8ExtraDataTableNew(void);
void IDirect3DDevice8ExtraDataTableInsert(struct IDirect3DDevice8ExtraDataTable*, IDirect3DDevice8*, struct IDirect3DDevice8ExtraData*);
void IDirect3DDevice8ExtraDataTableErase(struct IDirect3DDevice8ExtraDataTable*, IDirect3DDevice8*);
struct IDirect3DDevice8ExtraData* IDirect3DDevice8ExtraDataTableGet(struct IDirect3DDevice8ExtraDataTable*, IDirect3DDevice8*);

#ifdef __cplusplus
}
#endif

#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define CINTERFACE
#include <d3d8.h>

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

#define THF_VERSION "0.1"

// thd3d8mitigation.c

// type of Direct3DCreate8
typedef IDirect3D8* (WINAPI* Direct3DCreate8_t)(UINT);

// type of IDirect3D8::CreateDevice
typedef HRESULT(__stdcall* IDirect3D8CreateDevice_t)(
	IDirect3D8*,
	UINT,
	D3DDEVTYPE,
	HWND,
	DWORD,
	D3DPRESENT_PARAMETERS*,
	IDirect3DDevice8**
);

// type of IDirect3D8::Release (inherit from IUnknown::Release)
typedef ULONG(__stdcall* IDirect3D8Release_t)(IDirect3D8*);

// type of IDirect3DDevice8::Present
typedef HRESULT(__stdcall* IDirect3DDevice8Present_t)(
	IDirect3DDevice8*,
	CONST RECT*,
	CONST RECT*,
	HWND,
	CONST RGNDATA*
);

// type of IDirect3DDevice8::Release (inherit from IUnknown::Release)
typedef ULONG(__stdcall* IDirect3DDevice8Release_t)(IDirect3DDevice8*);

// type of IDirect3DDevice8::Reset
typedef HRESULT(__stdcall* IDirect3DDevice8Reset_t)(IDirect3DDevice8*, D3DPRESENT_PARAMETERS*);

struct IDirect3D8ExtraData {
	IDirect3D8CreateDevice_t VanillaCreateDevice;
	IDirect3D8Release_t VanillaRelease;
};

struct IDirect3DDevice8ExtraData {
	IDirect3DDevice8Present_t VanillaPresent;
	IDirect3DDevice8Release_t VanillaRelease;
	IDirect3DDevice8Reset_t VanillaReset;
	D3DPRESENT_PARAMETERS pp;
};

// util.c

#define THF_LOG_PREFIX "[thd3d8mitigation]"

int myvasprintf(char**, const char*, va_list);
int myasprintf(char**, const char*, ...);
int AllocateErrorMessageA(DWORD, char**);
void ThfVLog(const char*, va_list);
void ThfLog(const char*, ...);
void ThfVError(DWORD, const char*, va_list);
void ThfError(DWORD, const char*, ...);
__declspec(noreturn) void ThfFatal(int, const char*, ...);

// extradatatable.cpp

struct IDirect3D8ExtraDataTable;
struct IDirect3DDevice8ExtraDataTable;

struct IDirect3D8ExtraDataTable* IDirect3D8ExtraDataTableNew(void);
int IDirect3D8ExtraDataTableInsert(struct IDirect3D8ExtraDataTable *, IDirect3D8*, /* malloc-ed */ struct IDirect3D8ExtraData*);
void IDirect3D8ExtraDataTableErase(struct IDirect3D8ExtraDataTable *, IDirect3D8*);
/* malloc-ed */ struct IDirect3D8ExtraData* IDirect3D8ExtraDataTableGet(struct IDirect3D8ExtraDataTable *, IDirect3D8*);
void IDirect3D8ExtraDataTableShrinkToFit(struct IDirect3D8ExtraDataTable*);

struct IDirect3DDevice8ExtraDataTable* IDirect3DDevice8ExtraDataTableNew(void);
int IDirect3DDevice8ExtraDataTableInsert(struct IDirect3DDevice8ExtraDataTable*, IDirect3DDevice8*, /* malloc-ed */ struct IDirect3DDevice8ExtraData*);
void IDirect3DDevice8ExtraDataTableErase(struct IDirect3DDevice8ExtraDataTable*, IDirect3DDevice8*);
/* malloc-ed */ struct IDirect3DDevice8ExtraData* IDirect3DDevice8ExtraDataTableGet(struct IDirect3DDevice8ExtraDataTable*, IDirect3DDevice8*);
void IDirect3DDevice8ExtraDataTableShrinkToFit(struct IDirect3DDevice8ExtraDataTable*);

#ifdef __cplusplus
}
#endif

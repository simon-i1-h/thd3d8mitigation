#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define CINTERFACE
#include <d3d8.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PROGRAM_VERSION "1.3dev"

/* thd3d8mitigation.c */

/* type of Direct3DCreate8 */
typedef IDirect3D8*(WINAPI* Direct3DCreate8_t)(UINT);

/* type of IDirect3D8::CreateDevice */
typedef HRESULT(__stdcall* IDirect3D8CreateDevice_t)(IDirect3D8*, UINT, D3DDEVTYPE, HWND, DWORD,
	D3DPRESENT_PARAMETERS*, IDirect3DDevice8**);

/* type of IDirect3D8::Release (inherit from IUnknown::Release) */
typedef ULONG(__stdcall* IDirect3D8Release_t)(IDirect3D8*);

/* type of IDirect3DDevice8::Present */
typedef HRESULT(__stdcall* IDirect3DDevice8Present_t)(IDirect3DDevice8*, CONST RECT*, CONST RECT*, HWND,
	CONST RGNDATA*);

/* type of IDirect3DDevice8::Release (inherit from IUnknown::Release) */
typedef ULONG(__stdcall* IDirect3DDevice8Release_t)(IDirect3DDevice8*);

/* type of IDirect3DDevice8::Reset */
typedef HRESULT(__stdcall* IDirect3DDevice8Reset_t)(IDirect3DDevice8*, D3DPRESENT_PARAMETERS*);

typedef HRESULT (*ModIDirect3DDevice8Present_t)(IDirect3DDevice8*, struct IDirect3DDevice8ExtraData*,
	CONST RECT*, CONST RECT*, HWND, CONST RGNDATA*);

enum ConfigWaitFor {
	CONFIG_WAITFOR_VSYNC,
	CONFIG_WAITFOR_NORMAL,
	CONFIG_WAITFOR_AUTO
};

struct IDirect3D8ExtraData {
	IDirect3D8CreateDevice_t VanillaCreateDevice;
	IDirect3D8Release_t VanillaRelease;
};

struct IDirect3DDevice8ExtraData {
	IDirect3DDevice8Present_t VanillaPresent;
	IDirect3DDevice8Release_t VanillaRelease;
	IDirect3DDevice8Reset_t VanillaReset;
	ModIDirect3DDevice8Present_t ModPresent;
	D3DPRESENT_PARAMETERS pp;
};

extern CRITICAL_SECTION g_CS;
extern const char* const ConfigWaitForNameTable[3];

/* util.c */

#define LOG_PREFIX "[thd3d8mitigation]"

/* nullable */
extern HANDLE g_LogFile;

int myasprintf(char**, const char*, ...);
void Log(const char*, ...);
void LogWithErrorCode(DWORD, const char*, ...);
__declspec(noreturn) void Fatal(int, const char*, ...);

/* extradatatable.cpp */

struct IDirect3D8ExtraDataTable;
struct IDirect3DDevice8ExtraDataTable;

struct IDirect3D8ExtraDataTable* IDirect3D8ExtraDataTableNew(void);
BOOL IDirect3D8ExtraDataTableInsert(struct IDirect3D8ExtraDataTable*, IDirect3D8*, struct IDirect3D8ExtraData);
void IDirect3D8ExtraDataTableErase(struct IDirect3D8ExtraDataTable*, IDirect3D8*);
struct IDirect3D8ExtraData* IDirect3D8ExtraDataTableGet(struct IDirect3D8ExtraDataTable*, IDirect3D8*);
void IDirect3D8ExtraDataTableShrinkToFit(struct IDirect3D8ExtraDataTable*);

struct IDirect3DDevice8ExtraDataTable* IDirect3DDevice8ExtraDataTableNew(void);
BOOL IDirect3DDevice8ExtraDataTableInsert(struct IDirect3DDevice8ExtraDataTable*, IDirect3DDevice8*,
	struct IDirect3DDevice8ExtraData);
void IDirect3DDevice8ExtraDataTableErase(struct IDirect3DDevice8ExtraDataTable*, IDirect3DDevice8*);
struct IDirect3DDevice8ExtraData* IDirect3DDevice8ExtraDataTableGet(struct IDirect3DDevice8ExtraDataTable*,
	IDirect3DDevice8*);
void IDirect3DDevice8ExtraDataTableShrinkToFit(struct IDirect3DDevice8ExtraDataTable*);

#ifdef __cplusplus
}
#endif

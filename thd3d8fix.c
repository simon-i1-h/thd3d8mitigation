#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define CINTERFACE
#include <d3d8.h>

#include <tchar.h>
#include <mmsystem.h>

#include <stdlib.h>

#include "thd3d8fix.h"

// Abbreviation:
//   Thf: thd3d8fix

// XXX TODO error handling
// XXX TODO 東方と同様にファイルにもロギング
// XXX TODO review
// XXX TODO fix data race and race condition
// XXX TODO コード整形

static CRITICAL_SECTION g_cs;

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
	D3DPRESENT_PARAMETERS pp;  // XXX TODO 潜在的な危険性
};

//static Type_IDirect3DDevice8_Present Vanilla_IDirect3DDevice8_Present = NULL;
//static Type_IDirect3D8_CreateDevice Vanilla_IDirect3D8_CreateDevice = NULL;
struct hashtable* g_IDirect3D8ExtraDataTable; /* key: uintptr_t as IDirect3D8*, value: malloc-ed struct IDirect3D8ExtraData */
struct hashtable* g_IDirect3DDevice8ExtraDataTable; /* key: uintptr_t as IDirect3DDevice8*, value: malloc-ed struct IDirect3DDevice8ExtraData */

struct IDirect3D8ExtraData* AllocateIDirect3D8ExtraData(Type_IDirect3D8_CreateDevice VanillaCreateDevice)
{
	struct IDirect3D8ExtraData* ret;

	if ((ret = malloc(sizeof(*ret))) == NULL) // XXX TODO error handling
		return NULL;
	*ret = (struct IDirect3D8ExtraData){ .VanillaCreateDevice = VanillaCreateDevice };
	return ret;
}

struct IDirect3DDevice8ExtraData* AllocateIDirect3DDevice8ExtraData(Type_IDirect3DDevice8_Present VanillaPresent, D3DPRESENT_PARAMETERS pp)
{
	struct IDirect3DDevice8ExtraData* ret;

	if ((ret = malloc(sizeof(*ret))) == NULL) // XXX TODO error handling
		return NULL;
	*ret = (struct IDirect3DDevice8ExtraData){ .VanillaPresent = VanillaPresent, .pp = pp };
	return ret;
}

HRESULT __stdcall Mod_IDirect3DDevice8_Present(
	IDirect3DDevice8* me,
	CONST RECT* pSourceRect,
	CONST RECT* pDestRect,
	HWND hDestWindowOverride,
	CONST RGNDATA* pDirtyRegion
)
{
	D3DRASTER_STATUS stat;
	HRESULT ret;

	struct IDirect3DDevice8ExtraData* me_exdata;

	EnterCriticalSection(&g_cs);
	me_exdata = hashtable_get(g_IDirect3DDevice8ExtraDataTable, (uintptr_t)me);
	LeaveCriticalSection(&g_cs);

	if (me_exdata == NULL)
		return E_FAIL; // XXX TODO logging

	if (me_exdata->pp.Windowed)
		return me_exdata->VanillaPresent(me, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);

	do {
		if (FAILED(me->lpVtbl->GetRasterStatus(me, &stat)))
		{
			ThfLog("%s: IDirect3DDevice8::GetRasterStatus failed.", __FUNCTION__);
			return E_FAIL;
		}
		Sleep(0); // XXX TODO SleepEx?
	} while (stat.InVBlank);

	ret = me_exdata->VanillaPresent(me, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);

	do {
		if (FAILED(me->lpVtbl->GetRasterStatus(me, &stat)))
		{
			ThfLog("%s: IDirect3DDevice8::GetRasterStatus failed.", __FUNCTION__);
			return E_FAIL;
		}
		Sleep(0); // XXX TODO SleepEx?
	} while (!stat.InVBlank);

	return ret;
}

HRESULT g_Mod_IDirect3D8_CreateDevice(IDirect3D8* me, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice8** ppReturnedDeviceInterface)
{
	HRESULT ret;

	struct IDirect3D8ExtraData* me_exdata;
	if ((me_exdata = hashtable_get(g_IDirect3D8ExtraDataTable, (uintptr_t)me)) == NULL)
		return E_FAIL;

	ret = me_exdata->VanillaCreateDevice(me, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, ppReturnedDeviceInterface);

	if (SUCCEEDED(ret))
	{
		DWORD orig_protect;
		IDirect3DDevice8* device = *ppReturnedDeviceInterface;
		IDirect3DDevice8Vtbl* vtbl = device->lpVtbl;

		// IDirect3DDevice8::Present

		if (!VirtualProtect(&vtbl->Present, sizeof(vtbl->Present), PAGE_READWRITE, &orig_protect))
		{
			ThfError(GetLastError(), "%s: VirtualProtect failed.", __FUNCTION__);
			return E_FAIL;
		}

		hashtable_insert(g_IDirect3DDevice8ExtraDataTable, (uintptr_t)*ppReturnedDeviceInterface, AllocateIDirect3DDevice8ExtraData(vtbl->Present, *pPresentationParameters));
		vtbl->Present = Mod_IDirect3DDevice8_Present;

		// best effort
		if (!VirtualProtect(&vtbl->Present, sizeof(vtbl->Present), orig_protect, &orig_protect))
		{
			ThfError(GetLastError(), "%s: VirtualProtect failed.", __FUNCTION__);
			return E_FAIL;
		}
	}

	return ret;
}

HRESULT __stdcall Mod_IDirect3D8_CreateDevice(IDirect3D8* me, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice8** ppReturnedDeviceInterface)
{
	HRESULT ret;

	EnterCriticalSection(&g_cs);
	ret = g_Mod_IDirect3D8_CreateDevice(me, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, ppReturnedDeviceInterface);
	LeaveCriticalSection(&g_cs);

	return ret;
}

void g_d3d8handle_lazy_init(HMODULE* ret)
{
	char sysdirpath[MAX_PATH + 1];
	char* sysdllpath;
	unsigned int len;
	DWORD err;

	if ((len = GetSystemDirectoryA(sysdirpath, sizeof(sysdirpath))) == 0)
	{
		ThfError(GetLastError(), "%s: GetSystemDirectoryA failed.", __FUNCTION__);
		return;
	}

	if (len > sizeof(sysdirpath))  // XXX TODO review condition
	{
		ThfLog("%s: GetSystemDirectoryA failed.: Path too long.", __FUNCTION__); // XXX TODO error message
		return;
	}

	if (myasprintf(&sysdllpath, "%s\\d3d8.dll", sysdirpath) < 0)
	{
		ThfLog("%s: myasprintf failed.", __FUNCTION__);
		return;
	}

	*ret = LoadLibraryA(sysdllpath);
	err = GetLastError();
	free(sysdllpath);
	if (*ret == NULL)
	{
		ThfError(err, "%s: LoadLibraryA failed.", __FUNCTION__);
	}
}

HMODULE* g_d3d8handle(void)
{
	static HMODULE inner = NULL;

	if (inner == NULL)
		g_d3d8handle_lazy_init(&inner);
	return &inner;
}

void g_Vanilla_Direct3DCreate8_lazy_init(Type_Direct3DCreate8* ret)
{
	if (*g_d3d8handle() == NULL)
		return;

	if ((*ret = (Type_Direct3DCreate8)GetProcAddress(*g_d3d8handle(), "Direct3DCreate8")) == NULL)
	{
		ThfError(GetLastError(), "%s: LoadFuncFromD3D8 failed.", __FUNCTION__);
	}
}

Type_Direct3DCreate8* g_Vanilla_Direct3DCreate8(void)
{
	static Type_Direct3DCreate8 inner = NULL;

	if (inner == NULL)
		g_Vanilla_Direct3DCreate8_lazy_init(&inner);
	return &inner;
}

IDirect3D8* g_Mod_Direct3DCreate8(UINT SDKVersion)
{
	IDirect3D8* ret;

	if (*g_Vanilla_Direct3DCreate8() == NULL)
		return NULL;

	ret = (*g_Vanilla_Direct3DCreate8())(SDKVersion);

	if (ret != NULL)
	{
		DWORD orig_protect;
		IDirect3D8Vtbl* vtbl = ret->lpVtbl;

		if (!VirtualProtect(&vtbl->CreateDevice, sizeof(vtbl->CreateDevice), PAGE_READWRITE, &orig_protect))
		{
			ThfError(GetLastError(), "%s: VirtualProtect failed.", __FUNCTION__);
			return NULL;
		}

		hashtable_insert(g_IDirect3D8ExtraDataTable, (uintptr_t)ret, AllocateIDirect3D8ExtraData(vtbl->CreateDevice));
		vtbl->CreateDevice = Mod_IDirect3D8_CreateDevice;

		// best effort
		if (!VirtualProtect(&vtbl->CreateDevice, sizeof(vtbl->CreateDevice), orig_protect, &orig_protect))
		{
			ThfError(GetLastError(), "%s: VirtualProtect failed.", __FUNCTION__);
			return NULL;
		}
	}

	return ret;
}

IDirect3D8* WINAPI Mod_Direct3DCreate8(UINT SDKVersion)
{
	IDirect3D8* ret;

	EnterCriticalSection(&g_cs);
	ret = g_Mod_Direct3DCreate8(SDKVersion);
	LeaveCriticalSection(&g_cs);

	return ret;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	BOOL ret = TRUE;

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		ThfLog("Attaching to the process: begin");
		InitializeCriticalSection(&g_cs);
		g_IDirect3D8ExtraDataTable = hashtable_new(); // XXX TODO lazy init?
		g_IDirect3DDevice8ExtraDataTable = hashtable_new(); // XXX TODO lazy init?
		ThfLog("Attaching to the process: succeeded");
		break;
	}
	case DLL_PROCESS_DETACH:
	{
		ThfLog("Detaching to the process: begin");
		//hashtable_del(g_IDirect3D8ExtraData_table); XXX TODO
		//hashtable_del(g_IDirect3DDevice8ExtraData_table); XXX TODO
		DeleteCriticalSection(&g_cs);
		ThfLog("Detaching to the process: succeeded");
		break;
	}
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		break;
	}

	return ret;
}

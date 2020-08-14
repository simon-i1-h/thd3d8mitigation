#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define CINTERFACE
#include <d3d8.h>

#include <tchar.h>
#include <mmsystem.h>

#include <stdlib.h>

#include "thd3d8fix.h"

// Naming convention:
//   Thf*: thd3d8fix
//   cs_*: Critical section
//   g_*: Global variable
//   *_t: Type identifier

// XXX TODO error handling
// XXX TODO 東方と同様にファイルにもロギング
// XXX TODO review
// XXX TODO コード整形
// XXX TODO 東方と直接やりとりするわけではないのでUTF-8にできるのでする

static CRITICAL_SECTION g_CS;

struct IDirect3D8ExtraDataTable** cs_IDirect3D8ExtraDataTable(void)
{
	static struct IDirect3D8ExtraDataTable* inner = NULL;

	if (inner == NULL)
		inner = IDirect3D8ExtraDataTableNew();
	return &inner;
}

struct IDirect3DDevice8ExtraDataTable** cs_IDirect3DDevice8ExtraDataTable(void)
{
	static struct IDirect3DDevice8ExtraDataTable* inner = NULL;

	if (inner == NULL)
		inner = IDirect3DDevice8ExtraDataTableNew();
	return &inner;
}

struct IDirect3D8ExtraData* AllocateIDirect3D8ExtraData(IDirect3D8CreateDevice_t VanillaCreateDevice)
{
	struct IDirect3D8ExtraData* ret;

	if ((ret = malloc(sizeof(*ret))) == NULL) // XXX TODO error handling
		return NULL;
	*ret = (struct IDirect3D8ExtraData){ .VanillaCreateDevice = VanillaCreateDevice };
	return ret;
}

struct IDirect3DDevice8ExtraData* AllocateIDirect3DDevice8ExtraData(IDirect3DDevice8Present_t VanillaPresent, D3DPRESENT_PARAMETERS pp)
{
	struct IDirect3DDevice8ExtraData* ret;

	if ((ret = malloc(sizeof(*ret))) == NULL) // XXX TODO error handling
		return NULL;
	*ret = (struct IDirect3DDevice8ExtraData){ .VanillaPresent = VanillaPresent, .pp = pp };
	return ret;
}

HRESULT __stdcall ModIDirect3DDevice8Present(IDirect3DDevice8* me, CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion)
{
	D3DRASTER_STATUS stat;
	HRESULT ret;

	struct IDirect3DDevice8ExtraData* me_exdata;

	EnterCriticalSection(&g_CS);
	me_exdata = IDirect3DDevice8ExtraDataTableGet(*cs_IDirect3DDevice8ExtraDataTable(), me);
	LeaveCriticalSection(&g_CS);

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

HRESULT cs_ModIDirect3D8CreateDevice(IDirect3D8* me, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice8** ppReturnedDeviceInterface)
{
	HRESULT ret;

	struct IDirect3D8ExtraData* me_exdata;
	if ((me_exdata = IDirect3D8ExtraDataTableGet(*cs_IDirect3D8ExtraDataTable(), me)) == NULL)
		return E_FAIL;

	ret = me_exdata->VanillaCreateDevice(me, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, ppReturnedDeviceInterface);

	if (SUCCEEDED(ret))
	{
		DWORD orig_protect;
		IDirect3DDevice8* device = *ppReturnedDeviceInterface;
		IDirect3DDevice8Vtbl* vtbl = device->lpVtbl;

		if (!VirtualProtect(&vtbl->Present, sizeof(vtbl->Present), PAGE_READWRITE, &orig_protect))
		{
			ThfError(GetLastError(), "%s: VirtualProtect failed.", __FUNCTION__);
			return E_FAIL;
		}

		IDirect3DDevice8ExtraDataTableInsert(*cs_IDirect3DDevice8ExtraDataTable(), *ppReturnedDeviceInterface, AllocateIDirect3DDevice8ExtraData(vtbl->Present, *pPresentationParameters));
		vtbl->Present = ModIDirect3DDevice8Present;

		// best effort
		if (!VirtualProtect(&vtbl->Present, sizeof(vtbl->Present), orig_protect, &orig_protect))
		{
			ThfError(GetLastError(), "%s: VirtualProtect failed.", __FUNCTION__);
			return E_FAIL;
		}
	}

	return ret;
}

HRESULT __stdcall ModIDirect3D8CreateDevice(IDirect3D8* me, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice8** ppReturnedDeviceInterface)
{
	HRESULT ret;

	EnterCriticalSection(&g_CS);
	ret = cs_ModIDirect3D8CreateDevice(me, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, ppReturnedDeviceInterface);
	LeaveCriticalSection(&g_CS);

	return ret;
}

void cs_D3D8HandleLazyInit(HMODULE* ret)
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
		ThfError(err, "%s: LoadLibraryA failed.", __FUNCTION__);
}

HMODULE* cs_D3D8Handle(void)
{
	static HMODULE inner = NULL;

	if (inner == NULL)
		cs_D3D8HandleLazyInit(&inner);
	return &inner;
}

void cs_VanillaDirect3DCreate8LazyInit(Direct3DCreate8_t* ret)
{
	if (*cs_D3D8Handle() == NULL)
		return;

	if ((*ret = (Direct3DCreate8_t)GetProcAddress(*cs_D3D8Handle(), "Direct3DCreate8")) == NULL)
		ThfError(GetLastError(), "%s: LoadFuncFromD3D8 failed.", __FUNCTION__);
}

Direct3DCreate8_t* cs_VanillaDirect3DCreate8(void)
{
	static Direct3DCreate8_t inner = NULL;

	if (inner == NULL)
		cs_VanillaDirect3DCreate8LazyInit(&inner);
	return &inner;
}

IDirect3D8* cs_ModDirect3DCreate8(UINT SDKVersion)
{
	IDirect3D8* ret;

	if (*cs_VanillaDirect3DCreate8() == NULL)
		return NULL;

	ret = (*cs_VanillaDirect3DCreate8())(SDKVersion);

	if (ret != NULL)
	{
		DWORD orig_protect;
		IDirect3D8Vtbl* vtbl = ret->lpVtbl;

		if (!VirtualProtect(&vtbl->CreateDevice, sizeof(vtbl->CreateDevice), PAGE_READWRITE, &orig_protect))
		{
			ThfError(GetLastError(), "%s: VirtualProtect failed.", __FUNCTION__);
			return NULL;
		}

		IDirect3D8ExtraDataTableInsert(*cs_IDirect3D8ExtraDataTable(), ret, AllocateIDirect3D8ExtraData(vtbl->CreateDevice));
		vtbl->CreateDevice = ModIDirect3D8CreateDevice;

		// best effort
		if (!VirtualProtect(&vtbl->CreateDevice, sizeof(vtbl->CreateDevice), orig_protect, &orig_protect))
		{
			ThfError(GetLastError(), "%s: VirtualProtect failed.", __FUNCTION__);
			return NULL;
		}
	}

	return ret;
}

IDirect3D8* WINAPI ModDirect3DCreate8(UINT SDKVersion)
{
	IDirect3D8* ret;

	EnterCriticalSection(&g_CS);
	ret = cs_ModDirect3DCreate8(SDKVersion);
	LeaveCriticalSection(&g_CS);

	return ret;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		ThfLog("Attaching to the process: begin");
		InitializeCriticalSection(&g_CS);
		ThfLog("Attaching to the process: succeeded");
		break;
	}
	case DLL_PROCESS_DETACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		break;
	}

	return TRUE;
}

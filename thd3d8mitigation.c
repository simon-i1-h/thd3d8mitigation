﻿#include "thd3d8mitigation.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define CINTERFACE
#include <d3d8.h>

#include <tchar.h>

#include <stdlib.h>

// Naming convention:
//   Thf*: thd3d8mitigation
//   cs_*: Critical section
//   g_*: Global variable
//   *_t: Type identifier

// XXX TODO error handling
// XXX TODO 東方と同様にファイルにもロギング
// XXX TODO review
// XXX TODO コード整形
// XXX TODO Windowsのビルド番号を見て機能の有効/無効を自動で切り替えたい
//   https://docs.microsoft.com/ja-jp/windows/win32/devnotes/rtlgetversion
//   ↑これをLoadLibrary + GetProcAddressで
//   引数の型はwindows.hでいいらしい。
// XXX TODO 潜在的なデッドロックの可能性を排除

static CRITICAL_SECTION g_CS;

struct IDirect3D8ExtraDataTable** cs_D3D8ExDataTable(void)
{
	static struct IDirect3D8ExtraDataTable* inner = NULL;

	if (inner == NULL)
		inner = IDirect3D8ExtraDataTableNew();
	return &inner;
}

struct IDirect3DDevice8ExtraDataTable** cs_D3DDev8ExDataTable(void)
{
	static struct IDirect3DDevice8ExtraDataTable* inner = NULL;

	if (inner == NULL)
		inner = IDirect3DDevice8ExtraDataTableNew();
	return &inner;
}

struct IDirect3D8ExtraData* AllocateIDirect3D8ExtraData(IDirect3D8CreateDevice_t VanillaCreateDevice, IDirect3D8Release_t VanillaRelease)
{
	struct IDirect3D8ExtraData* ret;

	if ((ret = malloc(sizeof(*ret))) == NULL) // XXX TODO error handling
		return NULL;
	*ret = (struct IDirect3D8ExtraData){ .VanillaCreateDevice = VanillaCreateDevice, .VanillaRelease = VanillaRelease };
	return ret;
}

struct IDirect3DDevice8ExtraData* AllocateIDirect3DDevice8ExtraData(IDirect3DDevice8Present_t VanillaPresent, IDirect3DDevice8Release_t VanillaRelease, IDirect3DDevice8Reset_t VanillaReset, D3DPRESENT_PARAMETERS pp)
{
	struct IDirect3DDevice8ExtraData* ret;

	if ((ret = malloc(sizeof(*ret))) == NULL) // XXX TODO error handling
		return NULL;
	*ret = (struct IDirect3DDevice8ExtraData){ .VanillaPresent = VanillaPresent, .VanillaRelease = VanillaRelease, .VanillaReset = VanillaReset, .pp = pp };
	return ret;
}

HRESULT ModIDirect3DDevice8PresentWithGetRasterStatus(IDirect3DDevice8* me, struct IDirect3DDevice8ExtraData* me_exdata, CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion)
{
	D3DRASTER_STATUS stat;
	HRESULT ret;

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

HRESULT __stdcall ModIDirect3DDevice8Present(IDirect3DDevice8* me, CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion)
{
	// me_exdataはmeに1対1で紐づく拡張プロパティと考えられるので、meのメソッド内ではデータ競合や競合状態について考えなくてよい。
	struct IDirect3DDevice8ExtraData* me_exdata;

	EnterCriticalSection(&g_CS);
	me_exdata = IDirect3DDevice8ExtraDataTableGet(*cs_D3DDev8ExDataTable(), me);
	LeaveCriticalSection(&g_CS);

	if (me_exdata == NULL)
		return E_FAIL; // XXX TODO logging

	if (me_exdata->pp.Windowed || (me_exdata->pp.FullScreen_PresentationInterval != D3DPRESENT_INTERVAL_DEFAULT && me_exdata->pp.FullScreen_PresentationInterval != D3DPRESENT_INTERVAL_ONE && me_exdata->pp.FullScreen_PresentationInterval != D3DPRESENT_INTERVAL_TWO && me_exdata->pp.FullScreen_PresentationInterval != D3DPRESENT_INTERVAL_THREE && me_exdata->pp.FullScreen_PresentationInterval != D3DPRESENT_INTERVAL_FOUR))
		return me_exdata->VanillaPresent(me, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
	else
		return ModIDirect3DDevice8PresentWithGetRasterStatus(me, me_exdata, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
}

ULONG cs_ModIDirect3DDevice8Release(IDirect3DDevice8* me)
{
	ULONG ret;
	struct IDirect3DDevice8ExtraData* me_exdata;

	if ((me_exdata = IDirect3DDevice8ExtraDataTableGet(*cs_D3DDev8ExDataTable(), me)) == NULL)
		return 0; // XXX FIXME error handling

	ret = me_exdata->VanillaRelease(me); // XXX TODO 潜在的なデッドロックの可能性を排除

	// XXX TODO 解放されたかどうかはほかの方法で検出したほうがいいかも。その場合、Releaseのhookは適切ではないかもしれない。
	// https://docs.microsoft.com/en-us/previous-versions/windows/embedded/ms890669(v=msdn.10)?redirectedfrom=MSDN
	// https://docs.microsoft.com/ja-jp/windows/win32/api/unknwn/nf-unknwn-iunknown-release
	if (ret == 0)
	{
		free(me_exdata);
		IDirect3DDevice8ExtraDataTableErase(*cs_D3DDev8ExDataTable(), me);
		IDirect3DDevice8ExtraDataTableShrinkToFit(*cs_D3DDev8ExDataTable());
	}

	return ret;
}

ULONG __stdcall ModIDirect3DDevice8Release(IDirect3DDevice8* me)
{
	ULONG ret;

	EnterCriticalSection(&g_CS);
	ret = cs_ModIDirect3DDevice8Release(me);
	LeaveCriticalSection(&g_CS);

	return ret;
}

HRESULT cs_ModIDirect3DDevice8Reset(IDirect3DDevice8* me, D3DPRESENT_PARAMETERS* pPresentationParameters)
{
	HRESULT ret;
	struct IDirect3DDevice8ExtraData* me_exdata;

	if ((me_exdata = IDirect3DDevice8ExtraDataTableGet(*cs_D3DDev8ExDataTable(), me)) == NULL)
		return E_FAIL; // XXX FIXME error handling

	ret = me_exdata->VanillaReset(me, pPresentationParameters); // XXX TODO 潜在的なデッドロックの可能性を排除

	if (SUCCEEDED(ret))
		me_exdata->pp = *pPresentationParameters;

	return ret;
}

HRESULT __stdcall ModIDirect3DDevice8Reset(IDirect3DDevice8* me, D3DPRESENT_PARAMETERS* pPresentationParameters)
{
	HRESULT ret;

	EnterCriticalSection(&g_CS);
	ret = cs_ModIDirect3DDevice8Reset(me, pPresentationParameters);
	LeaveCriticalSection(&g_CS);

	return ret;
}

HRESULT cs_ModIDirect3D8CreateDevice(IDirect3D8* me, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice8** ppReturnedDeviceInterface)
{
	HRESULT ret;

	struct IDirect3D8ExtraData* me_exdata;
	if ((me_exdata = IDirect3D8ExtraDataTableGet(*cs_D3D8ExDataTable(), me)) == NULL)
		return E_FAIL;

	// XXX TODO 潜在的なデッドロックの可能性を排除
	ret = me_exdata->VanillaCreateDevice(me, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, ppReturnedDeviceInterface);

	if (SUCCEEDED(ret))
	{
		DWORD orig_protect;
		IDirect3DDevice8* device = *ppReturnedDeviceInterface;
		IDirect3DDevice8Vtbl* vtbl = device->lpVtbl;

		// hook IDirect3DDevice8::Present, IDirect3DDevice8::Release (inherit from IUnknown::Release), and IDirect3DDevice8::Reset

		if (!VirtualProtect(vtbl, sizeof(*vtbl), PAGE_READWRITE, &orig_protect))
		{
			ThfError(GetLastError(), "%s: VirtualProtect (PAGE_READWRITE) failed.", __FUNCTION__);
			return E_FAIL;
		}

		IDirect3DDevice8ExtraDataTableInsert(*cs_D3DDev8ExDataTable(), device, AllocateIDirect3DDevice8ExtraData(vtbl->Present, vtbl->Release, vtbl->Reset, *pPresentationParameters));
		vtbl->Present = ModIDirect3DDevice8Present;
		vtbl->Release = ModIDirect3DDevice8Release;
		vtbl->Reset = ModIDirect3DDevice8Reset;

		// best effort
		if (!VirtualProtect(vtbl, sizeof(*vtbl), orig_protect, &orig_protect))
			ThfError(GetLastError(), "%s: VirtualProtect (original protect) failed.", __FUNCTION__);
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

ULONG cs_ModIDirect3D8Release(IDirect3D8* me)
{
	ULONG ret;
	struct IDirect3D8ExtraData* me_exdata;

	if ((me_exdata = IDirect3D8ExtraDataTableGet(*cs_D3D8ExDataTable(), me)) == NULL)
		return 0; // XXX FIXME error handling

	ret = me_exdata->VanillaRelease(me); // XXX TODO 潜在的なデッドロックの可能性を排除

	// XXX TODO 解放されたかどうかはほかの方法で検出したほうがいいかも。その場合、Releaseのhookは適切ではないかもしれない。
	// https://docs.microsoft.com/en-us/previous-versions/windows/embedded/ms890669(v=msdn.10)?redirectedfrom=MSDN
	// https://docs.microsoft.com/ja-jp/windows/win32/api/unknwn/nf-unknwn-iunknown-release
	if (ret == 0)
	{
		free(me_exdata);
		IDirect3D8ExtraDataTableErase(*cs_D3D8ExDataTable(), me);
		IDirect3D8ExtraDataTableShrinkToFit(*cs_D3D8ExDataTable());
	}

	return ret;
}

ULONG __stdcall ModIDirect3D8Release(IDirect3D8* me)
{
	ULONG ret;

	EnterCriticalSection(&g_CS);
	ret = cs_ModIDirect3D8Release(me);
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

	ret = (*cs_VanillaDirect3DCreate8())(SDKVersion); // XXX TODO 潜在的なデッドロックの可能性を排除

	if (ret != NULL)
	{
		DWORD orig_protect;
		IDirect3D8Vtbl* vtbl = ret->lpVtbl;

		// hook IDirect3D8::CreateDevice and IDirect3D8::Release (inherit from IUnknown::Release)

		if (!VirtualProtect(vtbl, sizeof(*vtbl), PAGE_READWRITE, &orig_protect))
		{
			ThfError(GetLastError(), "%s: VirtualProtect (PAGE_READWRITE) failed.", __FUNCTION__);
			return NULL;
		}

		IDirect3D8ExtraDataTableInsert(*cs_D3D8ExDataTable(), ret, AllocateIDirect3D8ExtraData(vtbl->CreateDevice, vtbl->Release));
		vtbl->CreateDevice = ModIDirect3D8CreateDevice;
		vtbl->Release = ModIDirect3D8Release;

		// best effort
		if (!VirtualProtect(vtbl, sizeof(*vtbl), orig_protect, &orig_protect))
			ThfError(GetLastError(), "%s: VirtualProtect (original protect) failed.", __FUNCTION__);
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
		// https://docs.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-best-practices#general-best-practices
		OutputDebugStringA(THF_LOG_PREFIX __FUNCTION__ ": Version: " THF_VERSION "\n");
		OutputDebugStringA(THF_LOG_PREFIX __FUNCTION__ ": Attaching to the process: begin\n");
		InitializeCriticalSection(&g_CS);
		OutputDebugStringA(THF_LOG_PREFIX __FUNCTION__ ": Attaching to the process: succeeded\n");
		break;
	}
	case DLL_PROCESS_DETACH:
		// https://docs.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-best-practices#best-practices-for-synchronization
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		break;
	}

	return TRUE;
}

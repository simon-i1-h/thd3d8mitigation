#include "thd3d8mitigation.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define CINTERFACE
#include <d3d8.h>

#include <tchar.h>

#include <stdbool.h>

// Naming convention:
//   Thf*: thd3d8mitigation: XXX TODO rename
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
// XXX TODO 高精度タイマーを使った代替実装。設定ファイルで切り替え可能にする。
// XXX TODO ログにタグを付ける


static CRITICAL_SECTION g_CS;
HMODULE g_D3D8Handle;
Direct3DCreate8_t g_VanillaDirect3DCreate8;
struct IDirect3D8ExtraDataTable* g_D3D8ExDataTable;
struct IDirect3DDevice8ExtraDataTable* g_D3DDev8ExDataTable;

HRESULT ModIDirect3DDevice8PresentWithGetRasterStatus(IDirect3DDevice8* me, struct IDirect3DDevice8ExtraData* me_exdata, CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion)
{
	D3DRASTER_STATUS stat;
	HRESULT ret;

	do {
		if (FAILED(me->lpVtbl->GetRasterStatus(me, &stat)))
		{
			LogInfo("%s: IDirect3DDevice8::GetRasterStatus failed.", __FUNCTION__);
			return E_FAIL;
		}
		Sleep(0); // XXX TODO SleepEx?
	} while (stat.InVBlank);

	ret = me_exdata->VanillaPresent(me, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);

	do {
		if (FAILED(me->lpVtbl->GetRasterStatus(me, &stat)))
		{
			LogInfo("%s: IDirect3DDevice8::GetRasterStatus failed.", __FUNCTION__);
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

	// load from critical section
	EnterCriticalSection(&g_CS);
	me_exdata = IDirect3DDevice8ExtraDataTableGet(g_D3DDev8ExDataTable, me);
	LeaveCriticalSection(&g_CS);

	if (me_exdata == NULL)
		return E_FAIL; // XXX TODO logging

	if (me_exdata->pp.Windowed || (me_exdata->pp.FullScreen_PresentationInterval != D3DPRESENT_INTERVAL_DEFAULT && me_exdata->pp.FullScreen_PresentationInterval != D3DPRESENT_INTERVAL_ONE && me_exdata->pp.FullScreen_PresentationInterval != D3DPRESENT_INTERVAL_TWO && me_exdata->pp.FullScreen_PresentationInterval != D3DPRESENT_INTERVAL_THREE && me_exdata->pp.FullScreen_PresentationInterval != D3DPRESENT_INTERVAL_FOUR))
		return me_exdata->VanillaPresent(me, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
	else
		return ModIDirect3DDevice8PresentWithGetRasterStatus(me, me_exdata, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
}

ULONG __stdcall ModIDirect3DDevice8Release(IDirect3DDevice8* me)
{
	ULONG ret;
	// me_exdataはmeに1対1で紐づく拡張プロパティと考えられるので、meのメソッド内ではデータ競合や競合状態について考えなくてよい。
	struct IDirect3DDevice8ExtraData* me_exdata;

	// load from critical section
	EnterCriticalSection(&g_CS);
	me_exdata = IDirect3DDevice8ExtraDataTableGet(g_D3DDev8ExDataTable, me);
	LeaveCriticalSection(&g_CS);

	if (me_exdata == NULL)
		return 0; // XXX FIXME error handling

	ret = me_exdata->VanillaRelease(me);

	// XXX TODO 解放されたかどうかはほかの方法で検出したほうがいいかも。その場合、Releaseのhookは適切ではないかもしれない。
	// https://docs.microsoft.com/en-us/previous-versions/windows/embedded/ms890669(v=msdn.10)?redirectedfrom=MSDN
	// https://docs.microsoft.com/ja-jp/windows/win32/api/unknwn/nf-unknwn-iunknown-release
	if (ret == 0)
	{
		// store to critical section
		EnterCriticalSection(&g_CS);
		IDirect3DDevice8ExtraDataTableErase(g_D3DDev8ExDataTable, me);
		IDirect3DDevice8ExtraDataTableShrinkToFit(g_D3DDev8ExDataTable);
		LeaveCriticalSection(&g_CS);
	}

	return ret;
}

HRESULT __stdcall ModIDirect3DDevice8Reset(IDirect3DDevice8* me, D3DPRESENT_PARAMETERS* pPresentationParameters)
{
	HRESULT ret;
	// me_exdataはmeに1対1で紐づく拡張プロパティと考えられるので、meのメソッド内ではデータ競合や競合状態について考えなくてよい。
	struct IDirect3DDevice8ExtraData* me_exdata;

	// load from critical section
	EnterCriticalSection(&g_CS);
	me_exdata = IDirect3DDevice8ExtraDataTableGet(g_D3DDev8ExDataTable, me);
	LeaveCriticalSection(&g_CS);

	if (me_exdata == NULL)
		return E_FAIL; // XXX FIXME error handling

	ret = me_exdata->VanillaReset(me, pPresentationParameters);
	if (SUCCEEDED(ret))
		me_exdata->pp = *pPresentationParameters;

	return ret;
}

HRESULT __stdcall ModIDirect3D8CreateDevice(IDirect3D8* me, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice8** ppReturnedDeviceInterface)
{
	HRESULT ret;
	// me_exdataはmeに1対1で紐づく拡張プロパティと考えられるので、meのメソッド内ではデータ競合や競合状態について考えなくてよい。
	struct IDirect3D8ExtraData* me_exdata;
	DWORD orig_protect;
	IDirect3DDevice8* d3ddev8;
	IDirect3DDevice8Vtbl* vtbl;
	struct IDirect3DDevice8ExtraData d3ddev8_exdata;

	// load from critical section
	EnterCriticalSection(&g_CS);
	me_exdata = IDirect3D8ExtraDataTableGet(g_D3D8ExDataTable, me);
	LeaveCriticalSection(&g_CS);

	if (me_exdata == NULL)
		return E_FAIL; // XXX TODO logging

	ret = me_exdata->VanillaCreateDevice(me, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, ppReturnedDeviceInterface);
	if (FAILED(ret))
		return ret;

	// hook IDirect3DDevice8::Present, IDirect3DDevice8::Release (inherit from IUnknown::Release), and IDirect3DDevice8::Reset

	d3ddev8 = *ppReturnedDeviceInterface;
	vtbl = d3ddev8->lpVtbl;

	if (!VirtualProtect(vtbl, sizeof(*vtbl), PAGE_READWRITE, &orig_protect))
	{
		LogError(GetLastError(), "%s: VirtualProtect (PAGE_READWRITE) failed.", __FUNCTION__);
		return E_FAIL;
	}

	d3ddev8_exdata = (struct IDirect3DDevice8ExtraData){ .VanillaPresent = vtbl->Present, .VanillaRelease = vtbl->Release, .VanillaReset = vtbl->Reset, .pp = *pPresentationParameters };
	vtbl->Present = ModIDirect3DDevice8Present;
	vtbl->Release = ModIDirect3DDevice8Release;
	vtbl->Reset = ModIDirect3DDevice8Reset;

	// best effort
	if (!VirtualProtect(vtbl, sizeof(*vtbl), orig_protect, &orig_protect))
		LogError(GetLastError(), "%s: VirtualProtect (original protect) failed.", __FUNCTION__);

	// store to critical section
	EnterCriticalSection(&g_CS);
	IDirect3DDevice8ExtraDataTableInsert(g_D3DDev8ExDataTable, d3ddev8, d3ddev8_exdata); // XXX TODO error handling
	LeaveCriticalSection(&g_CS);

	return ret;
}

ULONG __stdcall ModIDirect3D8Release(IDirect3D8* me)
{
	ULONG ret;
	// me_exdataはmeに1対1で紐づく拡張プロパティと考えられるので、meのメソッド内ではデータ競合や競合状態について考えなくてよい。
	struct IDirect3D8ExtraData* me_exdata;

	// load from critical section
	EnterCriticalSection(&g_CS);
	me_exdata = IDirect3D8ExtraDataTableGet(g_D3D8ExDataTable, me);
	LeaveCriticalSection(&g_CS);

	if (me_exdata == NULL)
		return 0; // XXX FIXME error handling

	ret = me_exdata->VanillaRelease(me);

	// XXX TODO 解放されたかどうかはほかの方法で検出したほうがいいかも。その場合、Releaseのhookは適切ではないかもしれない。
	// https://docs.microsoft.com/en-us/previous-versions/windows/embedded/ms890669(v=msdn.10)?redirectedfrom=MSDN
	// https://docs.microsoft.com/ja-jp/windows/win32/api/unknwn/nf-unknwn-iunknown-release
	if (ret == 0)
	{
		// store to critical section
		EnterCriticalSection(&g_CS);
		IDirect3D8ExtraDataTableErase(g_D3D8ExDataTable, me);
		IDirect3D8ExtraDataTableShrinkToFit(g_D3D8ExDataTable);
		LeaveCriticalSection(&g_CS);
	}

	return ret;
}

bool InitD3D8Handle(HMODULE* ret)
{
	char sysdirpath[MAX_PATH + 1];
	char* sysdllpath;
	unsigned int len;
	DWORD err;

	if ((len = GetSystemDirectoryA(sysdirpath, sizeof(sysdirpath))) == 0)
	{
		LogError(GetLastError(), "%s: GetSystemDirectoryA failed.", __FUNCTION__);
		return false;
	}

	if (len > sizeof(sysdirpath))  // XXX TODO review condition
	{
		LogInfo("%s: GetSystemDirectoryA failed.: Path too long.", __FUNCTION__); // XXX TODO error message
		return false;
	}

	if (myasprintf(&sysdllpath, "%s\\d3d8.dll", sysdirpath) < 0)
	{
		LogInfo("%s: myasprintf failed.", __FUNCTION__);
		return false;
	}

	*ret = LoadLibraryA(sysdllpath);
	err = GetLastError();
	free(sysdllpath);
	if (*ret == NULL)
	{
		LogError(err, "%s: LoadLibraryA failed.", __FUNCTION__);
		return false;
	}

	return true;
}

BOOL InitVanillaDirect3DCreate8(HMODULE D3D8Handle, Direct3DCreate8_t* ret)
{
	if ((*ret = (Direct3DCreate8_t)GetProcAddress(D3D8Handle, "Direct3DCreate8")) == NULL)
	{
		LogError(GetLastError(), "%s: LoadFuncFromD3D8 failed.", __FUNCTION__);
		return false;
	}

	return true;
}

enum InitStatus {
	INITSTATUS_UNINITED,
	INITSTATUS_SUCCEEDED,
	INITSTATUS_FAILED
};

// XXX TODO Init専用のクリティカルセクションがあったほうがいいかも
bool Init(void)
{
	static enum InitStatus g_initstatus = INITSTATUS_UNINITED;

	// XXX TODO file handle for logging

	EnterCriticalSection(&g_CS);

	switch (g_initstatus)
	{
	case INITSTATUS_SUCCEEDED:
		LeaveCriticalSection(&g_CS);
		return true;
	case INITSTATUS_FAILED:
		LeaveCriticalSection(&g_CS);
		return false;
	case INITSTATUS_UNINITED:
		break;
	}

	// init

	if (!InitD3D8Handle(&g_D3D8Handle))
	{
		LeaveCriticalSection(&g_CS);
		return false; // XXX TODO logging
	}

	if (!InitVanillaDirect3DCreate8(g_D3D8Handle, &g_VanillaDirect3DCreate8))
	{
		LeaveCriticalSection(&g_CS);
		return false; // XXX TODO logging
	}

	g_D3D8ExDataTable = IDirect3D8ExtraDataTableNew();
	g_D3DDev8ExDataTable = IDirect3DDevice8ExtraDataTableNew();

	LeaveCriticalSection(&g_CS);

	return true;
}

IDirect3D8* WINAPI ModDirect3DCreate8(UINT SDKVersion)
{
	IDirect3D8* ret;
	Direct3DCreate8_t VanillaDirect3DCreate8;
	DWORD orig_protect;
	IDirect3D8Vtbl* vtbl;
	struct IDirect3D8ExtraData d3d8_exdata;

	if (!Init())
		return NULL; // XXX TODO logging

	// load from critical section
	EnterCriticalSection(&g_CS);
	VanillaDirect3DCreate8 = g_VanillaDirect3DCreate8;
	LeaveCriticalSection(&g_CS);

	if ((ret = VanillaDirect3DCreate8(SDKVersion)) == NULL)
		return NULL; // XXX TODO logging

	// hook IDirect3D8::CreateDevice and IDirect3D8::Release (inherit from IUnknown::Release)

	vtbl = ret->lpVtbl;

	if (!VirtualProtect(vtbl, sizeof(*vtbl), PAGE_READWRITE, &orig_protect))
	{
		LogError(GetLastError(), "%s: VirtualProtect (PAGE_READWRITE) failed.", __FUNCTION__);
		return NULL;
	}

	d3d8_exdata = (struct IDirect3D8ExtraData){ .VanillaCreateDevice = vtbl->CreateDevice, .VanillaRelease = vtbl->Release };
	vtbl->CreateDevice = ModIDirect3D8CreateDevice;
	vtbl->Release = ModIDirect3D8Release;

	// best effort
	if (!VirtualProtect(vtbl, sizeof(*vtbl), orig_protect, &orig_protect))
		LogError(GetLastError(), "%s: VirtualProtect (original protect) failed.", __FUNCTION__);

	// store to critical section
	EnterCriticalSection(&g_CS);
	IDirect3D8ExtraDataTableInsert(g_D3D8ExDataTable, ret, d3d8_exdata); // XXX TODO error handling
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

#include "thd3d8mitigation.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define CINTERFACE
#include <d3d8.h>

#include <tchar.h>

#include <stdbool.h>

// Naming convention:
//   cs_*: Critical section
//   g_*: Global variable
//   *_t: Type identifier
//   Mod*: Modified
//   V*: Function with va_list

// XXX TODO error handling
// XXX TODO review
// XXX TODO コード整形
// XXX TODO Direct3D 8の振る舞いを見て機能の有効/無効を自動で切り替えたい
// XXX TODO 高精度タイマーを使った代替実装。設定ファイルで切り替え可能にする。

enum InitStatus {
	INITSTATUS_UNINITED,
	INITSTATUS_SUCCEEDED,
	INITSTATUS_FAILED
};

CRITICAL_SECTION g_CS;

static HMODULE g_D3D8Handle;
static Direct3DCreate8_t g_VanillaDirect3DCreate8;
static struct IDirect3D8ExtraDataTable* g_D3D8ExDataTable;
static struct IDirect3DDevice8ExtraDataTable* g_D3DDev8ExDataTable;

HRESULT ModIDirect3DDevice8PresentWithGetRasterStatus(IDirect3DDevice8* me, struct IDirect3DDevice8ExtraData* me_exdata, CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion)
{
	D3DRASTER_STATUS stat;
	HRESULT ret;

	do {
		if (FAILED(me->lpVtbl->GetRasterStatus(me, &stat)))
		{
			Log("%s: error: IDirect3DDevice8::GetRasterStatus failed.", __FUNCTION__);
			return E_FAIL;
		}
		Sleep(0); // XXX TODO SleepEx?
	} while (stat.InVBlank);

	ret = me_exdata->VanillaPresent(me, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);

	do {
		if (FAILED(me->lpVtbl->GetRasterStatus(me, &stat)))
		{
			Log("%s: error: IDirect3DDevice8::GetRasterStatus failed.", __FUNCTION__);
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
	{
		Log("%s: error: IDirect3DDevice8ExtraDataTableGet failed.", __FUNCTION__);
		return E_FAIL;
	}

	if (me_exdata->pp.Windowed || (me_exdata->pp.FullScreen_PresentationInterval != D3DPRESENT_INTERVAL_DEFAULT && me_exdata->pp.FullScreen_PresentationInterval != D3DPRESENT_INTERVAL_ONE && me_exdata->pp.FullScreen_PresentationInterval != D3DPRESENT_INTERVAL_TWO && me_exdata->pp.FullScreen_PresentationInterval != D3DPRESENT_INTERVAL_THREE && me_exdata->pp.FullScreen_PresentationInterval != D3DPRESENT_INTERVAL_FOUR))
		return me_exdata->VanillaPresent(me, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
	else
		return ModIDirect3DDevice8PresentWithGetRasterStatus(me, me_exdata, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
}

ULONG cs_ModIDirect3DDevice8ReleaseImpl(IDirect3DDevice8* me)
{
	ULONG ret;
	// me_exdataはmeに1対1で紐づく拡張プロパティと考えられるので、meのメソッド内ではデータ競合や競合状態について考えなくてよい。
	struct IDirect3DDevice8ExtraData* me_exdata;

	if ((me_exdata = IDirect3DDevice8ExtraDataTableGet(g_D3DDev8ExDataTable, me)) == NULL)
	{
		Log("%s: warning: IDirect3DDevice8ExtraDataTableGet failed. Resource is leaking.", __FUNCTION__);
		return 0;
	}

	if ((ret = me_exdata->VanillaRelease(me)) == 0)
	{
		IDirect3DDevice8ExtraDataTableErase(g_D3DDev8ExDataTable, me);
		IDirect3DDevice8ExtraDataTableShrinkToFit(g_D3DDev8ExDataTable);
	}

	return ret;
}

ULONG __stdcall ModIDirect3DDevice8Release(IDirect3DDevice8* me)
{
	ULONG ret;

	EnterCriticalSection(&g_CS);
	ret = cs_ModIDirect3DDevice8ReleaseImpl(me);
	LeaveCriticalSection(&g_CS);
	return ret;
}

HRESULT cs_ModIDirect3DDevice8ResetImpl(IDirect3DDevice8* me, D3DPRESENT_PARAMETERS* pPresentationParameters)
{
	HRESULT ret;
	// me_exdataはmeに1対1で紐づく拡張プロパティと考えられるので、meのメソッド内ではデータ競合や競合状態について考えなくてよい。
	struct IDirect3DDevice8ExtraData* me_exdata;

	if ((me_exdata = IDirect3DDevice8ExtraDataTableGet(g_D3DDev8ExDataTable, me)) == NULL)
	{
		Log("%s: error: IDirect3DDevice8ExtraDataTableGet failed.", __FUNCTION__);
		return E_FAIL;
	}

	ret = me_exdata->VanillaReset(me, pPresentationParameters);
	if (SUCCEEDED(ret))
		me_exdata->pp = *pPresentationParameters;

	return ret;
}

HRESULT __stdcall ModIDirect3DDevice8Reset(IDirect3DDevice8* me, D3DPRESENT_PARAMETERS* pPresentationParameters)
{
	HRESULT ret;

	EnterCriticalSection(&g_CS);
	ret = cs_ModIDirect3DDevice8ResetImpl(me, pPresentationParameters);
	LeaveCriticalSection(&g_CS);
	return ret;
}

HRESULT cs_ModIDirect3D8CreateDeviceImpl(IDirect3D8* me, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice8** ppReturnedDeviceInterface)
{
	HRESULT ret;
	// me_exdataはmeに1対1で紐づく拡張プロパティと考えられるので、meのメソッド内ではデータ競合や競合状態について考えなくてよい。
	struct IDirect3D8ExtraData* me_exdata;
	DWORD orig_protect;
	IDirect3DDevice8* d3ddev8;
	IDirect3DDevice8Vtbl* vtbl;
	struct IDirect3DDevice8ExtraData d3ddev8_exdata;

	if ((me_exdata = IDirect3D8ExtraDataTableGet(g_D3D8ExDataTable, me)) == NULL)
	{
		Log("%s: error: IDirect3D8ExtraDataTableGet failed.", __FUNCTION__);
		return E_FAIL;
	}

	ret = me_exdata->VanillaCreateDevice(me, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, ppReturnedDeviceInterface);
	if (FAILED(ret))
		return ret;

	// hook IDirect3DDevice8::Present, IDirect3DDevice8::Release (inherit from IUnknown::Release), and IDirect3DDevice8::Reset

	d3ddev8 = *ppReturnedDeviceInterface;
	vtbl = d3ddev8->lpVtbl;

	if (!VirtualProtect(vtbl, sizeof(*vtbl), PAGE_READWRITE, &orig_protect))
	{
		LogWithErrorCode(GetLastError(), "%s: error: VirtualProtect (PAGE_READWRITE) failed", __FUNCTION__);
		return E_FAIL;
	}

	d3ddev8_exdata = (struct IDirect3DDevice8ExtraData){ .VanillaPresent = vtbl->Present, .VanillaRelease = vtbl->Release, .VanillaReset = vtbl->Reset, .pp = *pPresentationParameters };
	vtbl->Present = ModIDirect3DDevice8Present;
	vtbl->Release = ModIDirect3DDevice8Release;
	vtbl->Reset = ModIDirect3DDevice8Reset;

	// best effort
	if (!VirtualProtect(vtbl, sizeof(*vtbl), orig_protect, &orig_protect))
		LogWithErrorCode(GetLastError(), "%s: warning: VirtualProtect (original protect) failed.", __FUNCTION__);

	IDirect3DDevice8ExtraDataTableInsert(g_D3DDev8ExDataTable, d3ddev8, d3ddev8_exdata); // XXX TODO error handling

	return ret;
}

HRESULT __stdcall ModIDirect3D8CreateDevice(IDirect3D8* me, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice8** ppReturnedDeviceInterface)
{
	HRESULT ret;

	EnterCriticalSection(&g_CS);
	ret = cs_ModIDirect3D8CreateDeviceImpl(me, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, ppReturnedDeviceInterface);
	LeaveCriticalSection(&g_CS);
	return ret;
}

ULONG __stdcall cs_ModIDirect3D8ReleaseImpl(IDirect3D8* me)
{
	ULONG ret;
	// me_exdataはmeに1対1で紐づく拡張プロパティと考えられるので、meのメソッド内ではデータ競合や競合状態について考えなくてよい。
	struct IDirect3D8ExtraData* me_exdata;

	if ((me_exdata = IDirect3D8ExtraDataTableGet(g_D3D8ExDataTable, me)) == NULL)
	{
		Log("%s: warning: IDirect3D8ExtraDataTableGet failed. Resource is leaking.", __FUNCTION__);
		return 0;
	}

	if ((ret = me_exdata->VanillaRelease(me)) == 0)
	{
		IDirect3D8ExtraDataTableErase(g_D3D8ExDataTable, me);
		IDirect3D8ExtraDataTableShrinkToFit(g_D3D8ExDataTable);
	}

	return ret;
}

ULONG __stdcall ModIDirect3D8Release(IDirect3D8* me)
{
	ULONG ret;

	EnterCriticalSection(&g_CS);
	ret = cs_ModIDirect3D8ReleaseImpl(me);
	LeaveCriticalSection(&g_CS);
	return ret;
}

IDirect3D8* cs_ModDirect3DCreate8Impl(UINT SDKVersion)
{
	IDirect3D8* ret;
	DWORD orig_protect;
	IDirect3D8Vtbl* vtbl;
	struct IDirect3D8ExtraData d3d8_exdata;

	if ((ret = g_VanillaDirect3DCreate8(SDKVersion)) == NULL)
	{
		Log("%s: error: g_VanillaDirect3DCreate8 failed.", __FUNCTION__);
		return NULL;
	}

	// hook IDirect3D8::CreateDevice and IDirect3D8::Release (inherit from IUnknown::Release)

	vtbl = ret->lpVtbl;

	if (!VirtualProtect(vtbl, sizeof(*vtbl), PAGE_READWRITE, &orig_protect))
	{
		LogWithErrorCode(GetLastError(), "%s: error: VirtualProtect (PAGE_READWRITE) failed.", __FUNCTION__);
		return NULL;
	}

	d3d8_exdata = (struct IDirect3D8ExtraData){ .VanillaCreateDevice = vtbl->CreateDevice, .VanillaRelease = vtbl->Release };
	vtbl->CreateDevice = ModIDirect3D8CreateDevice;
	vtbl->Release = ModIDirect3D8Release;

	// best effort
	if (!VirtualProtect(vtbl, sizeof(*vtbl), orig_protect, &orig_protect))
		LogWithErrorCode(GetLastError(), "%s: warning: VirtualProtect (original protect) failed.", __FUNCTION__);

	IDirect3D8ExtraDataTableInsert(g_D3D8ExDataTable, ret, d3d8_exdata); // XXX TODO error handling

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
		LogWithErrorCode(GetLastError(), "%s: error: GetSystemDirectoryA failed.", __FUNCTION__);
		return false;
	}

	if (len >= sizeof(sysdirpath))
	{
		LogWithErrorCode(ERROR_INSUFFICIENT_BUFFER, "%s: error: GetSystemDirectoryA failed.", __FUNCTION__);
		return false;
	}

	if (myasprintf(&sysdllpath, "%s\\d3d8.dll", sysdirpath) < 0)
	{
		Log("%s: error: myasprintf failed.", __FUNCTION__);
		return false;
	}

	*ret = LoadLibraryA(sysdllpath);
	err = GetLastError();
	free(sysdllpath);
	if (*ret == NULL)
	{
		LogWithErrorCode(err, "%s: error: LoadLibraryA failed.", __FUNCTION__);
		return false;
	}

	return true;
}

bool InitVanillaDirect3DCreate8(HMODULE D3D8Handle, Direct3DCreate8_t* ret)
{
	if ((*ret = (Direct3DCreate8_t)GetProcAddress(D3D8Handle, "Direct3DCreate8")) == NULL)
	{
		LogWithErrorCode(GetLastError(), "%s: error: LoadFuncFromD3D8 failed.", __FUNCTION__);
		return false;
	}

	return true;
}

bool cs_LogInitImpl(void)
{
	char exepath[MAX_PATH + 1];
	char exedrivepath[_MAX_DRIVE + 1];
	char exedirpath[_MAX_DIR + 1];
	char* logpath;
	DWORD err, len;

	if ((len = GetModuleFileNameA(NULL, exepath, sizeof(exepath))) == 0)
	{
		LogWithErrorCode(GetLastError(), "%s: error: GetModuleFileNameA failed.", __FUNCTION__);
		return false;
	}

	if (len >= sizeof(exepath))
	{
		LogWithErrorCode(ERROR_INSUFFICIENT_BUFFER, "%s: error: GetModuleFileNameA failed.", __FUNCTION__);
		return false;
	}

	if (_splitpath_s(exepath, exedrivepath, sizeof(exedrivepath), exedirpath, sizeof(exedirpath), NULL, 0, NULL, 0) != 0)
	{
		Log("%s: error: _splitpath_s failed.", __FUNCTION__);
		return false;
	}

	if (myasprintf(&logpath, "%s%sthd3d8mitigationlog.txt", exedrivepath, exedirpath) < 0)
	{
		Log("%s: error: myasprintf failed.", __FUNCTION__);
		return false;
	}

	g_LogFile = CreateFileA(logpath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	err = GetLastError();
	free(logpath);
	if (g_LogFile == INVALID_HANDLE_VALUE)
	{
		LogWithErrorCode(GetLastError(), "%s: error: CreateFileA failed.", __FUNCTION__);
		return false;
	}

	return true;
}

bool cs_LogInit(void)
{
	static enum InitStatus g_initstatus = INITSTATUS_UNINITED;

	bool ret;

	switch (g_initstatus)
	{
	case INITSTATUS_SUCCEEDED:
		return true;
	case INITSTATUS_FAILED:
		return false;
	case INITSTATUS_UNINITED:
		break;
	default:
		Log("%s: error: unreachable.", __FUNCTION__);
		return false;
	}

	ret = cs_LogInitImpl();
	g_initstatus = ret ? INITSTATUS_SUCCEEDED : INITSTATUS_FAILED;
	return ret;
}

bool LogInit(void)
{
	bool ret;

	EnterCriticalSection(&g_CS);
	ret = cs_LogInit();
	LeaveCriticalSection(&g_CS);
	return ret;
}

bool cs_InitImpl(void)
{
	if (!InitD3D8Handle(&g_D3D8Handle))
	{
		Log("%s: error: InitD3D8Handle failed.", __FUNCTION__);
		return false;
	}

	if (!InitVanillaDirect3DCreate8(g_D3D8Handle, &g_VanillaDirect3DCreate8))
	{
		Log("%s: error: InitVanillaDirect3DCreate8 failed.", __FUNCTION__);
		return false;
	}

	g_D3D8ExDataTable = IDirect3D8ExtraDataTableNew();
	g_D3DDev8ExDataTable = IDirect3DDevice8ExtraDataTableNew();

	return true;
}

bool cs_Init(void)
{
	static enum InitStatus g_initstatus = INITSTATUS_UNINITED;

	bool ret;

	switch (g_initstatus)
	{
	case INITSTATUS_SUCCEEDED:
		return true;
	case INITSTATUS_FAILED:
		return false;
	case INITSTATUS_UNINITED:
		break;
	default:
		Log("%s: error: unreachable.", __FUNCTION__);
		return false;
	}

	ret = cs_InitImpl();
	g_initstatus = ret ? INITSTATUS_SUCCEEDED : INITSTATUS_FAILED;
	return ret;
}

bool Init(void)
{
	bool ret;

	EnterCriticalSection(&g_CS);
	ret = cs_Init();
	LeaveCriticalSection(&g_CS);
	return ret;
}

IDirect3D8* WINAPI ModDirect3DCreate8(UINT SDKVersion)
{
	IDirect3D8* ret;

	// 実質的なエントリーポイントなので、最初は初期化処理を行う

	if (!LogInit())
	{
		OutputDebugStringA(LOG_PREFIX __FUNCTION__ ": error: log initialization failed.\n");
		return NULL;
	}
	// これ以降はログがファイルにも記録される

	Log("%s: Version: %s", __FUNCTION__, PROGRAM_VERSION);

	if (!Init())
	{
		Log("%s: error: initialization failed.", __FUNCTION__);
		return NULL;
	}

	// 本体

	EnterCriticalSection(&g_CS);
	ret = cs_ModDirect3DCreate8Impl(SDKVersion);
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
		OutputDebugStringA(LOG_PREFIX __FUNCTION__ ": Version: " PROGRAM_VERSION "\n");
		OutputDebugStringA(LOG_PREFIX __FUNCTION__ ": Attaching to the process: begin\n");
		InitializeCriticalSection(&g_CS);
		OutputDebugStringA(LOG_PREFIX __FUNCTION__ ": Attaching to the process: succeeded\n");
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

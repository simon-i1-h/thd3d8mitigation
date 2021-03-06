﻿#include "thd3d8mitigation.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define CINTERFACE
#include <d3d8.h>

#include <mmsystem.h>

#include <math.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

enum InitStatus {
	INITSTATUS_UNINITED,
	INITSTATUS_SUCCEEDED,
	INITSTATUS_FAILED
};

CRITICAL_SECTION g_CS;

const char* const ConfigWaitForNameTable[3] = {
	[CONFIG_WAITFOR_VSYNC] = "vsync",
	[CONFIG_WAITFOR_NORMAL] = "normal",
	[CONFIG_WAITFOR_AUTO] = "auto"
};
static enum ConfigWaitFor g_ConfigFileWaitFor;

static HMODULE g_D3D8Handle;
static Direct3DCreate8_t g_VanillaDirect3DCreate8;
static struct IDirect3D8ExtraDataTable* g_D3D8ExDataTable;
static struct IDirect3DDevice8ExtraDataTable* g_D3DDev8ExDataTable;

BOOL NeedPresentMitigation(D3DPRESENT_PARAMETERS* pp)
{
	return !pp->Windowed && (pp->FullScreen_PresentationInterval == D3DPRESENT_INTERVAL_DEFAULT ||
								pp->FullScreen_PresentationInterval == D3DPRESENT_INTERVAL_ONE ||
								pp->FullScreen_PresentationInterval == D3DPRESENT_INTERVAL_TWO ||
								pp->FullScreen_PresentationInterval == D3DPRESENT_INTERVAL_THREE ||
								pp->FullScreen_PresentationInterval == D3DPRESENT_INTERVAL_FOUR);
}

HRESULT ModIDirect3DDevice8PresentWithGetRasterStatus(IDirect3DDevice8* me, struct IDirect3DDevice8ExtraData* me_exdata,
	CONST RECT* pSourceRect, CONST RECT* pDestRect,
	HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion)
{
	D3DRASTER_STATUS stat;
	HRESULT ret;

	do
	{
		if (FAILED(me->lpVtbl->GetRasterStatus(me, &stat)))
		{
			Log("%s: error: IDirect3DDevice8::GetRasterStatus failed.", __FUNCTION__);
			return E_FAIL;
		}
		SleepEx(0, TRUE);
	} while (stat.InVBlank);

	ret = me_exdata->VanillaPresent(me, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);

	do
	{
		if (FAILED(me->lpVtbl->GetRasterStatus(me, &stat)))
		{
			Log("%s: error: IDirect3DDevice8::GetRasterStatus failed.", __FUNCTION__);
			return E_FAIL;
		}
		SleepEx(0, TRUE);
	} while (!stat.InVBlank);

	return ret;
}

HRESULT ModIDirect3DDevice8PresentVanilla(IDirect3DDevice8* me, struct IDirect3DDevice8ExtraData* me_exdata,
	CONST RECT* pSourceRect, CONST RECT* pDestRect,
	HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion)
{
	return me_exdata->VanillaPresent(me, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
}

/*
 * Windows 10以降、IDirect3DDevice8::PresentはD3DPRESENT_PARAMETERS.FullScreen_PresentationIntervalに適切な値を設定しても
 * 待機しなくなった。これにより紅魔郷のゲームスピードが高速化してしまうようになった。そのため、必要であれば独自の方法で
 * 待機する。
 */
HRESULT __stdcall ModIDirect3DDevice8Present(IDirect3DDevice8* me, CONST RECT* pSourceRect, CONST RECT* pDestRect,
	HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion)
{
	struct IDirect3DDevice8ExtraData* me_exdata;

	/* load from critical section */
	EnterCriticalSection(&g_CS);
	me_exdata = IDirect3DDevice8ExtraDataTableGet(g_D3DDev8ExDataTable, me);
	LeaveCriticalSection(&g_CS);

	if (me_exdata == NULL)
	{
		Log("%s: bug, error: IDirect3DDevice8ExtraDataTableGet failed.", __FUNCTION__);
		return E_FAIL;
	}

	return me_exdata->ModPresent(me, me_exdata, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
}

ULONG cs_ModIDirect3DDevice8ReleaseImpl(IDirect3DDevice8* me)
{
	ULONG ret;
	struct IDirect3DDevice8ExtraData* me_exdata;

	if ((me_exdata = IDirect3DDevice8ExtraDataTableGet(g_D3DDev8ExDataTable, me)) == NULL)
	{
		Log("%s: bug, warning: IDirect3DDevice8ExtraDataTableGet failed. Resource is leaking.", __FUNCTION__);
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

BOOL tm_MeasureNormalFrameSecond(double* ret_frame_second, IDirect3DDevice8* me, struct IDirect3DDevice8ExtraData* me_exdata)
{
	double absolute_deviation_threshold = 3.0;

	DWORD starttime, endtime, sum;
	DWORD frame_second_list[10];
	double average, absolute_deviation, absolute_deviation_max;
	int i, j;

	for (i = 0; i < ARRAY_SIZE(frame_second_list); i++)
	{
		starttime = timeGetTime();
		if (FAILED(me_exdata->VanillaPresent(me, NULL, NULL, NULL, NULL)))
			return FALSE;
		endtime = timeGetTime();
		frame_second_list[i] = endtime - starttime;
	}

	for (i = 0; i < ARRAY_SIZE(frame_second_list) * 10; i++)
	{
		absolute_deviation_max = 0.0;
		sum = 0;
		for (j = 0; j < ARRAY_SIZE(frame_second_list); j++)
			sum += frame_second_list[j];
		average = (double)sum / (double)ARRAY_SIZE(frame_second_list);
		for (j = 0; j < ARRAY_SIZE(frame_second_list); j++)
		{
			absolute_deviation = fabs(average - frame_second_list[j]);
			if (absolute_deviation_max < absolute_deviation)
				absolute_deviation_max = absolute_deviation;
		}
		/* 入力が安定したら測定を終了する。 */
		if (absolute_deviation_max < absolute_deviation_threshold)
			break;

		starttime = timeGetTime();
		if (FAILED(me_exdata->VanillaPresent(me, NULL, NULL, NULL, NULL)))
			return FALSE;
		endtime = timeGetTime();

		frame_second_list[i % ARRAY_SIZE(frame_second_list)] = endtime - starttime;
	}

	*ret_frame_second = average;
	return TRUE;
}

BOOL tm_DetectProperConfig(IDirect3DDevice8* me, struct IDirect3DDevice8ExtraData* me_exdata,
	enum ConfigWaitFor* config_wait_for)
{
	double frame_second = 0.0;
	double frame_second_threshold = 15.0; /* 66.6666... FPS */

	Log("%s: Detecting proper config...", __FUNCTION__);

	if (!tm_MeasureNormalFrameSecond(&frame_second, me, me_exdata))
	{
		Log("%s: MeasureNormalFrameSecond failed.", __FUNCTION__);
		return FALSE;
	}

	if (frame_second > frame_second_threshold)
	{
		*config_wait_for = CONFIG_WAITFOR_NORMAL;
		Log("%s: Detected proper config: normal", __FUNCTION__);
		return TRUE;
	}

	*config_wait_for = CONFIG_WAITFOR_VSYNC;
	Log("%s: Detected proper config: vsync", __FUNCTION__);
	return TRUE;
}

BOOL DetectProperConfig(IDirect3DDevice8* me, struct IDirect3DDevice8ExtraData* me_exdata,
	enum ConfigWaitFor* config_wait_for)
{
	BOOL ret;

	timeBeginPeriod(1);
	ret = tm_DetectProperConfig(me, me_exdata, config_wait_for);
	timeEndPeriod(1);
	return ret;
}

BOOL InitIDirect3DDevice8ExtraDataModPresent(IDirect3DDevice8* me, struct IDirect3DDevice8ExtraData* me_exdata)
{
	enum ConfigWaitFor wait_for;

	/* load from critical section */
	EnterCriticalSection(&g_CS);
	wait_for = g_ConfigFileWaitFor;
	LeaveCriticalSection(&g_CS);

	if (NeedPresentMitigation(&me_exdata->pp))
	{
		if (wait_for == CONFIG_WAITFOR_AUTO)
		{
			if (!DetectProperConfig(me, me_exdata, &wait_for))
			{
				Log("%s: error: DetectProperConfig failed.", __FUNCTION__);
				return FALSE;
			}
		}

		switch (wait_for)
		{
		case CONFIG_WAITFOR_NORMAL:
			me_exdata->ModPresent = ModIDirect3DDevice8PresentVanilla;
			break;
		case CONFIG_WAITFOR_VSYNC:
			me_exdata->ModPresent = ModIDirect3DDevice8PresentWithGetRasterStatus;
			break;
		default:
			Log("%s: bug, error: unreachable.", __FUNCTION__);
			return FALSE;
		}
	}
	else
		me_exdata->ModPresent = ModIDirect3DDevice8PresentVanilla;

	return TRUE;
}

HRESULT cs_ModIDirect3DDevice8ResetImpl(IDirect3DDevice8* me, D3DPRESENT_PARAMETERS* pPresentationParameters)
{
	HRESULT ret;
	struct IDirect3DDevice8ExtraData* me_exdata;

	if ((me_exdata = IDirect3DDevice8ExtraDataTableGet(g_D3DDev8ExDataTable, me)) == NULL)
	{
		Log("%s: bug, error: IDirect3DDevice8ExtraDataTableGet failed.", __FUNCTION__);
		return E_FAIL;
	}

	if (SUCCEEDED(ret = me_exdata->VanillaReset(me, pPresentationParameters)))
	{
		me_exdata->pp = *pPresentationParameters;
		if (!InitIDirect3DDevice8ExtraDataModPresent(me, me_exdata))
		{
			Log("%s: error: InitIDirect3DDevice8ExtraDataModPresent failed.", __FUNCTION__);
			return E_FAIL;
		}
	}

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

HRESULT cs_ModIDirect3D8CreateDeviceImpl(IDirect3D8* me, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow,
	DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters,
	IDirect3DDevice8** ppReturnedDeviceInterface)
{
	HRESULT ret;
	struct IDirect3D8ExtraData* me_exdata;
	DWORD orig_protect;
	IDirect3DDevice8* d3ddev8;
	IDirect3DDevice8Vtbl* vtbl;
	struct IDirect3DDevice8ExtraData d3ddev8_exdata;

	if ((me_exdata = IDirect3D8ExtraDataTableGet(g_D3D8ExDataTable, me)) == NULL)
	{
		Log("%s: bug, error: IDirect3D8ExtraDataTableGet failed.", __FUNCTION__);
		return E_FAIL;
	}

	if (FAILED(ret = me_exdata->VanillaCreateDevice(me, Adapter, DeviceType, hFocusWindow, BehaviorFlags,
				   pPresentationParameters, ppReturnedDeviceInterface)))
		return ret;

	/*
	 * hook IDirect3DDevice8::Present, IDirect3DDevice8::Release (inherit from IUnknown::Release), and
	 * IDirect3DDevice8::Reset
	 */

	d3ddev8 = *ppReturnedDeviceInterface;
	vtbl = d3ddev8->lpVtbl;

	if (!VirtualProtect(vtbl, sizeof(*vtbl), PAGE_READWRITE, &orig_protect))
	{
		LogWithErrorCode(GetLastError(), "%s: error: VirtualProtect (PAGE_READWRITE) failed", __FUNCTION__);
		return E_FAIL;
	}

	d3ddev8_exdata = (struct IDirect3DDevice8ExtraData){ .VanillaPresent = vtbl->Present,
		.VanillaRelease = vtbl->Release,
		.VanillaReset = vtbl->Reset,
		.pp = *pPresentationParameters };
	if (!InitIDirect3DDevice8ExtraDataModPresent(d3ddev8, &d3ddev8_exdata))
	{
		Log("%s: error: InitIDirect3DDevice8ExtraDataModPresent failed.", __FUNCTION__);
		return E_FAIL;
	}

	if (!IDirect3DDevice8ExtraDataTableInsert(g_D3DDev8ExDataTable, d3ddev8, d3ddev8_exdata))
	{
		Log("%s: bug, error: IDirect3DDevice8ExtraDataTableInsert failed.", __FUNCTION__);
		return E_FAIL;
	}

	vtbl->Present = ModIDirect3DDevice8Present;
	vtbl->Release = ModIDirect3DDevice8Release;
	vtbl->Reset = ModIDirect3DDevice8Reset;

	/* best effort */
	if (!VirtualProtect(vtbl, sizeof(*vtbl), orig_protect, &orig_protect))
		LogWithErrorCode(GetLastError(), "%s: warning: VirtualProtect (original protect) failed.", __FUNCTION__);

	return ret;
}

HRESULT __stdcall ModIDirect3D8CreateDevice(IDirect3D8* me, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow,
	DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters,
	IDirect3DDevice8** ppReturnedDeviceInterface)
{
	HRESULT ret;

	EnterCriticalSection(&g_CS);
	ret = cs_ModIDirect3D8CreateDeviceImpl(me, Adapter, DeviceType, hFocusWindow, BehaviorFlags,
		pPresentationParameters, ppReturnedDeviceInterface);
	LeaveCriticalSection(&g_CS);
	return ret;
}

ULONG cs_ModIDirect3D8ReleaseImpl(IDirect3D8* me)
{
	ULONG ret;
	struct IDirect3D8ExtraData* me_exdata;

	if ((me_exdata = IDirect3D8ExtraDataTableGet(g_D3D8ExDataTable, me)) == NULL)
	{
		Log("%s: bug, warning: IDirect3D8ExtraDataTableGet failed. Resource is leaking.", __FUNCTION__);
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
		return NULL;

	/* hook IDirect3D8::CreateDevice and IDirect3D8::Release (inherit from IUnknown::Release) */

	vtbl = ret->lpVtbl;

	if (!VirtualProtect(vtbl, sizeof(*vtbl), PAGE_READWRITE, &orig_protect))
	{
		LogWithErrorCode(GetLastError(), "%s: error: VirtualProtect (PAGE_READWRITE) failed.", __FUNCTION__);
		return NULL;
	}

	d3d8_exdata =
		(struct IDirect3D8ExtraData){ .VanillaCreateDevice = vtbl->CreateDevice, .VanillaRelease = vtbl->Release };
	if (!IDirect3D8ExtraDataTableInsert(g_D3D8ExDataTable, ret, d3d8_exdata))
	{
		Log("%s: bug, error: IDirect3D8ExtraDataTableInsert failed.", __FUNCTION__);
		return NULL;
	}

	vtbl->CreateDevice = ModIDirect3D8CreateDevice;
	vtbl->Release = ModIDirect3D8Release;

	/* best effort */
	if (!VirtualProtect(vtbl, sizeof(*vtbl), orig_protect, &orig_protect))
		LogWithErrorCode(GetLastError(), "%s: warning: VirtualProtect (original protect) failed.", __FUNCTION__);

	return ret;
}

int AllocateDataDirPath(char** strp)
{
	char exepath[MAX_PATH];
	char exedrivepath[_MAX_DRIVE];
	char exedirpath[_MAX_DIR];
	DWORD len;

	if ((len = GetModuleFileNameA(NULL, exepath, sizeof(exepath))) == 0)
	{
		LogWithErrorCode(GetLastError(), "%s: error: GetModuleFileNameA failed.", __FUNCTION__);
		return -1;
	}

	if (len >= sizeof(exepath))
	{
		LogWithErrorCode(ERROR_INSUFFICIENT_BUFFER, "%s: error: GetModuleFileNameA failed.", __FUNCTION__);
		return -1;
	}

	if (_splitpath_s(exepath, exedrivepath, sizeof(exedrivepath), exedirpath, sizeof(exedirpath), NULL, 0, NULL, 0) !=
		0)
	{
		Log("%s: error: _splitpath_s failed.", __FUNCTION__);
		return -1;
	}

	return myasprintf(strp, "%s%s", exedrivepath, exedirpath);
}

int AllocateDataFilePath(char** strp, char* filename)
{
	char* datadirpath;
	int ret;

	if (AllocateDataDirPath(&datadirpath) < 0)
	{
		Log("%s: error: AllocateDataDirPath failed.", __FUNCTION__);
		return -1;
	}

	ret = myasprintf(strp, "%s%s", datadirpath, filename);
	free(datadirpath);
	return ret;
}

BOOL ExistsFile(char* path)
{
	return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
}

BOOL cs_InitConfig(void)
{
	const char* section_present = "presentation";
	const char* key_wait_for = "wait_for";

	enum ConfigWaitFor config_wait_for_default = CONFIG_WAITFOR_AUTO;
	const char* value_wait_for_default = ConfigWaitForNameTable[CONFIG_WAITFOR_AUTO];

	char* path = NULL;
	char buf[32];
	BOOL ret = FALSE;

	if (AllocateDataFilePath(&path, "thd3d8mitigationcfg.ini") < 0)
	{
		Log("%s: error: AllocateDataFilePath failed.", __FUNCTION__);
		goto cleanup;
	}

	if (!ExistsFile(path))
		if (WritePrivateProfileStringA(section_present, key_wait_for, value_wait_for_default, path) == 0)
		{
			LogWithErrorCode(GetLastError(), "%s: error: WritePrivateProfileStringA (%s) failed.", __FUNCTION__,
				key_wait_for);
			goto cleanup;
		}

	g_ConfigFileWaitFor = config_wait_for_default;
	if (GetPrivateProfileStringA(section_present, key_wait_for, value_wait_for_default, buf, sizeof(buf), path) >=
		sizeof(buf) - 1)
		/* no op */;
	else if (strcmp(buf, ConfigWaitForNameTable[CONFIG_WAITFOR_VSYNC]) == 0)
		g_ConfigFileWaitFor = CONFIG_WAITFOR_VSYNC;
	else if (strcmp(buf, ConfigWaitForNameTable[CONFIG_WAITFOR_NORMAL]) == 0)
		g_ConfigFileWaitFor = CONFIG_WAITFOR_NORMAL;
	else if (strcmp(buf, ConfigWaitForNameTable[CONFIG_WAITFOR_AUTO]) == 0)
		g_ConfigFileWaitFor = CONFIG_WAITFOR_AUTO;
	else
		/* no op */;
	Log("%s: config file %s: %s", __FUNCTION__, key_wait_for, ConfigWaitForNameTable[g_ConfigFileWaitFor]);

	ret = TRUE;
cleanup:
	free(path);
	return ret;
}

BOOL InitD3D8Handle(HMODULE* ret)
{
	char sysdirpath[MAX_PATH];
	char* sysdllpath;
	unsigned int len;
	DWORD err;

	if ((len = GetSystemDirectoryA(sysdirpath, sizeof(sysdirpath))) == 0)
	{
		LogWithErrorCode(GetLastError(), "%s: error: GetSystemDirectoryA failed.", __FUNCTION__);
		return FALSE;
	}

	if (len >= sizeof(sysdirpath))
	{
		LogWithErrorCode(ERROR_INSUFFICIENT_BUFFER, "%s: error: GetSystemDirectoryA failed.", __FUNCTION__);
		return FALSE;
	}

	if (myasprintf(&sysdllpath, "%s\\d3d8.dll", sysdirpath) < 0)
	{
		Log("%s: error: myasprintf failed.", __FUNCTION__);
		return FALSE;
	}

	*ret = LoadLibraryA(sysdllpath);
	err = GetLastError();
	free(sysdllpath);
	if (*ret == NULL)
	{
		LogWithErrorCode(err, "%s: error: LoadLibraryA failed.", __FUNCTION__);
		return FALSE;
	}

	return TRUE;
}

BOOL InitVanillaDirect3DCreate8(HMODULE D3D8Handle, Direct3DCreate8_t* ret)
{
	if ((*ret = (Direct3DCreate8_t)GetProcAddress(D3D8Handle, "Direct3DCreate8")) == NULL)
	{
		LogWithErrorCode(GetLastError(), "%s: error: GetProcAddress failed.", __FUNCTION__);
		return FALSE;
	}

	return TRUE;
}

BOOL cs_LogInit(void)
{
	char* logpath;
	DWORD err;

	if (AllocateDataFilePath(&logpath, "thd3d8mitigationlog.txt") < 0)
	{
		Log("%s: error: AllocateDataFilePath failed.", __FUNCTION__);
		return FALSE;
	}

	g_LogFile = CreateFileA(logpath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	err = GetLastError();
	free(logpath);
	if (g_LogFile == INVALID_HANDLE_VALUE)
	{
		LogWithErrorCode(err, "%s: error: CreateFileA failed.", __FUNCTION__);
		return FALSE;
	}

	return TRUE;
}

BOOL cs_InitImpl(void)
{
	if (!cs_LogInit())
	{
		Log("%s: error: log initialization failed.", __FUNCTION__);
		return FALSE;
	}
	/* これ以降はログがファイルにも記録される */

	Log("%s: Version: %s", __FUNCTION__, PROGRAM_VERSION);

	if (!InitD3D8Handle(&g_D3D8Handle))
	{
		Log("%s: error: InitD3D8Handle failed.", __FUNCTION__);
		return FALSE;
	}

	if (!InitVanillaDirect3DCreate8(g_D3D8Handle, &g_VanillaDirect3DCreate8))
	{
		Log("%s: error: InitVanillaDirect3DCreate8 failed.", __FUNCTION__);
		return FALSE;
	}

	g_D3D8ExDataTable = IDirect3D8ExtraDataTableNew();
	g_D3DDev8ExDataTable = IDirect3DDevice8ExtraDataTableNew();

	if (!cs_InitConfig())
	{
		Log("%s: error: cs_InitConfig failed.", __FUNCTION__);
		return FALSE;
	}

	return TRUE;
}

BOOL cs_Init(void)
{
	static enum InitStatus g_initstatus = INITSTATUS_UNINITED;

	BOOL ret;

	switch (g_initstatus)
	{
	case INITSTATUS_SUCCEEDED:
		return TRUE;
	case INITSTATUS_FAILED:
		return FALSE;
	case INITSTATUS_UNINITED:
		break;
	default:
		Log("%s: bug, error: unreachable.", __FUNCTION__);
		return FALSE;
	}

	ret = cs_InitImpl();
	g_initstatus = ret ? INITSTATUS_SUCCEEDED : INITSTATUS_FAILED;
	return ret;
}

BOOL Init(void)
{
	BOOL ret;

	EnterCriticalSection(&g_CS);
	ret = cs_Init();
	LeaveCriticalSection(&g_CS);
	return ret;
}

IDirect3D8* WINAPI ModDirect3DCreate8(UINT SDKVersion)
{
	IDirect3D8* ret;

	/* 実質的なエントリーポイントなので、最初は初期化処理を行う */
	if (!Init())
	{
		Log("%s: error: initialization failed.", __FUNCTION__);
		return NULL;
	}

	EnterCriticalSection(&g_CS);
	ret = cs_ModDirect3DCreate8Impl(SDKVersion);
	LeaveCriticalSection(&g_CS);

	return ret;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		/* https://docs.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-best-practices#general-best-practices */
		OutputDebugStringA(LOG_PREFIX __FUNCTION__ ": Version: " PROGRAM_VERSION "\r\n");
		OutputDebugStringA(LOG_PREFIX __FUNCTION__ ": Attaching to the process: begin\r\n");
		InitializeCriticalSection(&g_CS);
		OutputDebugStringA(LOG_PREFIX __FUNCTION__ ": Attaching to the process: succeeded\r\n");
		break;
	case DLL_PROCESS_DETACH:
		/* https://docs.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-best-practices#best-practices-for-synchronization */
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		break;
	}

	return TRUE;
}

#include "thd3d8mitigation.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define CINTERFACE
#include <d3d8.h>

#include <mmsystem.h>

#include <math.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

// Naming convention:
//   cs_*: Critical section
//   g_*: Global variable
//   *_t: Type identifier
//   Mod*: Modified
//   V*: Function with va_list
//   tm_*: timeBeginPeriod and timeEndPeriod required

// XXX TODO review
// XXX TODO コード整形

enum InitStatus {
	INITSTATUS_UNINITED,
	INITSTATUS_SUCCEEDED,
	INITSTATUS_FAILED
};

CRITICAL_SECTION g_CS;

static enum ConfigWaitFor g_ConfigFileWaitFor;

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
		SleepEx(0, TRUE);
	} while (stat.InVBlank);

	ret = me_exdata->VanillaPresent(me, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);

	do {
		if (FAILED(me->lpVtbl->GetRasterStatus(me, &stat)))
		{
			Log("%s: error: IDirect3DDevice8::GetRasterStatus failed.", __FUNCTION__);
			return E_FAIL;
		}
		SleepEx(0, TRUE);
	} while (!stat.InVBlank);

	return ret;
}

HRESULT ModIDirect3DDevice8PresentWithQueryPerformanceCounter(IDirect3DDevice8* me, struct IDirect3DDevice8ExtraData* me_exdata, CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion)
{
	D3DRASTER_STATUS stat;
	HRESULT ret;
	LARGE_INTEGER prevframe_count, nextframe_count, curr_count, curr_count_on_second, count;
	int curr_frame_on_second;

	do {
		if (FAILED(me->lpVtbl->GetRasterStatus(me, &stat)))
		{
			Log("%s: error: IDirect3DDevice8::GetRasterStatus failed.", __FUNCTION__);
			return E_FAIL;
		}
		SleepEx(0, TRUE);
	} while (stat.InVBlank);

	ret = me_exdata->VanillaPresent(me, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);

	QueryPerformanceCounter(&curr_count);
	curr_count_on_second.QuadPart = curr_count.QuadPart % me_exdata->PerformanceFrequency.QuadPart;
	prevframe_count.QuadPart = curr_count.QuadPart - curr_count_on_second.QuadPart;
	curr_frame_on_second = (int)(curr_count_on_second.QuadPart / me_exdata->CountPerFrame.QuadPart);
	nextframe_count.QuadPart = prevframe_count.QuadPart + me_exdata->CountPerFrame.QuadPart * curr_frame_on_second + me_exdata->CountPerFrame.QuadPart;
	if (curr_frame_on_second == 0)
		nextframe_count.QuadPart += me_exdata->RemainderPerSecond;

	do {
		QueryPerformanceCounter(&count);
		SleepEx(0, TRUE);
	} while (count.QuadPart < nextframe_count.QuadPart);

	return ret;
}

BOOL NeedPresentMitigation(IDirect3DDevice8* me, struct IDirect3DDevice8ExtraData* me_exdata)
{
	return !me_exdata->pp.Windowed &&
		(me_exdata->pp.FullScreen_PresentationInterval == D3DPRESENT_INTERVAL_DEFAULT ||
			me_exdata->pp.FullScreen_PresentationInterval == D3DPRESENT_INTERVAL_ONE ||
			me_exdata->pp.FullScreen_PresentationInterval == D3DPRESENT_INTERVAL_TWO ||
			me_exdata->pp.FullScreen_PresentationInterval == D3DPRESENT_INTERVAL_THREE ||
			me_exdata->pp.FullScreen_PresentationInterval == D3DPRESENT_INTERVAL_FOUR);
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
		Log("%s: bug, error: IDirect3DDevice8ExtraDataTableGet failed.", __FUNCTION__);
		return E_FAIL;
	}

	if (!NeedPresentMitigation(me, me_exdata))
		return me_exdata->VanillaPresent(me, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
	else if (me_exdata->ConfigWaitFor == CONFIG_WAITFOR_VSYNC)
		return ModIDirect3DDevice8PresentWithGetRasterStatus(me, me_exdata, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
	else if (me_exdata->ConfigWaitFor == CONFIG_WAITFOR_TIMER60)
		return ModIDirect3DDevice8PresentWithQueryPerformanceCounter(me, me_exdata, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
	else if (me_exdata->ConfigWaitFor == CONFIG_WAITFOR_NORMAL)
		return me_exdata->VanillaPresent(me, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);

	Log("%s: bug, error: unreachable.", __FUNCTION__);
	return E_FAIL;
}

ULONG cs_ModIDirect3DDevice8ReleaseImpl(IDirect3DDevice8* me)
{
	ULONG ret;
	// me_exdataはmeに1対1で紐づく拡張プロパティと考えられるので、meのメソッド内ではデータ競合や競合状態について考えなくてよい。
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

HRESULT cs_ModIDirect3DDevice8ResetImpl(IDirect3DDevice8* me, D3DPRESENT_PARAMETERS* pPresentationParameters)
{
	HRESULT ret;
	// me_exdataはmeに1対1で紐づく拡張プロパティと考えられるので、meのメソッド内ではデータ競合や競合状態について考えなくてよい。
	struct IDirect3DDevice8ExtraData* me_exdata;

	if ((me_exdata = IDirect3DDevice8ExtraDataTableGet(g_D3DDev8ExDataTable, me)) == NULL)
	{
		Log("%s: bug, error: IDirect3DDevice8ExtraDataTableGet failed.", __FUNCTION__);
		return E_FAIL;
	}

	if (SUCCEEDED(ret = me_exdata->VanillaReset(me, pPresentationParameters)))
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

struct MeasureFrameRateCallBackArgs {
	IDirect3DDevice8* me;
	struct IDirect3DDevice8ExtraData* me_exdata;
};

typedef BOOL(*MeasureFrameRateCallBack_t)(struct MeasureFrameRateCallBackArgs args);

BOOL MeasureFrameRate(double* ret_frame_second, MeasureFrameRateCallBack_t callback, struct MeasureFrameRateCallBackArgs args)
{
	double absolute_deviation_threshold = 3.0;

	DWORD starttime;
	DWORD endtime;
	DWORD frame_second_list[10];
	DWORD sum;
	double average, absolute_deviation, absolute_deviation_max;
	int i, j;

	for (i = 0; i < ARRAY_SIZE(frame_second_list); i++)
	{
		starttime = timeGetTime();
		if (!callback(args))
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
		if (absolute_deviation_max < absolute_deviation_threshold)
			break;

		starttime = timeGetTime();
		if (!callback(args))
			return FALSE;
		endtime = timeGetTime();

		frame_second_list[i % ARRAY_SIZE(frame_second_list)] = endtime - starttime;
	}

	*ret_frame_second = average;
	return TRUE;
}

BOOL MeasureFrameRateCallBackNormal(struct MeasureFrameRateCallBackArgs args)
{
	return SUCCEEDED(args.me_exdata->VanillaPresent(args.me, NULL, NULL, NULL, NULL));
}

BOOL MeasureFrameRateCallBackVsync(struct MeasureFrameRateCallBackArgs args)
{
	D3DRASTER_STATUS stat;

	do {
		if (FAILED(args.me->lpVtbl->GetRasterStatus(args.me, &stat)))
		{
			Log("%s: error: IDirect3DDevice8::GetRasterStatus failed.", __FUNCTION__);
			return FALSE;
		}
		SleepEx(0, TRUE);
	} while (stat.InVBlank);

	do {
		if (FAILED(args.me->lpVtbl->GetRasterStatus(args.me, &stat)))
		{
			Log("%s: error: IDirect3DDevice8::GetRasterStatus failed.", __FUNCTION__);
			return FALSE;
		}
		SleepEx(0, TRUE);
	} while (!stat.InVBlank);

	return TRUE;
}

BOOL tm_DetectProperConfig(IDirect3DDevice8* me, struct IDirect3DDevice8ExtraData* me_exdata, enum ConfigWaitFor* config_wait_for)
{
	double frame_second = 0.0;
	double frame_second_threshold = 15.0; /* 66.6666... FPS */

	Log("%s: Detecting proper configure...", __FUNCTION__);

	if (!MeasureFrameRate(&frame_second, MeasureFrameRateCallBackNormal, (struct MeasureFrameRateCallBackArgs) { .me = me, .me_exdata = me_exdata }))
		return FALSE;

	if (frame_second > frame_second_threshold)
	{
		*config_wait_for = CONFIG_WAITFOR_NORMAL;
		Log("%s: wait_for config: normal", __FUNCTION__);
		return TRUE;
	}

	if (!MeasureFrameRate(&frame_second, MeasureFrameRateCallBackVsync, (struct MeasureFrameRateCallBackArgs) { .me = me, .me_exdata = me_exdata }))
		return FALSE;

	if (frame_second > frame_second_threshold)
	{
		*config_wait_for = CONFIG_WAITFOR_VSYNC;
		Log("%s: wait_for config: vsync", __FUNCTION__);
		return TRUE;
	}

	*config_wait_for = CONFIG_WAITFOR_TIMER60;
	Log("%s: wait_for config: timer60", __FUNCTION__);
	return TRUE;
}

BOOL DetectProperConfig(IDirect3DDevice8* me, struct IDirect3DDevice8ExtraData* me_exdata, enum ConfigWaitFor* config_wait_for)
{
	BOOL ret;

	timeBeginPeriod(1);
	ret = tm_DetectProperConfig(me, me_exdata, config_wait_for);
	timeEndPeriod(1);

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
		Log("%s: bug, error: IDirect3D8ExtraDataTableGet failed.", __FUNCTION__);
		return E_FAIL;
	}

	if (FAILED(ret = me_exdata->VanillaCreateDevice(me, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, ppReturnedDeviceInterface)))
		return ret;

	// hook IDirect3DDevice8::Present, IDirect3DDevice8::Release (inherit from IUnknown::Release), and IDirect3DDevice8::Reset

	d3ddev8 = *ppReturnedDeviceInterface;
	vtbl = d3ddev8->lpVtbl;

	if (!VirtualProtect(vtbl, sizeof(*vtbl), PAGE_READWRITE, &orig_protect))
	{
		LogWithErrorCode(GetLastError(), "%s: error: VirtualProtect (PAGE_READWRITE) failed", __FUNCTION__);
		return E_FAIL;
	}

	d3ddev8_exdata = (struct IDirect3DDevice8ExtraData){
		.VanillaPresent = vtbl->Present,
		.VanillaRelease = vtbl->Release,
		.VanillaReset = vtbl->Reset,
		.pp = *pPresentationParameters,
		.FrameRate = FRAME_RATE
	};
	QueryPerformanceFrequency(&d3ddev8_exdata.PerformanceFrequency);
	d3ddev8_exdata.RemainderPerSecond = d3ddev8_exdata.PerformanceFrequency.QuadPart % d3ddev8_exdata.FrameRate;
	d3ddev8_exdata.CountPerFrame.QuadPart = d3ddev8_exdata.PerformanceFrequency.QuadPart / d3ddev8_exdata.FrameRate;
	if (g_ConfigFileWaitFor == CONFIG_WAITFOR_AUTO && NeedPresentMitigation(d3ddev8, &d3ddev8_exdata))
	{
		if (!DetectProperConfig(d3ddev8, &d3ddev8_exdata, &d3ddev8_exdata.ConfigWaitFor))
		{
			Log("%s: error: DetectProperConfig failed.", __FUNCTION__);
			return E_FAIL;
		}
	}
	else
		d3ddev8_exdata.ConfigWaitFor = g_ConfigFileWaitFor;

	if (!IDirect3DDevice8ExtraDataTableInsert(g_D3DDev8ExDataTable, d3ddev8, d3ddev8_exdata))
	{
		Log("%s: bug, error: IDirect3DDevice8ExtraDataTableInsert failed.", __FUNCTION__);
		return E_FAIL;
	}

	vtbl->Present = ModIDirect3DDevice8Present;
	vtbl->Release = ModIDirect3DDevice8Release;
	vtbl->Reset = ModIDirect3DDevice8Reset;

	// best effort
	if (!VirtualProtect(vtbl, sizeof(*vtbl), orig_protect, &orig_protect))
		LogWithErrorCode(GetLastError(), "%s: warning: VirtualProtect (original protect) failed.", __FUNCTION__);

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

	// hook IDirect3D8::CreateDevice and IDirect3D8::Release (inherit from IUnknown::Release)

	vtbl = ret->lpVtbl;

	if (!VirtualProtect(vtbl, sizeof(*vtbl), PAGE_READWRITE, &orig_protect))
	{
		LogWithErrorCode(GetLastError(), "%s: error: VirtualProtect (PAGE_READWRITE) failed.", __FUNCTION__);
		return NULL;
	}

	d3d8_exdata = (struct IDirect3D8ExtraData){
		.VanillaCreateDevice = vtbl->CreateDevice,
		.VanillaRelease = vtbl->Release
	};
	if (!IDirect3D8ExtraDataTableInsert(g_D3D8ExDataTable, ret, d3d8_exdata))
	{
		Log("%s: bug, error: IDirect3D8ExtraDataTableInsert failed.", __FUNCTION__);
		return NULL;
	}

	vtbl->CreateDevice = ModIDirect3D8CreateDevice;
	vtbl->Release = ModIDirect3D8Release;

	// best effort
	if (!VirtualProtect(vtbl, sizeof(*vtbl), orig_protect, &orig_protect))
		LogWithErrorCode(GetLastError(), "%s: warning: VirtualProtect (original protect) failed.", __FUNCTION__);

	return ret;
}

int AllocateDataDirPath(char** strp)
{
	char exepath[MAX_PATH + 1];
	char exedrivepath[_MAX_DRIVE + 1];
	char exedirpath[_MAX_DIR + 1];
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

	if (_splitpath_s(exepath, exedrivepath, sizeof(exedrivepath), exedirpath, sizeof(exedirpath), NULL, 0, NULL, 0) != 0)
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
	char* section_present = "presentation";
	char* key_wait_for = "wait_for";
	char* value_wait_for_vsync = "vsync";
	char* value_wait_for_timer60 = "timer60";
	char* value_wait_for_normal = "normal";
	char* value_wait_for_auto = "auto";

	enum ConfigWaitFor config_wait_for_default = CONFIG_WAITFOR_AUTO;
	char* value_wait_for_default = value_wait_for_auto;

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
			LogWithErrorCode(GetLastError(), "%s: error: WritePrivateProfileStringA (%s) failed.", __FUNCTION__, key_wait_for);
			goto cleanup;
		}

	g_ConfigFileWaitFor = config_wait_for_default;
	if (GetPrivateProfileStringA(section_present, key_wait_for, value_wait_for_default, buf, sizeof(buf), path) >= sizeof(buf) - 1)
		/* no op */;
	else if (strcmp(buf, value_wait_for_vsync) == 0)
		g_ConfigFileWaitFor = CONFIG_WAITFOR_VSYNC;
	else if (strcmp(buf, value_wait_for_timer60) == 0)
		g_ConfigFileWaitFor = CONFIG_WAITFOR_TIMER60;
	else if (strcmp(buf, value_wait_for_normal) == 0)
		g_ConfigFileWaitFor = CONFIG_WAITFOR_NORMAL;
	else if (strcmp(buf, value_wait_for_auto) == 0)
		g_ConfigFileWaitFor = CONFIG_WAITFOR_AUTO;
	else
		/* no op */;
	Log("%s: config file %s: %s", __FUNCTION__, key_wait_for, buf);

	ret = TRUE;
cleanup:
	free(path);
	return ret;
}

BOOL InitD3D8Handle(HMODULE* ret)
{
	char sysdirpath[MAX_PATH + 1];
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
		LogWithErrorCode(GetLastError(), "%s: error: LoadFuncFromD3D8 failed.", __FUNCTION__);
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
		LogWithErrorCode(GetLastError(), "%s: error: CreateFileA failed.", __FUNCTION__);
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
	// これ以降はログがファイルにも記録される

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

	// 実質的なエントリーポイントなので、最初は初期化処理を行う
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

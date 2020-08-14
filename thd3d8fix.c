#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define CINTERFACE
#include <d3d8.h>

#include <tchar.h>
#include <mmsystem.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

// Abbreviation:
//   Thf: thd3d8fix

// XXX TODO error handling
// XXX TODO 東方と同様にファイルにもロギング
// XXX TODO review
// XXX TODO fix data race and race condition
// XXX TODO コード整形

// utility

#define THF_LOG_PREFIX "[thd3d8fix]"

int myvasprintf(char** strp, const char* fmt, va_list ap)
{
	va_list ap2;
	int len;

	va_copy(ap2, ap);
	len = vsnprintf(NULL, 0, fmt, ap2);
	va_end(ap2);
	if (len < 0)
		return -1;

	if ((*strp = malloc(len + 1)) == NULL)
		return -1;

	if ((len = vsnprintf(*strp, len + 1, fmt, ap)) < 0)
	{
		free(*strp);
		return -1;
	}

	return len;
}

int myasprintf(char** strp, const char* fmt, ...)
{
	va_list ap;
	int len;

	va_start(ap, fmt);
	len = myvasprintf(strp, fmt, ap);
	va_end(ap);
	return len;
}

int AllocateErrorMessageA(DWORD code, char** strp)
{
	char* errmsg;
	int len;

	// XXX TODO review
	if (FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&errmsg, 0, NULL) == 0)
		return -1;
	len = myasprintf(strp, "%s", errmsg);
	LocalFree(errmsg);
	return len;
}

void ThfVLog(const char* fmt, va_list ap)
{
	char* origmsg = NULL, * msg = NULL;

	if (myvasprintf(&origmsg, fmt, ap) < 0)
	{
		OutputDebugStringA(THF_LOG_PREFIX __FUNCTION__ ": myvasprintf failed.\n");
		return;
	}

	if (myasprintf(&msg, "%s%s\n", THF_LOG_PREFIX, origmsg) < 0)
	{
		OutputDebugStringA(THF_LOG_PREFIX __FUNCTION__ ": myasprintf failed.\n");
		goto cleanup;
	}

	OutputDebugStringA(msg);

cleanup:
	free(msg);
	free(origmsg);
}

void ThfLog(const char* fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	ThfVLog(fmt, ap);
	va_end(ap);
}

void ThfVError(DWORD err, const char* fmt, va_list ap)
{
	char* origmsg = NULL, * msg = NULL, * errmsg = NULL;

	if (AllocateErrorMessageA(err, &errmsg) < 0)
	{
		OutputDebugStringA(THF_LOG_PREFIX __FUNCTION__ ": AllocateErrorMessage failed.\n");
		return;
	}

	if (myvasprintf(&origmsg, fmt, ap) < 0)
	{
		OutputDebugStringA(THF_LOG_PREFIX __FUNCTION__ ": myvasprintf failed.\n");
		goto cleanup;
	}

	if (myasprintf(&msg, "%s%s: %s\n", THF_LOG_PREFIX, origmsg, errmsg) < 0)
	{
		OutputDebugStringA(THF_LOG_PREFIX __FUNCTION__ ": myasprintf failed.\n");
		goto cleanup;
	}

	OutputDebugStringA(msg);

cleanup:
	free(msg);
	free(origmsg);
	free(errmsg);
}

void ThfError(DWORD err, const char* fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	ThfVError(err, fmt, ap);
	va_end(ap);
}

// hook COM method

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

static Type_IDirect3DDevice8_Present Vanilla_IDirect3DDevice8_Present = NULL;
static Type_IDirect3D8_CreateDevice Vanilla_IDirect3D8_CreateDevice = NULL;

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

	do {
		if (FAILED(me->lpVtbl->GetRasterStatus(me, &stat)))
		{
			ThfLog("%s: IDirect3DDevice8::GetRasterStatus failed.", __FUNCTION__);
			return E_FAIL;
		}
		Sleep(0); // XXX TODO SleepEx?
	} while (stat.InVBlank);

	ret = Vanilla_IDirect3DDevice8_Present(me, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);

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

HRESULT __stdcall Mod_IDirect3D8_CreateDevice(IDirect3D8* me, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice8** ppReturnedDeviceInterface)
{
	HRESULT ret = Vanilla_IDirect3D8_CreateDevice(me, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, ppReturnedDeviceInterface);

	// XXX TODO: retを見る
	if (*ppReturnedDeviceInterface != NULL)
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

		Vanilla_IDirect3DDevice8_Present = vtbl->Present;
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

// proxy dll

typedef IDirect3D8* (WINAPI* Type_Direct3DCreate8)(UINT);

Type_Direct3DCreate8 Vanilla_Direct3DCreate8 = NULL;
HMODULE d3d8handle = NULL;

BOOL Init_Direct3DCreate8(void)
{
	char sysdirpath[MAX_PATH + 1];
	char* sysdllpath;
	unsigned int len;
	DWORD err;

	if (Vanilla_Direct3DCreate8 != NULL)
	{
		return TRUE;
	}

	if ((len = GetSystemDirectoryA(sysdirpath, sizeof(sysdirpath))) == 0)
	{
		ThfError(GetLastError(), "%s: GetSystemDirectoryA failed.", __FUNCTION__);
		return FALSE;
	}

	if (len > sizeof(sysdirpath))  // XXX TODO review condition
	{
		ThfLog("%s: GetSystemDirectoryA failed.: Path too long.", __FUNCTION__); // XXX TODO error message
		return FALSE;
	}

	if (myasprintf(&sysdllpath, "%s\\d3d8.dll", sysdirpath) < 0)
	{
		ThfLog("%s: myasprintf failed.", __FUNCTION__);
		return FALSE;
	}

	d3d8handle = LoadLibraryA(sysdllpath);
	err = GetLastError();
	free(sysdllpath);
	if (d3d8handle == NULL)
	{
		ThfError(err, "%s: LoadLibraryA failed.", __FUNCTION__);
		return FALSE;
	}

	if ((Vanilla_Direct3DCreate8 = (Type_Direct3DCreate8)GetProcAddress(d3d8handle, "Direct3DCreate8")) == NULL)
	{
		ThfError(GetLastError(), "%s: LoadFuncFromD3D8 failed.", __FUNCTION__);
		return FALSE;
	}

	return TRUE;
}

IDirect3D8* WINAPI Mod_Direct3DCreate8(UINT SDKVersion)
{
	IDirect3D8* ret;

	if (!Init_Direct3DCreate8())
		return NULL;

	ret = Vanilla_Direct3DCreate8(SDKVersion);

	if (ret != NULL)
	{
		DWORD orig_protect;
		IDirect3D8Vtbl* vtbl = ret->lpVtbl;

		if (!VirtualProtect(&vtbl->CreateDevice, sizeof(vtbl->CreateDevice), PAGE_READWRITE, &orig_protect))
		{
			ThfError(GetLastError(), "%s: VirtualProtect failed.", __FUNCTION__);
			return NULL;
		}

		Vanilla_IDirect3D8_CreateDevice = vtbl->CreateDevice;
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

// main

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	BOOL ret = TRUE;

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		ThfLog("Attaching to the process: begin");
		ThfLog("Attaching to the process: succeeded");
		break;
	}
	case DLL_PROCESS_DETACH:
	{
		ThfLog("Detaching to the process: begin");
		ThfLog("Detaching to the process: succeeded");
		break;
	}
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		break;
	}

	return ret;
}

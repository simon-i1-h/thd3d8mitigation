#include "thd3d8mitigation.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <mmsystem.h>

#include <locale.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

HANDLE g_LogFile = NULL;

int myvasprintf_l(char** strp, const char* fmt, _locale_t loc, va_list ap)
{
	va_list ap2;
	int len;

	va_copy(ap2, ap);
	len = _vscprintf_l(fmt, loc, ap);
	va_end(ap2);
	if (len < 0)
		return -1;

	if ((*strp = malloc(len + 1)) == NULL)
		return -1;

	if ((len = _vsprintf_s_l(*strp, len + 1, fmt, loc, ap)) < 0)
	{
		free(*strp);
		return -1;
	}

	return len;
}

int myvasprintf(char** strp, const char* fmt, va_list ap)
{
	_locale_t loc;
	int ret;

	loc = _get_current_locale();
	ret = myvasprintf_l(strp, fmt, loc, ap);
	_free_locale(loc);

	return ret;
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

	if (FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, code, 0, (LPSTR)&errmsg, 0, NULL) == 0)
		return -1;
	len = myasprintf(strp, "%s", errmsg);
	LocalFree(errmsg);
	return len;
}

void VLog(const char* fmt, va_list ap)
{
	char *origmsg = NULL, *msg = NULL;
	DWORD tmp, now;

	if (myvasprintf(&origmsg, fmt, ap) < 0)
	{
		OutputDebugStringA(LOG_PREFIX __FUNCTION__ ": warning: myvasprintf failed.\r\n");
		goto cleanup;
	}

	timeBeginPeriod(1);
	now = timeGetTime();
	timeEndPeriod(1);

	if (myasprintf(&msg, "%s[System time: %010lu][Thread: %010lu]: %s\r\n", LOG_PREFIX, now, GetCurrentThreadId(),
			origmsg) < 0)
	{
		OutputDebugStringA(LOG_PREFIX __FUNCTION__ ": warning: myasprintf failed.\r\n");
		goto cleanup;
	}

	OutputDebugStringA(msg);
	EnterCriticalSection(&g_CS);
	if (g_LogFile != NULL)
		if (!WriteFile(g_LogFile, msg, strlen(msg), &tmp, NULL))
			OutputDebugStringA(LOG_PREFIX __FUNCTION__ ": warning: WriteFile failed.\r\n");
	LeaveCriticalSection(&g_CS);

cleanup:
	free(msg);
	free(origmsg);
}

void Log(const char* fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	VLog(fmt, ap);
	va_end(ap);
}

void VLogWithErrorCode(DWORD err, const char* fmt, va_list ap)
{
	char *origmsg = NULL, *msg = NULL, *errmsg = NULL;
	DWORD tmp, now;

	if (AllocateErrorMessageA(err, &errmsg) < 0)
	{
		OutputDebugStringA(LOG_PREFIX __FUNCTION__ ": warning: AllocateErrorMessage failed.\r\n");
		goto cleanup;
	}

	if (myvasprintf(&origmsg, fmt, ap) < 0)
	{
		OutputDebugStringA(LOG_PREFIX __FUNCTION__ ": warning: myvasprintf failed.\r\n");
		goto cleanup;
	}

	timeBeginPeriod(1);
	now = timeGetTime();
	timeEndPeriod(1);

	if (myasprintf(&msg, "%s[System time: %010lu][Thread: %010lu]: %s: error code: 0x%lx (%s)\r\n", LOG_PREFIX, now,
			GetCurrentThreadId(), origmsg, err, errmsg) < 0)
	{
		OutputDebugStringA(LOG_PREFIX __FUNCTION__ ": warning: myasprintf failed.\r\n");
		goto cleanup;
	}

	OutputDebugStringA(msg);
	EnterCriticalSection(&g_CS);
	if (g_LogFile != NULL)
		if (!WriteFile(g_LogFile, msg, strlen(msg), &tmp, NULL))
			OutputDebugStringA(LOG_PREFIX __FUNCTION__ ": warning: WriteFile failed.\r\n");
	LeaveCriticalSection(&g_CS);

cleanup:
	free(msg);
	free(origmsg);
	free(errmsg);
}

void LogWithErrorCode(DWORD err, const char* fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	VLogWithErrorCode(err, fmt, ap);
	va_end(ap);
}

__declspec(noreturn) void Fatal(int exitcode, const char* fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	VLog(fmt, ap);
	va_end(ap);
	ExitProcess(exitcode);
}

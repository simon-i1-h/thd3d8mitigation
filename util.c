#include "thd3d8mitigation.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <mmsystem.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

// XXX TODO Logをlog.cに分離するかも。その場合、LogInitもそちらの移す。

HANDLE g_LogFile = NULL;

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
	if (FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&errmsg, 0, NULL) == 0)
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
		OutputDebugStringA(LOG_PREFIX __FUNCTION__ ": warning: myvasprintf failed.\n");
		return;
	}

	timeBeginPeriod(1);
	now = timeGetTime();
	timeEndPeriod(1);

	if (myasprintf(&msg, "%s[System time: %010lu][Thread: %010lu]: %s\n", LOG_PREFIX, now, GetCurrentThreadId(),
			origmsg) < 0)
	{
		OutputDebugStringA(LOG_PREFIX __FUNCTION__ ": warning: myasprintf failed.\n");
		goto cleanup;
	}

	OutputDebugStringA(msg);
	EnterCriticalSection(&g_CS);
	if (g_LogFile != NULL)
		if (!WriteFile(g_LogFile, msg, strlen(msg), &tmp, NULL))
			OutputDebugStringA(LOG_PREFIX __FUNCTION__ ": warning: WriteFile failed.\n");
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
		OutputDebugStringA(LOG_PREFIX __FUNCTION__ ": warning: AllocateErrorMessage failed.\n");
		return;
	}

	if (myvasprintf(&origmsg, fmt, ap) < 0)
	{
		OutputDebugStringA(LOG_PREFIX __FUNCTION__ ": warning: myvasprintf failed.\n");
		goto cleanup;
	}

	timeBeginPeriod(1);
	now = timeGetTime();
	timeEndPeriod(1);

	if (myasprintf(&msg, "%s System time [%10lu]: Thread [%10lu]: %s: error code: 0x%lx (%s)\n", LOG_PREFIX, now,
			GetCurrentThreadId(), origmsg, err, errmsg) < 0)
	{
		OutputDebugStringA(LOG_PREFIX __FUNCTION__ ": warning: myasprintf failed.\n");
		goto cleanup;
	}

	OutputDebugStringA(msg);
	EnterCriticalSection(&g_CS);
	if (g_LogFile != NULL)
		if (!WriteFile(g_LogFile, msg, strlen(msg), &tmp, NULL))
			OutputDebugStringA(LOG_PREFIX __FUNCTION__ ": warning: WriteFile failed.\n");
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

// XXX TODO proper exit
__declspec(noreturn) void Fatal(int exitcode, const char* fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	VLog(fmt, ap);
	va_end(ap);
	ExitProcess(exitcode);
}
